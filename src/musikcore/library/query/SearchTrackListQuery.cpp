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
#include "SearchTrackListQuery.h"

#include <musikcore/i18n/Locale.h>
#include "TrackReadQueries.h"
#include <musikcore/library/query/util/Serialization.h>

#pragma warning(push, 0)
#include <nlohmann/json.hpp>
#pragma warning(pop)

using musik::core::ILibraryPtr;

using namespace musik::core;
using namespace musik::core::db;
using namespace musik::core::library::query;
using namespace musik::core::library::query::serialization;

const std::string SearchTrackListQuery::kQueryName = "SearchTrackListQuery";

SearchTrackListQuery::SearchTrackListQuery(
    ILibraryPtr library,
    MatchType matchType,
    const std::string& filter,
    TrackSortType sort)
{
    this->library = library;
    this->matchType = matchType;
    this->sortType = sort;
    this->filter = filter;

    this->result = std::make_shared<TrackList>(library);
    this->headers = std::make_shared<std::set<size_t>>();
    this->durations = std::make_shared<std::map<size_t, size_t>>();
    this->hash = 0;
}

SearchTrackListQuery::Result SearchTrackListQuery::GetResult() noexcept {
    return this->result;
}

SearchTrackListQuery::Headers SearchTrackListQuery::GetHeaders() noexcept {
    return this->headers;
}

SearchTrackListQuery::Durations SearchTrackListQuery::GetDurations() noexcept {
    return this->durations;
}

size_t SearchTrackListQuery::GetQueryHash() noexcept {
    this->hash = std::hash<std::string>()(this->filter);
    return this->hash;
}

bool SearchTrackListQuery::OnRun(Connection& db) {
    TrackSearchRows rows;
    const TrackSearchQuery query(this->filter, this->matchType == MatchType::Regex,
        this->sortType, this->GetLimit(), this->GetOffset(), _TSTR("tracklist_unknown_album"));
    if (!query.Run(db.NativeHandle(), rows)) return false;

    /* Adapt the shared search result to the existing serialized query contract. */
    this->result = std::make_shared<TrackList>(this->library, rows.ids.data(), rows.ids.size());
    this->headers = std::make_shared<std::set<size_t>>();
    this->durations = std::make_shared<std::map<size_t, size_t>>();
    for (const auto& album : rows.albumDurations) {
        this->headers->insert(album.first);
        this->durations->emplace(album.first, album.second);
    }
    return true;
}

/* ISerializableQuery */

std::string SearchTrackListQuery::SerializeQuery() {
    nlohmann::json output = {
        { "name", kQueryName },
        { "options", {
            { "filter", filter },
            { "matchType", matchType },
            { "sortType", sortType }
        }}
    };
    return FinalizeSerializedQueryWithLimitAndOffset(output);
}

std::string SearchTrackListQuery::SerializeResult() {
    return InitializeSerializedResultWithHeadersAndTrackList().dump();
}

void SearchTrackListQuery::DeserializeResult(const std::string& data) {
    this->SetStatus(IQuery::Failed);
    nlohmann::json result = nlohmann::json::parse(data)["result"];
    this->DeserializeTrackListAndHeaders(result, this->library, this);
    this->SetStatus(IQuery::Finished);
}

std::shared_ptr<SearchTrackListQuery> SearchTrackListQuery::DeserializeQuery(musik::core::ILibraryPtr library, const std::string& data) {
    auto options = nlohmann::json::parse(data)["options"];
    auto result = std::make_shared<SearchTrackListQuery>(
        library,
        options.value("matchType", MatchType::Substring),
        options["filter"].get<std::string>(),
        options["sortType"].get<TrackSortType>());
    result->ExtractLimitAndOffsetFromDeserializedQuery(options);
    return result;
}
