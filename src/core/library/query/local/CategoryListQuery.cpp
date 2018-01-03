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

#define RESET_RESULT(x) x.reset(new std::vector<std::shared_ptr<Result>>);

#if 0
#ifdef WIN32
#define DUMP(x) OutputDebugStringA(x.c_str()); OutputDebugStringA("\n");
#else
#include <iostream>
#define DUMP(x) std::cout << x << "\n";
#endif
#endif

static const std::string REGULAR_QUERY_PREDICATE = " AND %s=? ";
static const std::string REGULAR_QUERY_PREDICATE_TABLES = "";

static const std::string EXTENDED_QUERY_PREDICATE_TABLES =
    ", track_meta tm, meta_keys mk, meta_values mv ";

static const std::string EXTENDED_QUERY_PREDICATE =
    " AND tracks.id=tm.track_id "
    " AND tm.meta_value_id=mv.id "
    " AND mv.meta_key_id=mk.id "
    " AND mk.id=? "
    " AND mv.id=? ";

static std::map<std::string, std::string> PREDICATE_TO_COLUMN_MAP = {
    { Track::ALBUM, "album_id" },
    { Track::ARTIST, "visual_artist_id" },
    { Track::ALBUM_ARTIST, "album_artist_id" },
    { Track::GENRE, "visual_genre_id" }
};

static std::map<std::string, std::pair<std::string, std::string>> PROPERTY_MAP = {
    { "album",{ "albums", "album_id" } },
    { "artist",{ "artists", "visual_artist_id" } },
    { "album_artist",{ "artists", "album_artist_id" } },
    { "genre", { "genres", "visual_genre_id" } }
};

static const std::string UNFILTERED_PROPERTY_QUERY =
    "SELECT DISTINCT {{table}}.id, {{table}}.name "
    "FROM {{table}}, tracks {{extended_tables}} "
    "WHERE {{table}}.id = tracks.{{id_column}} AND tracks.visible = 1 {{predicate}}"
    "ORDER BY {{table}}.sort_order;";

static const std::string FILTERED_PROPERTY_QUERY =
    "SELECT DISTINCT {{table}}.id, {{table}}.name "
    "FROM {{table}}, tracks {{extended_tables}} "
    "WHERE {{table}}.id = tracks.{{id_column}} AND LOWER({{table}}.name) LIKE ? AND tracks.visible = 1 {{predicate}} "
    "ORDER BY {{table}}.sort_order;";

//static const std::string UNFILTERED_EXTENDED_PROPERTY_QUERY =
//    "SELECT DISTINCT meta_values.* "
//    "FROM meta_values, track_meta, tracks "
//    "WHERE "
//    "  meta_values.id = track_meta.meta_value_id AND "
//    "  track_meta.track_id = tracks.id AND "
//    "  tracks.visible = 1 AND "
//    "  meta_values.meta_key_id IN( "
//    "    SELECT DISTINCT meta_keys.id "
//    "    FROM meta_keys "
//    "    WHERE LOWER(meta_keys.name) = LOWER(?) "
//    "  ) "
//    "{{predicate}} "
//    "ORDER BY meta_values.content ASC";
//
//static const std::string FILTERED_EXTENDED_PROPERTY_QUERY =
//    "SELECT DISTINCT meta_values.* "
//    "FROM meta_values, track_meta, tracks "
//    "WHERE "
//    "  meta_values.id = track_meta.meta_value_id AND "
//    "  track_meta.track_id = tracks.id AND "
//    "  tracks.visible = 1 AND "
//    "  meta_values.meta_key_id IN( "
//    "    SELECT DISTINCT meta_keys.id "
//    "    FROM meta_keys "
//    "    WHERE LOWER(meta_keys.name) = LOWER(?) "
//    "  ) "
//    "AND LOWER(meta_values.content) LIKE LOWER(?) "
//    "{{predicate}} "
//    "ORDER BY meta_values.content ASC";

static const std::string UNFILTERED_PLAYLISTS_QUERY =
    "SELECT DISTINCT id, name "
    "FROM playlists "
    "ORDER BY name;";

static const std::string FILTERED_PLAYLISTS_QUERY =
    "SELECT DISTINCT id, name "
    "FROM playlists"
    "WHERE LOWER(name) LIKE LOWER(?) "
    "ORDER BY name;";

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

static void replaceAll(std::string& input, const std::string& find, const std::string& replace) {
    size_t pos = input.find(find);
    while (pos != std::string::npos) {
        input.replace(pos, find.size(), replace);
        pos = input.find(find, pos + replace.size());
    }
}

CategoryListQuery::CategoryListQuery(
    const std::string& trackField, const std::string& filter)
: CategoryListQuery(trackField, { "", -1LL }, filter) {
}

CategoryListQuery::CategoryListQuery(
    const std::string& trackField,
    const Predicate predicate,
    const std::string& filter)
: trackField(trackField)
, predicate(predicate)
, filter(filter) {
    RESET_RESULT(result);

    if (this->filter.size()) {
        /* transform "FilteR" => "%filter%" */
        std::string wild = this->filter;
        std::transform(wild.begin(), wild.end(), wild.begin(), tolower);
        this->filter = "%" + wild + "%";
    }

    if (trackField == "playlists") {
        this->type = Playlist;
    }
    else if (PROPERTY_MAP.find(trackField) != PROPERTY_MAP.end()) {
        this->type = Regular;
    }
    else {
        this->type = Extended;
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

void CategoryListQuery::QueryRegular(musik::core::db::Connection &db) {
    bool filtered = this->filter.size() > 0;

    bool predicated =
        predicate.first.size() &&
        predicate.first != Playlists::TABLE_NAME &&
        predicate.second > 0;

    std::string query;

    if (this->type == Playlist) {
        query = filtered ? FILTERED_PLAYLISTS_QUERY : UNFILTERED_PLAYLISTS_QUERY;
    }
    else {
        query = filtered ? FILTERED_PROPERTY_QUERY : UNFILTERED_PROPERTY_QUERY;
        auto fields = PROPERTY_MAP.find(this->trackField);
        replaceAll(query, "{{table}}", fields->second.first);
        replaceAll(query, "{{id_column}}", fields->second.second);
    }

    int64_t extendedKeyId = 0;
    std::string predicateTables = "";
    std::string predicateStatement = "";

    if (predicated) {
        auto end = PREDICATE_TO_COLUMN_MAP.end();
        if (PREDICATE_TO_COLUMN_MAP.find(predicate.first) != end) {
            /* regular/simple/optimized predicate... */
            predicateStatement = boost::str(boost::format(
                REGULAR_QUERY_PREDICATE) % PREDICATE_TO_COLUMN_MAP[predicate.first]);

            predicateTables = REGULAR_QUERY_PREDICATE_TABLES;
        }
        else {
            /* extended metadata predicate. gotta do more work. whee... */
            Statement keyQuery("SELECT DISTINCT id FROM meta_keys WHERE LOWER(name)=LOWER(?)", db);
            keyQuery.BindText(0, this->predicate.first);
            if (keyQuery.Step() == db::Row) {
                extendedKeyId = keyQuery.ColumnInt64(0);
            }

            if (extendedKeyId > 0) {
                predicateStatement = EXTENDED_QUERY_PREDICATE;
                predicateTables = EXTENDED_QUERY_PREDICATE_TABLES;
            }
            else {
                predicated = false;
            }
        }
    }

    replaceAll(query, "{{predicate}}", predicateStatement);
    replaceAll(query, "{{extended_tables}}", predicateTables);

    int bindIndex = 0;
    Statement stmt(query.c_str(), db);

    if (filtered) {
        stmt.BindText(bindIndex++, this->filter);
    }

    if (predicated) {
        if (extendedKeyId > 0) {
            stmt.BindInt64(bindIndex++, extendedKeyId);
            stmt.BindInt64(bindIndex++, predicate.second);
        }
        else {
            stmt.BindInt64(bindIndex++, predicate.second);
        }
    }

    this->ProcessResult(stmt);
}

void CategoryListQuery::QueryExtended(musik::core::db::Connection &db) {

}

void CategoryListQuery::ProcessResult(musik::core::db::Statement &stmt) {
    while (stmt.Step() == Row) {
        std::shared_ptr<Result> row(new Result());
        row->id = stmt.ColumnInt64(0);
        row->displayValue = stmt.ColumnText(1);
        row->type = this->trackField;
        result->push_back(row);
    }
}

bool CategoryListQuery::OnRun(Connection& db) {
    RESET_RESULT(result);
    (this->type == Extended) ? QueryExtended(db) : QueryRegular(db);
    return true;
}
