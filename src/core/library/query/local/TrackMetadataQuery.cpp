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
#include "TrackMetadataQuery.h"
#include <core/library/LocalLibraryConstants.h>

using namespace musik::core::db;
using namespace musik::core::db::local;
using namespace musik::core;
using namespace musik::core::library;

static const std::string COLUMNS = "t.track, t.disc, t.bpm, t.duration, t.filesize, t.title, t.filename, t.thumbnail_id, al.name AS album, alar.name AS album_artist, gn.name AS genre, ar.name AS artist, t.filetime, t.visual_genre_id, t.visual_artist_id, t.album_artist_id, t.album_id, t.source_id, t.external_id, t.rating ";
static const std::string TABLES = "tracks t, albums al, artists alar, artists ar, genres gn";
static const std::string PREDICATE = "t.album_id=al.id AND t.album_artist_id=alar.id AND t.visual_genre_id=gn.id AND t.visual_artist_id=ar.id";

static const std::string ALL_METADATA_QUERY_BY_ID =
    "SELECT DISTINCT " + COLUMNS + " " +
    "FROM " + TABLES + " " +
    "WHERE t.id=? AND " + PREDICATE;

static const std::string ALL_METADATA_QUERY_BY_EXTERNAL_ID =
    "SELECT DISTINCT " + COLUMNS + " " +
    "FROM " + TABLES + " " +
    "WHERE t.external_id=? AND " + PREDICATE;

static const std::string IDS_ONLY_QUERY_BY_ID =
    "SELECT DISTINCT external_id, source_id FROM tracks WHERE tracks.id=?";

static const std::string IDS_ONLY_QUERY_BY_EXTERNAL_ID =
    "SELECT DISTINCT external_id, source_id FROM tracks WHERE tracks.external_id=?";

TrackMetadataQuery::TrackMetadataQuery(TrackPtr target, ILibraryPtr library, Type type) {
    this->result = target;
    this->library = library;
    this->type = type;
}

bool TrackMetadataQuery::OnRun(Connection& db) {
    bool queryById = this->result->GetId() != 0;

    std::string query;

    if (this->type == Full) {
        query = queryById
            ? ALL_METADATA_QUERY_BY_ID
            : ALL_METADATA_QUERY_BY_EXTERNAL_ID;
    }
    else {
        query = queryById
            ? IDS_ONLY_QUERY_BY_ID
            : IDS_ONLY_QUERY_BY_EXTERNAL_ID;
    }

    Statement trackQuery(query.c_str(), db);

    if (queryById) {
        trackQuery.BindInt64(0, (int64_t) this->result->GetId());
    }
    else {
        const std::string& externalId = this->result->GetString("external_id");
        if (!externalId.size()) {
            return false;
        }

        trackQuery.BindText(0, externalId);
    }

    if (trackQuery.Step() == Row) {
        if (this->type == Full) {
            result->SetValue(constants::Track::TRACK_NUM, trackQuery.ColumnText(0));
            result->SetValue(constants::Track::DISC_NUM, trackQuery.ColumnText(1));
            result->SetValue(constants::Track::BPM, trackQuery.ColumnText(2));
            result->SetValue(constants::Track::DURATION, trackQuery.ColumnText(3));
            result->SetValue(constants::Track::FILESIZE, trackQuery.ColumnText(4));
            result->SetValue(constants::Track::TITLE, trackQuery.ColumnText(5));
            result->SetValue(constants::Track::FILENAME, trackQuery.ColumnText(6));
            result->SetValue(constants::Track::THUMBNAIL_ID, trackQuery.ColumnText(7));
            result->SetValue(constants::Track::ALBUM, trackQuery.ColumnText(8));
            result->SetValue(constants::Track::ALBUM_ARTIST, trackQuery.ColumnText(9));
            result->SetValue(constants::Track::GENRE, trackQuery.ColumnText(10));
            result->SetValue(constants::Track::ARTIST, trackQuery.ColumnText(11));
            result->SetValue(constants::Track::FILETIME, trackQuery.ColumnText(12));
            result->SetValue(constants::Track::GENRE_ID, trackQuery.ColumnText(13));
            result->SetValue(constants::Track::ARTIST_ID, trackQuery.ColumnText(14));
            result->SetValue(constants::Track::ALBUM_ARTIST_ID, trackQuery.ColumnText(15));
            result->SetValue(constants::Track::ALBUM_ID, trackQuery.ColumnText(16));
            result->SetValue(constants::Track::SOURCE_ID, trackQuery.ColumnText(17));
            result->SetValue(constants::Track::EXTERNAL_ID, trackQuery.ColumnText(18));
            result->SetValue(constants::Track::RATING, trackQuery.ColumnText(19));
        }
        else {
            result->SetValue(constants::Track::EXTERNAL_ID, trackQuery.ColumnText(0));
            result->SetValue(constants::Track::SOURCE_ID, trackQuery.ColumnText(1));
        }

        return true;
    }

    return false;
}