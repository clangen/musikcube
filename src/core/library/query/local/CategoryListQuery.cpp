//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2007-2017 musikcube team
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
    "WHERE albums.id = tracks.album_id AND tracks.visible = 1 %s"
    "ORDER BY albums.sort_order;";

static const std::string FILTERED_ALBUM_QUERY =
    "SELECT DISTINCT albums.id, albums.name "
    "FROM albums, tracks "
    "WHERE albums.id = tracks.album_id AND LOWER(albums.name) LIKE ? AND tracks.visible = 1 %s"
    "ORDER BY albums.sort_order;";

static const std::string REGULAR_ARTIST_QUERY =
    "SELECT DISTINCT artists.id, artists.name "
    "FROM artists, tracks "
    "WHERE artists.id = tracks.visual_artist_id AND tracks.visible = 1 %s"
    "ORDER BY artists.sort_order;";

static const std::string FILTERED_ARTIST_QUERY =
    "SELECT DISTINCT artists.id, artists.name "
    "FROM artists, tracks "
    "WHERE artists.id = tracks.visual_artist_id AND LOWER(artists.name) LIKE ? AND tracks.visible = 1 %s"
    "ORDER BY artists.sort_order;";

static const std::string REGULAR_ALBUM_ARTIST_QUERY =
    "SELECT DISTINCT artists.id, artists.name "
    "FROM artists, tracks "
    "WHERE artists.id = tracks.album_artist_id AND tracks.visible = 1 %s"
    "ORDER BY artists.sort_order;";

static const std::string FILTERED_ALBUM_ARTIST_QUERY =
    "SELECT DISTINCT artists.id, artists.name "
    "FROM artists, tracks "
    "WHERE artists.id = tracks.album_artist_id AND LOWER(artists.name) LIKE ? AND tracks.visible = 1 %s"
    "ORDER BY artists.sort_order;";

static const std::string REGULAR_GENRE_QUERY =
    "SELECT DISTINCT genres.id, genres.name "
    "FROM genres, tracks "
    "WHERE genres.id = tracks.visual_genre_id AND tracks.visible = 1 %s"
    "ORDER BY genres.sort_order;";

static const std::string FILTERED_GENRE_QUERY =
    "SELECT DISTINCT genres.id, genres.name "
    "FROM genres, tracks "
    "WHERE genres.id = tracks.visual_genre_id AND LOWER(genres.name) LIKE ? AND tracks.visible = 1 %s"
    "ORDER BY genres.sort_order;";

static const std::string REGULAR_PLAYLISTS_QUERY =
    "SELECT DISTINCT id, name "
    "FROM playlists %s "
    "ORDER BY name;";

static const std::string FILTERED_PLAYLISTS_QUERY =
    "SELECT DISTINCT id, name "
    "FROM playlists"
    "WHERE LOWER(name) LIKE ? %s "
    "ORDER BY name;";

static const std::string QUERY_PREDICATE = " AND %s=? ";

static std::map<std::string, std::string> FIELD_TO_QUERY_MAP = {
    { Track::ALBUM, REGULAR_ALBUM_QUERY },
    { Track::ARTIST, REGULAR_ARTIST_QUERY },
    { Track::ALBUM_ARTIST, REGULAR_ALBUM_ARTIST_QUERY },
    { Track::GENRE,  REGULAR_GENRE_QUERY },
    { Playlists::TABLE_NAME, REGULAR_PLAYLISTS_QUERY }
};

static std::map<std::string, std::string> FILTERED_FIELD_TO_QUERY_MAP = {
    { Track::ALBUM, FILTERED_ALBUM_QUERY },
    { Track::ARTIST, FILTERED_ARTIST_QUERY },
    { Track::ALBUM_ARTIST, FILTERED_ALBUM_ARTIST_QUERY },
    { Track::GENRE, FILTERED_GENRE_QUERY },
    { Playlists::TABLE_NAME, REGULAR_PLAYLISTS_QUERY }
};

static std::map<std::string, std::string> PREDICATE_TO_COLUMN_MAP = {
    { Track::ALBUM, "album_id" },
    { Track::ARTIST, "visual_artist_id" },
    { Track::ALBUM_ARTIST, "album_artist_id" },
    { Track::GENRE, "visual_genre_id" },
};

/* data structure that we can return to plugins who need metadata info */
class ValueList : public musik::core::sdk::IValueList {
    public:
        ValueList(CategoryListQuery::ResultList results) {
            this->results = results;
        }

        virtual void Release() {
            delete this;
        }

        virtual size_t Count() {
            return this->results->size();
        }

        virtual musik::core::sdk::IValue* GetAt(size_t index) {
            return this->results->at(index).get();
        }

    private:
        CategoryListQuery::ResultList results;
};

CategoryListQuery::CategoryListQuery(
    const std::string& trackField, const std::string& filter)
: CategoryListQuery(trackField, "", -1LL, filter) {
}

CategoryListQuery::CategoryListQuery(
    const std::string& trackField,
    const std::string& predicateField,
    int64_t predicateFieldId,
    const std::string& filter)
: trackField(trackField)
, predicateField(predicateField)
, predicateFieldId(predicateFieldId)
, filter(filter) {
    RESET_RESULT(result);

    if (FIELD_TO_QUERY_MAP.find(trackField) == FIELD_TO_QUERY_MAP.end()) {
        throw "invalid field for CategoryListView specified";
    }
}

CategoryListQuery::~CategoryListQuery() {

}

CategoryListQuery::ResultList CategoryListQuery::GetResult() {
    return this->result;
}

musik::core::sdk::IValueList* CategoryListQuery::GetSdkResult() {
    return new ValueList(this->result);
}

int CategoryListQuery::GetIndexOf(int64_t id) {
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

    bool predicated =
        predicateField.size() &&
        predicateField != Playlists::TABLE_NAME &&
        predicateFieldId > 0;

    std::string query = filtered
        ? FILTERED_FIELD_TO_QUERY_MAP[this->trackField]
        : FIELD_TO_QUERY_MAP[this->trackField];

    std::string predicate = "";

    if (predicated) {
        auto end = PREDICATE_TO_COLUMN_MAP.end();
        predicated = PREDICATE_TO_COLUMN_MAP.find(predicateField) != end;
        if (predicated) {
            predicate = boost::str(boost::format(
                QUERY_PREDICATE) % PREDICATE_TO_COLUMN_MAP[predicateField]);
        }
    }

    query = boost::str(boost::format(query) % predicate);

    int bindIndex = 0;
    Statement stmt(query.c_str(), db);

    if (filtered) {
        /* transform "FilteR" => "%filter%" */
        std::string wild = this->filter;
        std::transform(wild.begin(), wild.end(), wild.begin(), tolower);
        wild = "%" + wild + "%";
        stmt.BindText(bindIndex++, wild);
    }

    if (predicated) {
        stmt.BindInt64(bindIndex++, predicateFieldId);
    }

    while (stmt.Step() == Row) {
        std::shared_ptr<Result> row(new Result());
        row->id = stmt.ColumnInt64(0);
        row->displayValue = stmt.ColumnText(1);
        row->type = this->trackField;
        result->push_back(row);
    }

    return true;
}
