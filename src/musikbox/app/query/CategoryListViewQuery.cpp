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
#include "CategoryListViewQuery.h"

#include <core/library/LocalLibraryConstants.h>
#include <core/db/Statement.h>

#include <boost/thread/mutex.hpp>

#include <map>

using musik::core::db::Statement;
using musik::core::db::Row;

using namespace musik::core::db;
using namespace musik::core::library::constants;
using namespace musik::box;

#define RESET_RESULT(x) x.reset(new std::vector<std::shared_ptr<Result> >);

static const std::string ALBUM_QUERY =
    "SELECT DISTINCT albums.id, albums.name "
    "FROM albums, tracks "
    "WHERE albums.id = tracks.album_id "
    "ORDER BY albums.sort_order;";

static const std::string ARTIST_QUERY =
    "SELECT DISTINCT artists.id, artists.name "
    "FROM artists, tracks "
    "WHERE artists.id = tracks.visual_artist_id "
    "ORDER BY artists.sort_order;";

static const std::string GENRE_QUERY =
    "SELECT DISTINCT genres.id, genres.name "
    "FROM genres, tracks "
    "WHERE genres.id = tracks.visual_genre_id "
    "ORDER BY genres.sort_order;";

static boost::mutex QUERY_MAP_MUTEX;
static std::map<std::string, std::string> FIELD_TO_QUERY_MAP;

static void initFieldToQueryMap() {
    FIELD_TO_QUERY_MAP[Track::ALBUM] = ALBUM_QUERY;
    FIELD_TO_QUERY_MAP[Track::ARTIST] = ARTIST_QUERY;
    FIELD_TO_QUERY_MAP[Track::GENRE] = GENRE_QUERY;
}

CategoryListViewQuery::CategoryListViewQuery(const std::string& trackField) {
    this->trackField = trackField;

    RESET_RESULT(result);

    {
        boost::mutex::scoped_lock lock(QUERY_MAP_MUTEX);

        if (!FIELD_TO_QUERY_MAP.size()) {
            initFieldToQueryMap();
        }
    }

    if (FIELD_TO_QUERY_MAP.find(trackField) == FIELD_TO_QUERY_MAP.end()) {
        throw "invalid field for CategoryListView specified";
    }
}

CategoryListViewQuery::~CategoryListViewQuery() {

}

CategoryListViewQuery::ResultList CategoryListViewQuery::GetResult() {
    return this->result;
}

bool CategoryListViewQuery::OnRun(Connection& db) {
    RESET_RESULT(result);

    std::string query = FIELD_TO_QUERY_MAP[this->trackField];
    Statement stmt(query.c_str(), db);

    while (stmt.Step() == Row) {
        std::shared_ptr<Result> row(new Result());
        row->id = stmt.ColumnInt64(0);
        row->displayValue = stmt.ColumnText(1);
        result->push_back(row);
    }

    return true;
}
