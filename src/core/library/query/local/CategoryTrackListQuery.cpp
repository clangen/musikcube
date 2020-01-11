//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004-2019 musikcube team
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

#include <core/library/track/LibraryTrack.h>
#include <core/library/LocalLibraryConstants.h>

#include <boost/algorithm/string/case_conv.hpp>
#include <boost/algorithm/string.hpp>

using musik::core::db::Statement;
using musik::core::db::Row;
using musik::core::TrackPtr;
using musik::core::LibraryTrack;
using musik::core::ILibraryPtr;

using namespace musik::core::db;
using namespace musik::core::db::local;
using namespace musik::core::library::constants;
using namespace boost::algorithm;

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
    this->result.reset(new musik::core::TrackList(library));
    this->headers.reset(new std::set<size_t>());
    this->hash = category::Hash(predicates);

    category::SplitPredicates(predicates, this->regular, this->extended);

    if (filter.size()) {
        this->filter = "%" + trim_copy(to_lower_copy(filter)) + "%";
    }

    if (predicates.size() == 1 && predicates[0].first == Playlists::TABLE_NAME) {
        this->type = Playlist;
    }
    else {
        this->type = Regular;
    }

    this->orderBy = "ORDER BY " + kTrackListSortOrderBy.find(sortType)->second;
    this->parseHeaders = kTrackSortTypeWithAlbumGrouping.find(sortType) != kTrackSortTypeWithAlbumGrouping.end();
}

CategoryTrackListQuery::~CategoryTrackListQuery() {

}

CategoryTrackListQuery::Result CategoryTrackListQuery::GetResult() {
    return this->result;
}

CategoryTrackListQuery::Headers CategoryTrackListQuery::GetHeaders() {
    return this->headers;
}

size_t CategoryTrackListQuery::GetQueryHash() {
    return this->hash;
}

void CategoryTrackListQuery::PlaylistQuery(musik::core::db::Connection &db) {
    /* playlists are a special case. we already have a query for this, so
    delegate to that. */
    GetPlaylistQuery query(this->library, this->extended[0].second);
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
    std::string trackFilter;
    std::string limitAndOffset = this->GetLimitAndOffset();

    if (this->filter.size()) {
        trackFilter = category::CATEGORY_TRACKLIST_FILTER;
        args.push_back(category::StringArgument(this->filter));
        args.push_back(category::StringArgument(this->filter));
        args.push_back(category::StringArgument(this->filter));
        args.push_back(category::StringArgument(this->filter));
    }

    category::ReplaceAll(query, "{{extended_predicates}}", extended);
    category::ReplaceAll(query, "{{regular_predicates}}", regular);
    category::ReplaceAll(query, "{{tracklist_filter}}", trackFilter);
    category::ReplaceAll(query, "{{order_by}}", this->orderBy);
    category::ReplaceAll(query, "{{limit_and_offset}}", limitAndOffset);

    Statement stmt(query.c_str(), db);
    category::Apply(stmt, args);
    this->ProcessResult(stmt);
}

void CategoryTrackListQuery::ProcessResult(musik::core::db::Statement& trackQuery) {
    std::string lastAlbum;
    size_t index = 0;

    while (trackQuery.Step() == Row) {
        int64_t id = trackQuery.ColumnInt64(0);
        std::string album = trackQuery.ColumnText(1);

        if (this->parseHeaders && album != lastAlbum) {
            headers->insert(index);
            lastAlbum = album;
        }

        result->Add(id);
        ++index;
    }
}

bool CategoryTrackListQuery::OnRun(Connection& db) {
    if (result) {
        result.reset(new musik::core::TrackList(this->library));
        headers.reset(new std::set<size_t>());
    }

    switch (this->type) {
        case Playlist: this->PlaylistQuery(db); break;
        case Regular: this->RegularQuery(db); break;
    }

    return true;
}
