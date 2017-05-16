//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2007-2016 musikcube team
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

#include <core/library/track/LibraryTrack.h>
#include <core/library/LocalLibraryConstants.h>
#include <core/db/Statement.h>

#include <boost/algorithm/string/case_conv.hpp>
#include <boost/algorithm/string.hpp>

#include <map>

using musik::core::db::Statement;
using musik::core::db::Row;
using musik::core::TrackPtr;
using musik::core::LibraryTrack;
using musik::core::ILibraryPtr;

using namespace musik::core::db;
using namespace musik::core::db::local;
using namespace musik::core::library::constants;
using namespace boost::algorithm;

static std::map<std::string, std::string> FIELD_TO_FOREIGN_KEY =
    {
        std::make_pair(Track::ALBUM, Track::ALBUM_ID),
        std::make_pair(Track::ARTIST, Track::ARTIST_ID),
        std::make_pair(Track::GENRE, Track::GENRE_ID),
        std::make_pair(Track::ALBUM_ARTIST, Track::ALBUM_ARTIST_ID)
    };

CategoryTrackListQuery::CategoryTrackListQuery(
    ILibraryPtr library,
    const std::string& column,
    int64_t id,
    const std::string& filter)
{
    this->library = library;
    this->id = id;
    this->result.reset(new musik::core::TrackList(library));
    this->headers.reset(new std::set<size_t>());
    this->hash = 0;

    if (filter.size()) {
        this->filter = "%" + trim_copy(to_lower_copy(filter)) + "%";
    }

    if (FIELD_TO_FOREIGN_KEY.find(column) == FIELD_TO_FOREIGN_KEY.end()) {
        throw std::runtime_error("invalid input column specified");
    }

    this->column = FIELD_TO_FOREIGN_KEY[column];
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
    if (this->hash == 0) {
        std::string parts = boost::str(
            boost::format("%s-%s") % this->column % this->id);

        this->hash = std::hash<std::string>()(parts);
    }

    return this->hash;
}

bool CategoryTrackListQuery::OnRun(Connection& db) {
    if (result) {
        result.reset(new musik::core::TrackList(this->library));
        headers.reset(new std::set<size_t>());
    }

    std::string lastAlbum;
    size_t index = 0;

    std::string query =
        "SELECT DISTINCT t.id, al.name "
        "FROM tracks t, albums al, artists ar, genres gn "
        "WHERE t.visible=1 AND t.%s=? AND t.album_id=al.id AND t.visual_genre_id=gn.id AND t.visual_artist_id=ar.id ";

    if (this->filter.size()) {
        query += " AND (t.title LIKE ? OR al.name LIKE ? OR ar.name LIKE ? OR gn.name LIKE ?) ";
    }

    query += "ORDER BY al.name, disc, track, ar.name %s";

    query = boost::str(boost::format(query) % this->column % this->GetLimitAndOffset());

    Statement trackQuery(query.c_str(), db);

    if (this->filter.size()) {
        trackQuery.BindInt64(0, this->id);
        trackQuery.BindText(1, this->filter);
        trackQuery.BindText(2, this->filter);
        trackQuery.BindText(3, this->filter);
        trackQuery.BindText(4, this->filter);
    }
    else {
        trackQuery.BindInt64(0, this->id);
    }

    while (trackQuery.Step() == Row) {
        int64_t id = trackQuery.ColumnInt64(0);
        std::string album = trackQuery.ColumnText(1);

        if (album != lastAlbum) {
            headers->insert(index);
            lastAlbum = album;
        }

        result->Add(id);
        ++index;
    }

    return true;
}
