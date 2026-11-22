//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004-2023 musikcube team
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//    * Redistributions of source code must retain the above copyright notice,
//      this list of conditions and the following disclaimer.
//
//    * Redistributions in binary form must reproduce the above copyright
//      notice, this list of conditions and the following disclaimer in the
//      documentation and/or other materials provided with the distribution.
//
//    * Neither the name of the author nor the names of other contributors may
//      be used to endorse or promote products derived from this software
//      without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
//////////////////////////////////////////////////////////////////////////////

#include "pch.hpp"
#include "TrackMetadataBatchQuery.h"
#include <musikcore/library/LocalLibraryConstants.h>
#include <musikcore/library/query/util/Serialization.h>
#include <musikcore/library/track/LibraryTrack.h>
#include <musikcore/library/query/util/TrackQueryFragments.h>
#include <musikcore/sdk/String.h>

#include <musikcore/sdk/ReplayGain.h>
#include <algorithm>
#include <stdexcept>

#ifdef _MSC_VER
#pragma warning(push, 0)
#endif
#include <nlohmann/json.hpp>
#ifdef _MSC_VER
#pragma warning(pop)
#endif

using namespace musik::core;
using namespace musik::core::sdk;
using namespace musik::core::db;
using namespace musik::core::library;
using namespace musik::core::library::query;
using namespace musik::core::library::query::serialization;
using namespace musik::core::sdk;

const std::string TrackMetadataBatchQuery::kQueryName = "TrackMetadataBatchQuery";

TrackMetadataBatchQuery::TrackMetadataBatchQuery(std::unordered_set<int64_t> trackIds, ILibraryPtr library,
    std::vector<std::string> projection)
: library(std::move(library))
, trackIds(std::move(trackIds))
, projection(std::move(projection)) {
}

bool TrackMetadataBatchQuery::OnRun(Connection& db) {
    if (!this->projection.empty()) {
        /* Read selected fields directly. Going through full track objects loses
           the difference between a missing value and an empty string. */
        const std::vector<int64_t> ids(this->trackIds.begin(), this->trackIds.end());
        this->page.names = this->projection;
        if (!ids.empty() && !TrackPageQuery("", this->projection, 0, ids.size(), ids.data(), false)
            .Run(db.NativeHandle(), this->page)) return false;
        this->hasProjectedPage = true;
        return true;
    }
    if (this->trackIds.empty()) return true;
    std::string idList;
    for (size_t i = 0; i < this->trackIds.size(); ++i) {
        if (i != 0) {
            idList += ",";
        }
        idList += "?";
    }

    std::string query = tracks::kAllMetadataQueryByIdBatch;
    str::ReplaceAll(query, "{{ids}}", idList.c_str());

    Statement trackQuery(query.c_str(), db);
    int parameter = 0;
    for (const auto id : this->trackIds) trackQuery.BindInt64(parameter++, id);

    int status;
    while ((status = trackQuery.Step()) == Row) {
        auto id = trackQuery.ColumnInt64(0);
        auto track = std::make_shared<LibraryTrack>(id, this->library);
        tracks::ParseFullTrackMetadata(track, trackQuery);
        this->result[id] = track;
    }

    return status == Done;
}

/* ISerializableQuery */

std::string TrackMetadataBatchQuery::SerializeQuery() {
    nlohmann::json output = {
        { "name", kQueryName },
        { "options", {
            { "trackIds", this->trackIds }
        }}
    };
    /* Old servers ignore fields and return full tracks. */
    if (!this->projection.empty()) output["options"]["fields"] = this->projection;
    return output.dump();
}

std::string TrackMetadataBatchQuery::SerializeResult() {
    if (this->hasProjectedPage) {
        /* Value lists keep missing ([]) distinct from empty text ([""]). */
        nlohmann::json rows = nlohmann::json::array();
        for (const auto& row : this->page.rows) rows.push_back({{"id", row.id}, {"values", row.fields}});
        return nlohmann::json{{"projectedPage", {{"fields", this->page.names}, {"rows", rows}}}}.dump();
    }
    nlohmann::json idToTrackMap;
    for (auto& kv : this->result) {
        auto trackJson = TrackToJson(kv.second, false);;
        idToTrackMap[std::to_string(kv.first)] = trackJson;
    }
    nlohmann::json output = {
        { "result", idToTrackMap }
    };
    return output.dump();
}

void TrackMetadataBatchQuery::DeserializeResult(const std::string& data) {
    this->SetStatus(IQuery::Failed);
    const auto response = nlohmann::json::parse(data);
    if (response.contains("projectedPage")) {
        /* Reject replies for other fields or tracks. Sort once so the caller
           can look up each requested ID. */
        const auto& projected = response.at("projectedPage");
        ProjectedTrackPage page;
        page.names = projected.at("fields").get<std::vector<std::string>>();
        if (this->projection.empty() || page.names != this->projection ||
            projected.at("rows").size() > this->trackIds.size()) throw std::invalid_argument("Invalid projected fields");
        for (const auto& row : projected.at("rows")) {
            ProjectedTrackPage::Row value{row.at("id").get<int64_t>(),
                row.at("values").get<std::vector<std::vector<std::string>>>()};
            if (!this->trackIds.count(value.id) || value.fields.size() != this->projection.size()) {
                throw std::invalid_argument("Invalid projected row");
            }
            page.rows.push_back(std::move(value));
        }
        std::sort(page.rows.begin(), page.rows.end(), [](const auto& a, const auto& b) { return a.id < b.id; });
        for (size_t i = 1; i < page.rows.size(); ++i) {
            if (page.rows[i - 1].id == page.rows[i].id) throw std::invalid_argument("Duplicate projected row");
        }
        this->page = std::move(page);
        this->hasProjectedPage = true;
        this->SetStatus(IQuery::Finished);
        return;
    }
    /* Older servers ignore the optional projection and return their legacy map. */
    auto input = response["result"];
    for (const auto& kv : input.items()) {
        int64_t id = std::atoll(kv.key().c_str());
        auto track = std::make_shared<LibraryTrack>(id, this->library);
        TrackFromJson(kv.value(), track, false);
        this->result[id] = track;
    }
    this->SetStatus(IQuery::Finished);
}

std::shared_ptr<TrackMetadataBatchQuery> TrackMetadataBatchQuery::DeserializeQuery(
    musik::core::ILibraryPtr library, const std::string& data)
{
    using SetType = std::unordered_set <int64_t>;
    auto json = nlohmann::json::parse(data);
    SetType trackIds;
    JsonArrayToSet<SetType, int64_t>(json["options"]["trackIds"], trackIds);
    return std::make_shared<TrackMetadataBatchQuery>(trackIds, library,
        json["options"].value("fields", std::vector<std::string>{}));
}
