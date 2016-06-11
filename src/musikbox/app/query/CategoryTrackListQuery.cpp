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

#include "stdafx.h"
#include "CategoryTrackListQuery.h"

#include <core/library/track/LibraryTrack.h>
#include <core/library/LocalLibraryConstants.h>
#include <core/db/Statement.h>

#include <map>

using musik::core::db::Statement;
using musik::core::db::Row;
using musik::core::TrackPtr;
using musik::core::LibraryTrack;
using musik::core::LibraryPtr;

using namespace musik::core::db;
using namespace musik::core::library::constants;
using namespace musik::box;

static std::map<std::string, std::string> FIELD_TO_FOREIGN_KEY =
    {
        std::make_pair(Track::ALBUM, Track::ALBUM_ID),
        std::make_pair(Track::ARTIST, Track::ARTIST_ID),
        std::make_pair(Track::GENRE, Track::GENRE_ID),
        std::make_pair(Track::ALBUM_ARTIST, Track::ALBUM_ARTIST_ID)
    };

CategoryTrackListQuery::CategoryTrackListQuery(LibraryPtr library, const std::string& column, DBID id) {
    this->library = library;
    this->id = id;
    this->result.reset(new std::vector<TrackPtr>());
    this->headers.reset(new std::set<size_t>());
    this->hash = 0;

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
        result.reset(new std::vector<TrackPtr>());
        headers.reset(new std::set<size_t>());
    }

    this->query = boost::str(boost::format(
        "SELECT DISTINCT t.id, t.track, t.disc, t.bpm, t.duration, t.filesize, t.year, t.title, t.filename, t.thumbnail_id, al.name AS album, gn.name AS genre, ar.name AS artist, t.filetime " \
        "FROM tracks t, paths p, albums al, artists ar, genres gn " \
        "WHERE t.%s=? AND t.album_id=al.id AND t.visual_genre_id=gn.id AND t.visual_artist_id=ar.id "
        "ORDER BY album, disc, track, artist") % this->column);

    std::string lastAlbum;
    size_t index = 0;

    Statement trackQuery(this->query.c_str(), db);

    trackQuery.BindInt(0, this->id);
    while (trackQuery.Step() == Row) {
        std::string album = trackQuery.ColumnText(10);
        DBID id = trackQuery.ColumnInt64(0);

        if (album != lastAlbum) {
            headers->insert(index);
            lastAlbum = album;
        }

        TrackPtr track = TrackPtr(new LibraryTrack(id, this->library));
        track->SetValue(Track::TRACK_NUM, trackQuery.ColumnText(1));
        track->SetValue(Track::DISC_NUM, trackQuery.ColumnText(2));
        track->SetValue(Track::BPM, trackQuery.ColumnText(3));
        track->SetValue(Track::DURATION, trackQuery.ColumnText(4));
        track->SetValue(Track::FILESIZE, trackQuery.ColumnText(5));
        track->SetValue(Track::YEAR, trackQuery.ColumnText(6));
        track->SetValue(Track::TITLE, trackQuery.ColumnText(7));
        track->SetValue(Track::FILENAME, trackQuery.ColumnText(8));
        track->SetValue(Track::THUMBNAIL_ID, trackQuery.ColumnText(9));
        track->SetValue(Track::ALBUM, album.c_str());
        track->SetValue(Track::GENRE, trackQuery.ColumnText(11));
        track->SetValue(Track::ARTIST, trackQuery.ColumnText(12));
        track->SetValue(Track::FILETIME, trackQuery.ColumnText(13));

        result->push_back(track);
        ++index;
    }

    return true;
}
