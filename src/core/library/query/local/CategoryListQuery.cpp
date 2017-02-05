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
#include "CategoryListQuery.h"

#include <core/library/LocalLibraryConstants.h>
#include <core/db/Statement.h>

#include <mutex>
#include <map>

using musik::core::db::Statement;
using musik::core::db::Row;

using namespace musik::core::db;
using namespace musik::core::library::constants;
using namespace musik::core::db::local;

#define RESET_RESULT(x) x.reset(new std::vector<std::shared_ptr<Result> >);

static const std::string REGULAR_ALBUM_QUERY =
    "SELECT DISTINCT albums.id, albums.name "
    "FROM albums, tracks "
    "WHERE albums.id = tracks.album_id "
    "ORDER BY albums.sort_order;";

static const std::string FILTERED_ALBUM_QUERY =
    "SELECT DISTINCT albums.id, albums.name "
    "FROM albums, tracks "
    "WHERE albums.id = tracks.album_id AND LOWER(albums.name) LIKE ? "
    "ORDER BY albums.sort_order;";

static const std::string REGULAR_ARTIST_QUERY =
    "SELECT DISTINCT artists.id, artists.name "
    "FROM artists, tracks "
    "WHERE artists.id = tracks.visual_artist_id "
    "ORDER BY artists.sort_order;";

static const std::string FILTERED_ARTIST_QUERY =
    "SELECT DISTINCT artists.id, artists.name "
    "FROM artists, tracks "
    "WHERE artists.id = tracks.visual_artist_id AND LOWER(artists.name) LIKE ? "
    "ORDER BY artists.sort_order;";

static const std::string REGULAR_ALBUM_ARTIST_QUERY =
    "SELECT DISTINCT artists.id, artists.name "
    "FROM artists, tracks "
    "WHERE artists.id = tracks.album_artist_id "
    "ORDER BY artists.sort_order;";

static const std::string FILTERED_ALBUM_ARTIST_QUERY =
    "SELECT DISTINCT artists.id, artists.name "
    "FROM artists, tracks "
    "WHERE artists.id = tracks.album_artist_id AND LOWER(artists.name) LIKE ? "
    "ORDER BY artists.sort_order;";

static const std::string REGULAR_GENRE_QUERY =
    "SELECT DISTINCT genres.id, genres.name "
    "FROM genres, tracks "
    "WHERE genres.id = tracks.visual_genre_id "
    "ORDER BY genres.sort_order;";

static const std::string FILTERED_GENRE_QUERY =
    "SELECT DISTINCT genres.id, genres.name "
    "FROM genres, tracks "
    "WHERE genres.id = tracks.visual_genre_id AND LOWER(genres.name) LIKE ? "
    "ORDER BY genres.sort_order;";

static const std::string REGULAR_PLAYLISTS_QUERY =
    "SELECT DISTINCT id, name "
    "FROM playlists "
    "ORDER BY name;";

static const std::string FILTERED_PLAYLISTS_QUERY =
    "SELECT DISTINCT id, name "
    "FROM playlists "
    "WHERE LOWER(name) LIKE ? "
    "ORDER BY name;";

static std::mutex QUERY_MAP_MUTEX;
static std::map<std::string, std::string> FIELD_TO_QUERY_MAP;
static std::map<std::string, std::string> FILTERED_FIELD_TO_QUERY_MAP;

static void initFieldToQueryMap() {
    FIELD_TO_QUERY_MAP[Track::ALBUM] = REGULAR_ALBUM_QUERY;
    FIELD_TO_QUERY_MAP[Track::ARTIST] = REGULAR_ARTIST_QUERY;
    FIELD_TO_QUERY_MAP[Track::ALBUM_ARTIST] = REGULAR_ALBUM_ARTIST_QUERY;
    FIELD_TO_QUERY_MAP[Track::GENRE] = REGULAR_GENRE_QUERY;
    FIELD_TO_QUERY_MAP[Playlists::TABLE_NAME] = REGULAR_PLAYLISTS_QUERY;

    FILTERED_FIELD_TO_QUERY_MAP[Track::ALBUM] = FILTERED_ALBUM_QUERY;
    FILTERED_FIELD_TO_QUERY_MAP[Track::ARTIST] = FILTERED_ARTIST_QUERY;
    FILTERED_FIELD_TO_QUERY_MAP[Track::ALBUM_ARTIST] = FILTERED_ALBUM_ARTIST_QUERY;
    FILTERED_FIELD_TO_QUERY_MAP[Track::GENRE] = FILTERED_GENRE_QUERY;
    FILTERED_FIELD_TO_QUERY_MAP[Playlists::TABLE_NAME] = REGULAR_PLAYLISTS_QUERY;
}

/* data structure that we can return to plugins who need metadata info */
class MetadataList : public musik::core::sdk::IMetadataValueList {
    public:
        MetadataList(CategoryListQuery::ResultList results) {
            this->results = results;
        }

        virtual void Release() {
            delete this;
        }

        virtual size_t Count() {
            return this->results->size();
        }

        virtual musik::core::sdk::IMetadataValue* GetAt(size_t index) {
            return this->results->at(index).get();
        }

    private:
        CategoryListQuery::ResultList results;
};

CategoryListQuery::CategoryListQuery(
    const std::string& trackField, const std::string& filter)
: trackField(trackField)
, filter(filter) {
    RESET_RESULT(result);

    {
        std::unique_lock<std::mutex> lock(QUERY_MAP_MUTEX);

        if (!FIELD_TO_QUERY_MAP.size()) {
            initFieldToQueryMap();
        }
    }

    if (FIELD_TO_QUERY_MAP.find(trackField) == FIELD_TO_QUERY_MAP.end()) {
        throw "invalid field for CategoryListView specified";
    }
}

CategoryListQuery::~CategoryListQuery() {

}

CategoryListQuery::ResultList CategoryListQuery::GetResult() {
    return this->result;
}

musik::core::sdk::IMetadataValueList* CategoryListQuery::GetSdkResult() {
    return new MetadataList(this->result);
}

int CategoryListQuery::GetIndexOf(DBID id) {
    auto result = this->GetResult();
    for (size_t i = 0; i < result->size(); i++) {
        if (id == (*result)[i]->id) {
            return i;
        }
    }
    return -1;
}

bool CategoryListQuery::OnRun(Connection& db) {
    RESET_RESULT(result);

    bool filtered = this->filter.size() > 0;

    std::string query = filtered
        ? FILTERED_FIELD_TO_QUERY_MAP[this->trackField]
        : FIELD_TO_QUERY_MAP[this->trackField];

    Statement stmt(query.c_str(), db);

    if (filtered) {
        /* transform "FilteR" => "%filter%" */
        std::string wild = this->filter;
        std::transform(wild.begin(), wild.end(), wild.begin(), tolower);
        wild = "%" + wild + "%";
        stmt.BindText(0, wild);
    }

    while (stmt.Step() == Row) {
        std::shared_ptr<Result> row(new Result());
        row->id = stmt.ColumnInt64(0);
        row->displayValue = stmt.ColumnText(1);
        result->push_back(row);
    }

    return true;
}
