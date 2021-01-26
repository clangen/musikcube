//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004-2020 musikcube team
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
#include <musikcore/sdk/ReplayGain.h>

#pragma warning(push, 0)
#include <nlohmann/json.hpp>
#pragma warning(pop)

using namespace musik::core;
using namespace musik::core::db;
using namespace musik::core::library;
using namespace musik::core::library::query;
using namespace musik::core::library::query::serialization;
using namespace musik::core::sdk;

const std::string TrackMetadataBatchQuery::kQueryName = "TrackMetadataBatchQuery";

TrackMetadataBatchQuery::TrackMetadataBatchQuery(std::unordered_set<int64_t> trackIds, ILibraryPtr library)
: trackIds(trackIds)
, library(library) {
}

bool TrackMetadataBatchQuery::OnRun(Connection& db) {
    std::string idList;
    size_t i = 0;
    for (const int64_t id : this->trackIds) {
        idList += std::to_string(id);
        if (i < this->trackIds.size() - 1) {
            idList += ",";
        }
        ++i;
    }

    std::string query = tracks::kAllMetadataQueryByIdBatch;
    ReplaceAll(query, "{{ids}}", idList);

    Statement trackQuery(query.c_str(), db);

    while (trackQuery.Step() == Row) {
        auto id = trackQuery.ColumnInt64(0);
        auto track = std::make_shared<LibraryTrack>(id, this->library);
        tracks::ParseFullTrackMetadata(track, trackQuery);
        this->result[id] = track;
    }

    return true;
}

/* ISerializableQuery */

std::string TrackMetadataBatchQuery::SerializeQuery() {
    nlohmann::json output = {
        { "name", kQueryName },
        { "options", {
            { "trackIds", this->trackIds }
        }}
    };
    return output.dump();
}

std::string TrackMetadataBatchQuery::SerializeResult() {
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
    auto input = nlohmann::json::parse(data)["result"];
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
    return std::make_shared<TrackMetadataBatchQuery>(trackIds, library);
}
