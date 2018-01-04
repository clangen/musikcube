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
: CategoryListQuery(trackField, category::PredicateList(), filter) {
}

CategoryListQuery::CategoryListQuery(
    const std::string& trackField,
    const category::Predicate predicate,
    const std::string& filter)
: CategoryListQuery(trackField, category::PredicateList{ predicate }, filter) {
}

CategoryListQuery::CategoryListQuery(
    const std::string& trackField,
    const category::PredicateList predicates,
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


    category::SplitPredicates(predicates, this->regular, this->extended);

    auto end = category::REGULAR_PROPERTY_MAP.end();

    if (trackField == "playlists") {
        this->outputType = Playlist;
    }
    else if (category::GetPropertyType(trackField) == category::PropertyType::Regular) {
        this->outputType = Regular;
    }
    else {
        this->outputType = Extended;
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
    category::ArgumentList args;

    auto prop = category::REGULAR_PROPERTY_MAP[this->trackField];
    std::string query = category::REGULAR_PROPERTY_QUERY;
    std::string extended = InnerJoinExtended(this->extended, args);
    std::string regular = JoinRegular(this->regular, args, " AND ");
    std::string regularFilter;

    if (this->filter.size()) {
        regularFilter = category::REGULAR_FILTER;
        replaceAll(regularFilter, "{{table}}", prop.first);
        args.push_back(category::StringArgument(this->filter));
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
    category::ArgumentList args;

    std::string query = category::EXTENDED_PROPERTY_QUERY;
    std::string regular = category::JoinRegular(this->regular, args, " AND ");
    std::string extended = category::InnerJoinExtended(this->extended, args);
    std::string extendedFilter;

    if (this->filter.size()) {
        extendedFilter = category::EXTENDED_FILTER;
        args.push_back(category::StringArgument(this->filter));
    }

    replaceAll(query, "{{regular_predicates}}", regular);
    replaceAll(query, "{{extended_predicates}}", extended);
    replaceAll(query, "{{extended_filter}}", extendedFilter);

    args.push_back(category::StringArgument(this->trackField));

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
