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

#include "CategoryTrackListQuery.h"
#include "GetPlaylistQuery.h"

#include <musikcore/library/track/LibraryTrack.h>
#include <musikcore/library/LocalLibraryConstants.h>
#include <musikcore/library/query/util/Serialization.h>
#include <musikcore/sdk/String.h>

using musik::core::db::Statement;
using musik::core::db::Row;
using musik::core::TrackPtr;
using musik::core::LibraryTrack;
using musik::core::ILibraryPtr;

using namespace musik::core::db;
using namespace musik::core::library::query;
using namespace musik::core::library::query::serialization;
using namespace musik::core::library::constants;

const std::string CategoryTrackListQuery::kQueryName = "CategoryTrackListQuery";

CategoryTrackListQuery::CategoryTrackListQuery(
    musik::core::ILibraryPtr library,
    const std::string& filter,
    TrackSortType sortType)
: CategoryTrackListQuery(library, category::PredicateList(), filter, sortType)
{
}

CategoryTrackListQuery::CategoryTrackListQuery(
    musik::core::ILibraryPtr library,
    const std::string& column,
    int64_t id,
    const std::string& filter,
    TrackSortType sortType)
: CategoryTrackListQuery(library, { column, id }, filter, sortType)
{
}

CategoryTrackListQuery::CategoryTrackListQuery(
    ILibraryPtr library,
    const category::Predicate predicate,
    const std::string& filter,
    TrackSortType sortType)
: CategoryTrackListQuery(library, category::PredicateList { predicate }, filter, sortType)
{
}

CategoryTrackListQuery::CategoryTrackListQuery(
    ILibraryPtr library,
    const category::PredicateList predicates,
    const std::string& filter,
    TrackSortType sortType)
{
    this->library = library;
    this->result = std::make_shared<TrackList>(library);
    this->headers = std::make_shared<std::set<size_t>>();
    this->durations = std::make_shared<std::map<size_t, size_t>>();
    this->hash = category::Hash(predicates);
    this->sortType = sortType;
    this->filter = filter;

    category::SplitPredicates(predicates, this->regular, this->extended);
    this->ScanPredicateListsForQueryType();

    this->orderBy = "ORDER BY " + kTrackListSortOrderBy.find(sortType)->second;
    this->parseHeaders = kTrackSortTypeWithAlbumGrouping.find(sortType) != kTrackSortTypeWithAlbumGrouping.end();
}

void CategoryTrackListQuery::ScanPredicateListsForQueryType() {
    if (this->extended.size() == 1 && this->extended.at(0).first == Playlists::TABLE_NAME) {
        this->type = Type::Playlist;
    }
    else {
        this->type = Type::Regular;
    }
}

CategoryTrackListQuery::Result CategoryTrackListQuery::GetResult() noexcept {
    return this->result;
}

CategoryTrackListQuery::Headers CategoryTrackListQuery::GetHeaders() noexcept {
    return this->headers;
}

size_t CategoryTrackListQuery::GetQueryHash() noexcept {
    return this->hash;
}

CategoryTrackListQuery::Durations CategoryTrackListQuery::GetDurations() noexcept {
    return this->durations;
}

void CategoryTrackListQuery::PlaylistQuery(musik::core::db::Connection &db) {
    /* playlists are a special case. we already have a query for this, so
    delegate to that. */
    GetPlaylistQuery query(this->library, this->extended.at(0).second);
    query.Run(db);
    this->result = query.GetResult();
}

void CategoryTrackListQuery::RegularQuery(musik::core::db::Connection &db) {
    category::ArgumentList args;

    /* order of operations with args is important! otherwise bind params
    will be out of order! */
    std::string query = category::CATEGORY_TRACKLIST_QUERY;
    std::string extended = InnerJoinExtended(this->extended, args);
    std::string regular = JoinRegular(this->regular, args, " AND ");
    std::string trackFilterClause, trackFilterValue;
    std::string limitAndOffset = this->GetLimitAndOffset();

    if (this->filter.size()) {
        trackFilterValue = "%" + sdk::str::Trim(sdk::str::ToLowerCopy(filter)) + "%";
        trackFilterClause = category::CATEGORY_TRACKLIST_FILTER;
        args.push_back(category::StringArgument(trackFilterValue));
        args.push_back(category::StringArgument(trackFilterValue));
        args.push_back(category::StringArgument(trackFilterValue));
        args.push_back(category::StringArgument(trackFilterValue));
    }

    category::ReplaceAll(query, "{{extended_predicates}}", extended);
    category::ReplaceAll(query, "{{regular_predicates}}", regular);
    category::ReplaceAll(query, "{{tracklist_filter}}", trackFilterClause);
    category::ReplaceAll(query, "{{order_by}}", this->orderBy);
    category::ReplaceAll(query, "{{limit_and_offset}}", limitAndOffset);

    Statement stmt(query.c_str(), db);
    category::Apply(stmt, args);
    this->ProcessResult(stmt);
}

void CategoryTrackListQuery::ProcessResult(musik::core::db::Statement& trackQuery) {
    std::string lastAlbum;
    size_t index = 0;
    size_t lastHeaderIndex = 0;
    size_t trackDuration = 0;
    size_t runningDuration = 0;

    while (trackQuery.Step() == Row) {
        const int64_t id = trackQuery.ColumnInt64(0);
        trackDuration = trackQuery.ColumnInt32(1);
        std::string album = trackQuery.ColumnText(2);

        if (this->parseHeaders && album != lastAlbum) {
            if (!headers->empty()) { /* @copypaste */
                (*durations)[lastHeaderIndex] = runningDuration;
                lastHeaderIndex = index;
                runningDuration = 0;
            }

            headers->insert(index);
            lastAlbum = album;
        }

        runningDuration += trackDuration;

        result->Add(id);
        ++index;
    }

    if (this->parseHeaders && !headers->empty()) {
        (*durations)[lastHeaderIndex] = runningDuration;
    }
}

bool CategoryTrackListQuery::OnRun(Connection& db) {
    if (result) {
        result = std::make_shared<TrackList>(library);
        headers = std::make_shared<std::set<size_t>>();
    }

    switch (this->type) {
        case Type::Playlist: this->PlaylistQuery(db); break;
        case Type::Regular: this->RegularQuery(db); break;
    }

    return true;
}

/* ISerializableQuery */

std::string CategoryTrackListQuery::SerializeQuery() {
    nlohmann::json output = {
        { "name", kQueryName },
        { "options", {
            { "filter", filter },
            { "regularPredicateList", PredicateListToJson(regular) },
            { "extendedPredicateList", PredicateListToJson(extended) },
            { "sortType", sortType }
        }}
    };
    return FinalizeSerializedQueryWithLimitAndOffset(output);
}

std::string CategoryTrackListQuery::SerializeResult() {
    return InitializeSerializedResultWithHeadersAndTrackList().dump();
}

void CategoryTrackListQuery::DeserializeResult(const std::string& data) {
    this->SetStatus(IQuery::Failed);
    nlohmann::json result = nlohmann::json::parse(data)["result"];
    this->DeserializeTrackListAndHeaders(result, this->library, this);
    this->SetStatus(IQuery::Finished);
}

std::shared_ptr<CategoryTrackListQuery> CategoryTrackListQuery::DeserializeQuery(
    musik::core::ILibraryPtr library, const std::string& data)
{
    nlohmann::json options = nlohmann::json::parse(data)["options"];
    auto result = std::make_shared<CategoryTrackListQuery>(
        library,
        options["filter"].get<std::string>(),
        options["sortType"].get<TrackSortType>());
    result->ExtractLimitAndOffsetFromDeserializedQuery(options);
    PredicateListFromJson(options["regularPredicateList"], result->regular);
    PredicateListFromJson(options["extendedPredicateList"], result->extended);
    result->ScanPredicateListsForQueryType();
    return result;
}