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
#include "SearchTrackListQuery.h"

#include <musikcore/i18n/Locale.h>
#include <musikcore/library/track/LibraryTrack.h>
#include <musikcore/library/query/util/Serialization.h>
#include <musikcore/library/LocalLibraryConstants.h>
#include <musikcore/db/Statement.h>

#pragma warning(push, 0)
#include <nlohmann/json.hpp>

#include <boost/algorithm/string/case_conv.hpp>
#include <boost/algorithm/string.hpp>
#pragma warning(pop)

using musik::core::db::Statement;
using musik::core::db::Row;
using musik::core::TrackPtr;
using musik::core::LibraryTrack;
using musik::core::ILibraryPtr;

using namespace musik::core::db;
using namespace musik::core::library::constants;
using namespace musik::core::library::query;
using namespace musik::core::library::query::serialization;
using namespace boost::algorithm;

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

    if (kTrackSearchSortOrderByPredicate.find(sort) != kTrackSearchSortOrderByPredicate.end()) {
        this->orderByPredicate = kTrackSearchSortOrderByPredicate.find(sort)->second + " AND ";
    }

    this->parseHeaders = kTrackSortTypeWithAlbumGrouping.find(sort) != kTrackSortTypeWithAlbumGrouping.end();
    this->displayString = _TSTR(kTrackListOrderByToDisplayKey.find(sort)->second);
    this->orderBy = kTrackListSortOrderBy.find(sort)->second;
    this->result = std::make_shared<TrackList>(library);
    this->headers = std::make_shared<std::set<size_t>>();
    this->hash = 0;
}

SearchTrackListQuery::Result SearchTrackListQuery::GetResult() noexcept {
    return this->result;
}

SearchTrackListQuery::Headers SearchTrackListQuery::GetHeaders() noexcept {
    return this->headers;
}

size_t SearchTrackListQuery::GetQueryHash() noexcept {
    this->hash = std::hash<std::string>()(this->filter);
    return this->hash;
}

std::string SearchTrackListQuery::GetSortDisplayString() {
    return this->displayString;
}

bool SearchTrackListQuery::OnRun(Connection& db) {
    if (result) {
        this->result = std::make_shared<TrackList>(library);
        this->headers = std::make_shared<std::set<size_t>>();
    }

    const bool useRegex = (matchType == MatchType::Regex);
    const bool hasFilter = (this->filter.size() > 0);
    std::string lastAlbum;
    size_t index = 0;
    std::string query;

    if (hasFilter) {
        query =
            "SELECT DISTINCT tracks.id, al.name "
            "FROM tracks, albums al, artists ar, genres gn "
            "WHERE "
                " tracks.visible=1 AND "
                + this->orderByPredicate +
                "(tracks.title {{match_type}} ? OR al.name {{match_type}} ? OR ar.name {{match_type}} ? OR gn.name {{match_type}} ?) "
                " AND tracks.album_id=al.id AND tracks.visual_genre_id=gn.id AND tracks.visual_artist_id=ar.id "
            "ORDER BY " + this->orderBy + " ";

        ReplaceAll(query, "{{match_type}}", useRegex ? "REGEXP" : "LIKE");
    }
    else {
        query =
            "SELECT DISTINCT tracks.id, al.name "
            "FROM tracks, albums al, artists ar, genres gn "
            "WHERE "
                " tracks.visible=1 AND "
                + this->orderByPredicate +
                " tracks.album_id=al.id AND tracks.visual_genre_id=gn.id AND tracks.visual_artist_id=ar.id "
            "ORDER BY " + this->orderBy + " ";
    }

    query += this->GetLimitAndOffset();

    Statement trackQuery(query.c_str(), db);

    if (hasFilter) {
        std::string patternToMatch = useRegex
            ? filter :  "%" + trim_copy(to_lower_copy(filter)) + "%";

        trackQuery.BindText(0, patternToMatch);
        trackQuery.BindText(1, patternToMatch);
        trackQuery.BindText(2, patternToMatch);
        trackQuery.BindText(3, patternToMatch);
    }

    while (trackQuery.Step() == Row) {
        const int64_t id = trackQuery.ColumnInt64(0);
        std::string album = trackQuery.ColumnText(1);

        if (!album.size()) {
            album = _TSTR("tracklist_unknown_album");
        }

        if (this->parseHeaders && album != lastAlbum) {
            headers->insert(index);
            lastAlbum = album;
        }

        result->Add(id);
        ++index;
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
    this->DeserializeTrackListAndHeaders(result, this->library, this->result, this->headers);
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
