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

#if 1
#ifdef WIN32
#define DUMP(x) OutputDebugStringA(x.c_str()); OutputDebugStringA("\n");
#else
#include <iostream>
#define DUMP(x) std::cout << x << "\n";
#endif
#endif

static std::map<std::string, std::string> PREDICATE_TO_COLUMN_MAP = {
    { Track::ALBUM, "album_id" },
    { Track::ARTIST, "visual_artist_id" },
    { Track::ALBUM_ARTIST, "album_artist_id" },
    { Track::GENRE, "visual_genre_id" }
};

static std::map<std::string, std::pair<std::string, std::string>> REGULAR_PROPERTY_MAP = {
    { "album", { "albums", "album_id" } },
    { "artist", { "artists", "visual_artist_id" } },
    { "album_artist", { "artists", "album_artist_id" } },
    { "genre", { "genres", "visual_genre_id" } }
};

static const std::string REGULAR_PREDICATE = " tracks.{{fk_id}}=? ";
static const std::string REGULAR_FILTER = " AND LOWER({{table}}.name) LIKE ? ";

static const std::string EXTENDED_PREDICATE = " (key=? AND meta_value_id=?) ";
static const std::string EXTENDED_FILTER = " AND LOWER(extended_metadata.value) LIKE ?";

static const std::string EXTENDED_INNER_JOIN =
    "INNER JOIN ( "
    "  SELECT id AS track_id "
    "  FROM extended_metadata "
    "  WHERE {{extended_predicates}} "
    "  GROUP BY track_id "
    "  HAVING COUNT(track_id)={{extended_predicate_count}} "
    ") AS md ON tracks.id=md.track_id ";

static const std::string REGULAR_PROPERTY_QUERY =
    "SELECT DISTINCT {{table}}.id, {{table}}.name "
    "FROM {{table}}, tracks "
    "{{extended_predicates}} "
    "WHERE {{table}}.id=tracks.{{fk_id}} AND tracks.visible=1 "
    "{{regular_predicates}} "
    "{{regular_filter}} "
    "ORDER BY {{table}}.sort_order";

static const std::string EXTENDED_PROPERTY_QUERY =
    "SELECT DISTINCT meta_value_id, value "
    "FROM extended_metadata "
    "INNER JOIN ( "
    "  SELECT id AS track_id "
    "  FROM tracks "
    "  WHERE tracks.visible=1 {{regular_predicates}} "
    ") AS reg on extended_metadata.id=reg.track_id "
    "{{extended_predicates}} "
    "WHERE extended_metadata.key=? "
    "{{extended_filter}} "
    "ORDER BY extended_metadata.value ASC";

static const std::string UNFILTERED_PLAYLISTS_QUERY =
    "SELECT DISTINCT id, name "
    "FROM playlists "
    "ORDER BY name;";

static const std::string FILTERED_PLAYLISTS_QUERY =
    "SELECT DISTINCT id, name "
    "FROM playlists"
    "WHERE LOWER(name) LIKE LOWER(?) "
    "ORDER BY name;";

/* quick and dirty structures used to track bind arguments */
struct Id : public CategoryListQuery::Argument {
    int64_t id;
    Id(int64_t id) : id(id) { }
    virtual void Bind(Statement& stmt, int pos) const { stmt.BindInt64(pos, id); }
};

struct String : public CategoryListQuery::Argument {
    std::string str;
    String(const std::string& str) : str(str) { }
    virtual void Bind(Statement& stmt, int pos) const { stmt.BindText(pos, str.c_str()); }
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

static void replaceAll(std::string& input, const std::string& find, const std::string& replace) {
    size_t pos = input.find(find);
    while (pos != std::string::npos) {
        input.replace(pos, find.size(), replace);
        pos = input.find(find, pos + replace.size());
    }
}

CategoryListQuery::CategoryListQuery(
    const std::string& trackField, const std::string& filter)
: CategoryListQuery(trackField, PredicateList(), filter) {
}

CategoryListQuery::CategoryListQuery(
    const std::string& trackField,
    const Predicate predicate,
    const std::string& filter)
: CategoryListQuery(trackField, PredicateList{ predicate }, filter) {
}

CategoryListQuery::CategoryListQuery(
    const std::string& trackField,
    const PredicateList predicates,
    const std::string& filter)
: trackField(trackField)
, filter(filter) {
    RESET_RESULT(result);

    if (this->filter.size()) {
        /* transform "FilteR" => "%filter%" */
        std::string wild = this->filter;
        std::transform(wild.begin(), wild.end(), wild.begin(), tolower);
        this->filter = "%" + wild + "%";
    }

    auto end = REGULAR_PROPERTY_MAP.end();

    for (auto p : predicates) {
        if (REGULAR_PROPERTY_MAP.find(p.first) != end) {
            this->regular.push_back(p);
        }
        else {
            this->extended.push_back(p);
        }
    }

    if (trackField == "playlists") {
        this->outputType = Playlist;
    }
    else if (REGULAR_PROPERTY_MAP.find(trackField) != end) {
        this->outputType = Regular;
    }
    else {
        this->outputType = Extended;
    }
}

CategoryListQuery::~CategoryListQuery() {

}

std::string CategoryListQuery::JoinRegular(
    const PredicateList& pred, ArgumentList& args, const std::string& prefix)
{
    std::string result;
    if (pred.size()) {
        for (size_t i = 0; i < pred.size(); i++) {
            if (i > 0) { result += " AND "; }
            auto p = pred[i];
            auto str = REGULAR_PREDICATE;
            auto map = REGULAR_PROPERTY_MAP[p.first];
            replaceAll(str, "{{fk_id}}", map.second);
            result += str;
            args.push_back(std::make_shared<Id>(p.second));
        }

        if (prefix.size()) {
            result = prefix + result;
        }
    }
    return result;
}

std::string CategoryListQuery::InnerJoinExtended(
    const PredicateList& pred, ArgumentList& args)
{
    std::string result;

    std::string joined = JoinExtended(pred, args);
    if (joined.size()) {
        result = EXTENDED_INNER_JOIN;
        replaceAll(result, "{{extended_predicates}}", joined);
        replaceAll(result, "{{extended_predicate_count}}", std::to_string(pred.size()));
    }

    return result;
}

std::string CategoryListQuery::JoinExtended(
    const PredicateList& pred, ArgumentList& args)
{
    std::string result;
    for (size_t i = 0; i < pred.size(); i++) {
        if (i > 0) { result += " OR "; }
        result += EXTENDED_PREDICATE;
        auto p = pred[i];
        args.push_back(std::make_shared<String>(p.first));
        args.push_back(std::make_shared<Id>(p.second));
    }
    return result;
}

void CategoryListQuery::Apply(Statement& stmt, const ArgumentList& args) {
    for (size_t i = 0; i < args.size(); i++) {
        args[i]->Bind(stmt, (int) i);
    }
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

void CategoryListQuery::QueryPlaylist(musik::core::db::Connection& db) {
    bool filtered = this->filter.size();

    std::string query = filtered
        ? FILTERED_PLAYLISTS_QUERY
        : UNFILTERED_PLAYLISTS_QUERY;

    Statement stmt(query.c_str() , db);

    if (filtered) {
        stmt.BindText(0, this->filter);
    }

    ProcessResult(stmt);
}

void CategoryListQuery::QueryRegular(musik::core::db::Connection &db) {
    ArgumentList args;

    auto prop = REGULAR_PROPERTY_MAP[this->trackField];
    std::string query = REGULAR_PROPERTY_QUERY;
    std::string extended = InnerJoinExtended(this->extended, args);
    std::string regular = JoinRegular(this->regular, args, " AND ");
    std::string regularFilter;

    if (this->filter.size()) {
        regularFilter = REGULAR_FILTER;
        replaceAll(regularFilter, "{{table}}", prop.first);
        args.push_back(std::make_shared<String>(this->filter));
    }

    replaceAll(query, "{{table}}", prop.first);
    replaceAll(query, "{{fk_id}}", prop.second);
    replaceAll(query, "{{extended_predicates}}", extended);
    replaceAll(query, "{{regular_predicates}}", regular);
    replaceAll(query, "{{regular_filter}}", regularFilter);

    Statement stmt(query.c_str(), db);
    Apply(stmt, args);
    ProcessResult(stmt);
}

void CategoryListQuery::QueryExtended(musik::core::db::Connection &db) {
    ArgumentList args;

    std::string query = EXTENDED_PROPERTY_QUERY;
    std::string regular = JoinRegular(this->regular, args, " AND ");
    std::string extended = InnerJoinExtended(this->extended, args);
    std::string extendedFilter;

    if (this->filter.size()) {
        extendedFilter = EXTENDED_FILTER;
        args.push_back(std::make_shared<String>(this->filter));
    }

    replaceAll(query, "{{regular_predicates}}", regular);
    replaceAll(query, "{{extended_predicates}}", extended);
    replaceAll(query, "{{extended_filter}}", extendedFilter);

    args.push_back(std::make_shared<String>(this->trackField));

    DUMP(query);

    Statement stmt(query.c_str(), db);
    Apply(stmt, args);
    ProcessResult(stmt);
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

    switch (this->outputType) {
        case Playlist: QueryPlaylist(db); break;
        case Regular: QueryRegular(db); break;
        case Extended: QueryExtended(db); break;
    }

    return true;
}
