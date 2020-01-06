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
#include "SearchTrackListQuery.h"

#include <core/i18n/Locale.h>
#include <core/library/track/LibraryTrack.h>
#include <core/library/LocalLibraryConstants.h>
#include <core/db/Statement.h>

#include <boost/algorithm/string/case_conv.hpp>
#include <boost/algorithm/string.hpp>

#include <iostream>

using musik::core::db::Statement;
using musik::core::db::Row;
using musik::core::TrackPtr;
using musik::core::LibraryTrack;
using musik::core::ILibraryPtr;

using namespace musik::core::db;
using namespace musik::core::library::constants;
using namespace musik::core::db::local;
using namespace boost::algorithm;

static const std::map<TrackSortType, std::string> kAdditionalPredicate {
    { TrackSortType::LastPlayedAsc, "t.last_played IS NOT NULL" },
    { TrackSortType::LastPlayedDesc, "t.last_played IS NOT NULL" },
    { TrackSortType::RatingAsc, "t.rating IS NOT NULL AND t.rating > 0" },
    { TrackSortType::RatingDesc, "t.rating IS NOT NULL AND t.rating > 0" },
    { TrackSortType::PlayCountAsc, "t.play_count IS NOT NULL AND t.play_count > 0" },
    { TrackSortType::PlayCountDesc, "t.play_count IS NOT NULL AND t.play_count > 0" },
};

static const std::map<TrackSortType, std::string> kOrderBy = {
    { TrackSortType::Title, "track, ar.name, al.name" },
    { TrackSortType::Album, "al.name, disc, track, ar.name" },
    { TrackSortType::Artist, "ar.name, al.name, disc, track" },
    { TrackSortType::DateAddedAsc, "date(t.date_added) ASC, al.name, disc, track, ar.name" },
    { TrackSortType::DateAddedDesc, "date(t.date_added) DESC, al.name, disc, track, ar.name" },
    { TrackSortType::DateUpdatedAsc, "date(t.date_updated) ASC, al.name, disc, track, ar.name" },
    { TrackSortType::DateUpdatedDesc, "date(t.date_updated) DESC, al.name, disc, track, ar.name" },
    { TrackSortType::LastPlayedAsc, "datetime(t.last_played) ASC" },
    { TrackSortType::LastPlayedDesc, "datetime(t.last_played) DESC" },
    { TrackSortType::RatingAsc, "t.rating ASC" },
    { TrackSortType::RatingDesc, "t.rating DESC" },
    { TrackSortType::PlayCountAsc, "t.play_count ASC" },
    { TrackSortType::PlayCountDesc, "t.play_count DESC" },
    { TrackSortType::Genre, "gn.name, al.name, disc, track, ar.name" },
};

SearchTrackListQuery::SearchTrackListQuery(
    ILibraryPtr library, const std::string& filter, TrackSortType sort)
{
    this->library = library;

    if (filter.size()) {
        this->filter = "%" + trim_copy(to_lower_copy(filter)) + "%";
    }

    if (kAdditionalPredicate.find(sort) != kAdditionalPredicate.end()) {
        this->additionalPredicate = kAdditionalPredicate.find(sort)->second + " AND ";
    }

    this->displayString = _TSTR(kTrackSortTypeToDisplayKey.find(sort)->second);
    this->orderBy = kOrderBy.find(sort)->second;
    this->result.reset(new musik::core::TrackList(library));
    this->headers.reset(new std::set<size_t>());
    this->hash = 0;
}

SearchTrackListQuery::~SearchTrackListQuery() {
}

SearchTrackListQuery::Result SearchTrackListQuery::GetResult() {
    return this->result;
}

SearchTrackListQuery::Headers SearchTrackListQuery::GetHeaders() {
    return this->headers;
}

size_t SearchTrackListQuery::GetQueryHash() {
    this->hash = std::hash<std::string>()(this->filter);
    return this->hash;
}

std::string SearchTrackListQuery::GetSortDisplayString() {
    return this->displayString;
}

bool SearchTrackListQuery::OnRun(Connection& db) {
    if (result) {
        result.reset(new musik::core::TrackList(this->library));
        headers.reset(new std::set<size_t>());
    }

    bool hasFilter = (this->filter.size() > 0);

    std::string lastAlbum;
    size_t index = 0;

    std::string query;

    if (hasFilter) {
        query =
            "SELECT DISTINCT t.id, al.name "
            "FROM tracks t, albums al, artists ar, genres gn "
            "WHERE "
                " t.visible=1 AND "
                + this->additionalPredicate +
                "(t.title LIKE ? OR al.name LIKE ? OR ar.name LIKE ? OR gn.name LIKE ?) "
                " AND t.album_id=al.id AND t.visual_genre_id=gn.id AND t.visual_artist_id=ar.id "
            "ORDER BY " + this->orderBy + " ";
    }
    else {
        query =
            "SELECT DISTINCT t.id, al.name "
            "FROM tracks t, albums al, artists ar, genres gn "
            "WHERE "
                " t.visible=1 AND "
                + this->additionalPredicate +
                " t.album_id=al.id AND t.visual_genre_id=gn.id AND t.visual_artist_id=ar.id "
            "ORDER BY " + this->orderBy + " ";
    }

    query += this->GetLimitAndOffset();

    std::cerr << query << "\n";

    Statement trackQuery(query.c_str(), db);

    if (hasFilter) {
        trackQuery.BindText(0, this->filter);
        trackQuery.BindText(1, this->filter);
        trackQuery.BindText(2, this->filter);
        trackQuery.BindText(3, this->filter);
    }

    while (trackQuery.Step() == Row) {
        int64_t id = trackQuery.ColumnInt64(0);
        std::string album = trackQuery.ColumnText(1);

        if (!album.size()) {
            album = _TSTR("tracklist_unknown_album");
        }

        if (album != lastAlbum) {
            headers->insert(index);
            lastAlbum = album;
        }

        result->Add(id);
        ++index;
    }

    return true;
}
