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
#include "AlbumListQuery.h"

#include <core/library/LocalLibraryConstants.h>
#include <core/db/Statement.h>

using musik::core::db::Statement;
using musik::core::db::Row;

using namespace musik::core;
using namespace musik::core::db;
using namespace musik::core::db::local;
using namespace musik::core::library::constants;
using namespace musik::core::sdk;

#define RESET_RESULT(x) x.reset(new MetadataMapList())

static const std::string COLUMNS =
    "albums.id, "
    "albums.name as album, "
    "tracks.album_artist_id, "
    "artists.name as album_artist, "
    "tracks.thumbnail_id ";

static const std::string TABLES =
    "albums, tracks, artists ";

static const std::string FILTER_PREDICATE =
    "LOWER(album) like ? AND ";

static const std::string GENERAL_PREDICATE =
    "albums.id = tracks.album_id AND "
    "artists.id = tracks.album_artist_id ";

static const std::string ORDER =
    "albums.name asc ";

AlbumListQuery::AlbumListQuery(const std::string& filter)
: filter(filter) {
    RESET_RESULT(result);
}

AlbumListQuery::~AlbumListQuery() {

}

MetadataMapListPtr AlbumListQuery::GetResult() {
    return this->result;
}

musik::core::sdk::IMetadataMapList* AlbumListQuery::GetSdkResult() {
    return this->result->GetSdkValue();
}

bool AlbumListQuery::OnRun(Connection& db) {
    RESET_RESULT(result);

    bool filtered = this->filter.size() > 0;

    std::string query = "SELECT " + COLUMNS + " FROM " + TABLES + " WHERE ";
    query += filtered ? FILTER_PREDICATE : "";
    query += GENERAL_PREDICATE + " ORDER BY " + ORDER + ";";

    Statement stmt(query.c_str(), db);

    if (filtered) {
        /* transform "FilteR" => "%filter%" */
        std::string wild = this->filter;
        std::transform(wild.begin(), wild.end(), wild.begin(), tolower);
        wild = "%" + wild + "%";
        stmt.BindText(0, wild);
    }

    while (stmt.Step() == Row) {
        std::shared_ptr<MetadataMap> row(new MetadataMap(
            (unsigned long long) stmt.ColumnInt64(0),
            stmt.ColumnText(1),
            "album"));

        row->SetValue(Track::ALBUM_ARTIST_ID, stmt.ColumnText(2));
        row->SetValue(Track::ALBUM_ARTIST, stmt.ColumnText(3));
        row->SetValue(Track::THUMBNAIL_ID, stmt.ColumnText(4));

        result->Add(row);
    }

    return true;
}
