//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004-2023 musikcube team
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

#include <iterator>
#include "pch.hpp"
#include "CategoryListQuery.h"
#include <musikcore/plugin/library_query_names.h>
#include "util/SourcePredicate.h"
#include <musikcore/library/LocalLibraryConstants.h>
#include <musikcore/library/query/util/Serialization.h>
#include <common/String.h>
#include <musikcore/db/Statement.h>

using musik::core::db::Statement;
using musik::core::db::Row;

using namespace musik::core::sdk;
using namespace musik::core::db;
using namespace musik::core::library::constants;
using namespace musik::core::library::query;
using namespace musik::core::library::query::serialization;

static const std::string kUnfilteredPlaylistsQuery =
    "SELECT DISTINCT id, name "
    "FROM playlists "
    "ORDER BY name;";

static const std::string kFilteredPlaylistsQuery =
    "SELECT DISTINCT id, name "
    "FROM playlists"
    "WHERE LOWER(name) {{match_type}} ? "
    "ORDER BY name;";

const std::string CategoryListQuery::kQueryName = MC_LIBRARY_QUERY_CATEGORIES;
const std::string CategoryListQuery::kCategoryPageQueryName = MC_LIBRARY_QUERY_CATEGORY_PAGE;

static std::string getMatchType(CategoryListQuery::MatchType matchType) {
    return matchType == CategoryListQuery::MatchType::Regex ? "REGEXP" : "LIKE";
}

CategoryListQuery::CategoryListQuery(
    MatchType matchType,
    const std::string& trackField,
    const std::string& filter)
: CategoryListQuery(matchType, trackField, category::PredicateList(), filter) {
}

CategoryListQuery::CategoryListQuery(
    MatchType matchType,
    const std::string& trackField,
    const category::Predicate predicate,
    const std::string& filter)
: CategoryListQuery(matchType, trackField, category::PredicateList{ predicate }, filter) {
}

CategoryListQuery::CategoryListQuery(
    MatchType matchType,
    const std::string& trackField,
    const category::PredicateList predicates,
    const std::string& filter)
: trackField(trackField)
, filter(filter)
, matchType(matchType) {
    result = std::make_shared<CategoryValues>();

    if (this->filter.size() && this->matchType == MatchType::Substring) {
        /* transform "FilteR" => "%filter%" */
        std::string wild = this->filter;
        std::transform(wild.begin(), wild.end(), wild.begin(), tolower);
        this->filter = category::SubstringPattern(wild);
    }

    category::SplitPredicates(predicates, this->regular, this->extended);


    if (trackField == "playlists") {
        this->outputType = OutputType::Playlist;
    }
    else if (trackField == "duration" || trackField == "rating") {
        this->outputType = OutputType::Numeric;
    }
    else if (category::GetPropertyType(trackField) == category::PropertyType::Regular) {
        this->outputType = OutputType::Regular;
    }
    else {
        this->outputType = OutputType::Extended;
    }
}

void CategoryListQuery::SetTrackFilter(std::string search, MatchType mode) {
    this->trackMatchMode = mode;
    this->trackSearch = std::move(search);
}

void CategoryListQuery::SetPage(int64_t anchor, size_t offset, size_t limit) {
    this->paged = true;
    this->anchorId = anchor;
    this->pageOffset = offset;
    this->pageLimit = limit;
}

CategoryListQuery::Result CategoryListQuery::GetResult() noexcept {
    return this->result;
}

int CategoryListQuery::GetIndexOf(int64_t id) {
    auto result = this->GetResult();
    for (size_t i = 0; i < result->size(); i++) {
        if (id == result->at(i).id) {
            return (int) i;
        }
    }
    return -1;
}

void CategoryListQuery::QueryPlaylist(musik::core::db::Connection& db) {
    category::ArgumentList args;
    std::string query = "SELECT id,name,source_id FROM playlists WHERE 1";
    if (!this->filter.empty()) {
        query += " AND LOWER(name) " + getMatchType(this->matchType) + " ?";
        if (this->matchType == MatchType::Substring) query += " ESCAPE '\\'";
        args.push_back(category::StringArgument(this->filter));
    }
    query += category::SourcePredicate(this->scope, "source_id", args, true);
    query += " ORDER BY name,id";
    if (this->paged) {
        query.replace(0, std::string("SELECT id,name,source_id").size(), "SELECT id,name,name,source_id");
        this->QueryPage(db, std::move(query), std::move(args));
        return;
    }
    Statement stmt(query.c_str(), db);
    category::Apply(stmt, args);
    while (stmt.Step() == Row) {
        const size_t index = this->total++;
        if (stmt.ColumnInt64(0) == this->anchorId) {
            this->anchorIndex = index;
        }
        if (this->paged && (index < this->pageOffset || index - this->pageOffset >= this->pageLimit)) {
            continue;
        }
        CategoryValue value {
            .value = stmt.ColumnText(1),
            .id = stmt.ColumnInt64(0),
            .type = this->trackField,
            .sourceId = stmt.IsNull(2) ? std::nullopt : std::optional<int32_t>(stmt.ColumnInt32(2)),
        };
        this->result->push_back(std::move(value));
    }
}

static std::string makeTrackFilter(
    const std::string& text, CategoryListQuery::MatchType mode, category::ArgumentList& args) {
    if (text.empty()) {
        return {};
    }
    const std::string op = mode == CategoryListQuery::MatchType::Regex ? "REGEXP" : "LIKE";
    const auto pattern = mode == CategoryListQuery::MatchType::Regex ? text : category::SubstringPattern(text);
    const std::string escape = mode == CategoryListQuery::MatchType::Regex ? "" : " ESCAPE '\\'";
    for (int i = 0; i < 4; ++i) {
        args.push_back(category::StringArgument(pattern));
    }
    return " AND (tracks.title " + op + " ?" + escape + " OR tracks.album_id IN (SELECT id FROM albums WHERE name " + op +
        " ?" + escape + ") OR tracks.visual_artist_id IN (SELECT id FROM artists WHERE name " + op +
        " ?" + escape + ") OR tracks.visual_genre_id IN (SELECT id FROM genres WHERE name " + op + " ?" + escape + ")) ";
}

void CategoryListQuery::QueryRegular(musik::core::db::Connection &db) {
    category::ArgumentList args;

    /* order of operations with args is important! otherwise bind params
    will be out of order! */
    auto prop = category::REGULAR_PROPERTY_MAP[this->trackField];
    std::string query = category::REGULAR_PROPERTY_QUERY;
    std::string extended = InnerJoinExtended(this->extended, args);
    std::string regular = JoinRegular(this->regular, args, " AND ");
    std::string regularFilter;

    if (this->filter.size()) {
        regularFilter = category::REGULAR_FILTER;
        category::ReplaceAll(regularFilter, "{{table}}", prop.first);
        category::ReplaceAll(regularFilter, "{{match_type}}", getMatchType(matchType));
        if (matchType == MatchType::Substring) regularFilter += " ESCAPE '\\'";
        args.push_back(category::StringArgument(this->filter));
    }

    category::ReplaceAll(query, "{{table}}", prop.first);
    category::ReplaceAll(query, "{{fk_id}}", prop.second);
    category::ReplaceAll(query, "{{extended_predicates}}", extended);
    category::ReplaceAll(query, "{{regular_predicates}}", regular);
    category::ReplaceAll(query, "{{regular_filter}}", regularFilter);
    const auto trackFilterSql = makeTrackFilter(this->trackSearch, this->trackMatchMode, args);
    category::ReplaceAll(query, "{{track_filter}}", trackFilterSql);

    category::ApplySourceScope(query, this->scope, "tracks.source_id", args);
    if (this->paged) {
        const auto order = query.rfind("ORDER BY ");
        if (order != std::string::npos) {
            query.replace(order, std::string::npos,
                "ORDER BY " + prop.first + ".sort_order, " + prop.first + ".id");
        }
    }
    if (this->paged) {
        const auto projection = "SELECT DISTINCT " + prop.first + ".id, " + prop.first + ".name";
        query.replace(0, projection.size(), projection + ", " + prop.first + ".sort_order, NULL");
        this->QueryPage(db, std::move(query), std::move(args));
        return;
    }
    Statement stmt(query.c_str(), db);
    Apply(stmt, args);
    ProcessResult(stmt);
}

void CategoryListQuery::QueryExtended(musik::core::db::Connection& db) {
    category::ArgumentList args;
    std::string query = category::EXTENDED_PROPERTY_QUERY;
    const auto regular = category::JoinRegular(this->regular, args, " AND ");
    const auto trackFilter = makeTrackFilter(this->trackSearch, this->trackMatchMode, args);
    const auto extended = category::InnerJoinExtended(this->extended, args, "extended_metadata.id");
    args.push_back(category::StringArgument(this->trackField));
    std::string nameFilter;
    if (!this->filter.empty()) {
        nameFilter = category::EXTENDED_FILTER;
        category::ReplaceAll(nameFilter, "{{match_type}}", getMatchType(matchType));
        if (matchType == MatchType::Substring) nameFilter += " ESCAPE '\\'";
        args.push_back(category::StringArgument(this->filter));
    }
    category::ReplaceAll(query, "{{regular_predicates}}", regular);
    category::ReplaceAll(query, "{{track_filter}}", trackFilter);
    category::ReplaceAll(query, "{{extended_predicates}}", extended);
    category::ReplaceAll(query, "{{extended_filter}}", nameFilter);
    category::ApplySourceScope(query, this->scope, "extended_metadata.source_id", args);
    if (this->paged) {
        const auto order = query.rfind("ORDER BY ");
        if (order != std::string::npos) {
            query.replace(order, std::string::npos,
                "ORDER BY extended_metadata.value, meta_value_id");
        }
    }
    if (this->paged) {
        const std::string projection = "SELECT DISTINCT meta_value_id, value";
        query.replace(0, projection.size(), projection + ", value, NULL");
        this->QueryPage(db, std::move(query), std::move(args));
        return;
    }
    Statement stmt(query.c_str(), db);
    Apply(stmt, args);
    ProcessResult(stmt);
}

void CategoryListQuery::QueryNumeric(musik::core::db::Connection& db) {
    category::ArgumentList args;
    const auto column = this->trackField == "duration" ? "duration" : "rating";
    std::string query =
        "SELECT DISTINCT tracks." + std::string(column) + ", CAST(tracks." + column + " AS TEXT) FROM tracks ";
    const auto extended = category::InnerJoinExtended(this->extended, args);
    query += extended + " WHERE tracks.visible=1 AND tracks." + column + " IS NOT NULL AND tracks." + column + " >= 0";
    query += category::JoinRegular(this->regular, args, " AND ");
    query += makeTrackFilter(this->trackSearch, this->trackMatchMode, args);
    if (!this->filter.empty()) {
        query += " AND CAST(tracks." + std::string(column) + " AS TEXT) " + getMatchType(matchType) + " ?";
        if (matchType == MatchType::Substring) query += " ESCAPE '\\'";
        args.push_back(category::StringArgument(this->filter));
    }
    query += " {{source_predicate}} ";
    category::ApplySourceScope(query, this->scope, "tracks.source_id", args);
    query += " ORDER BY tracks." + std::string(column);
    if (this->paged) {
        const auto from = query.find(" FROM tracks ");
        query.insert(from, ", tracks." + std::string(column) + ", NULL");
        this->QueryPage(db, std::move(query), std::move(args));
        return;
    }
    Statement stmt(query.c_str(), db);
    Apply(stmt, args);
    ProcessResult(stmt);
}

/* page a category query without returning the full result to the caller.
`raw` holds distinct matching values, `ranked` assigns their stable positions,
and `stats` counts them and locates the optional anchor in the same snapshot.
For example, offset 100 and limit 50 returns up to 50 rows, the full count,
and the anchor position; an offset beyond the end still returns the count. */
void CategoryListQuery::QueryPage(musik::core::db::Connection& db, std::string sql, category::ArgumentList args) {
    /* the window supplies the final order, so avoid sorting the input twice. */
    const auto order = sql.rfind("ORDER BY ");
    if (order != std::string::npos) {
        sql.erase(order);
    }
    sql = "WITH raw(key,label,sort_key,source_id) AS MATERIALIZED (" + sql +
        "), ranked AS MATERIALIZED ("
        " SELECT key,label,source_id, row_number() OVER (ORDER BY sort_key, key)-1 AS position FROM raw"
        "), stats AS ("
        " SELECT count(*) AS total, coalesce(max(CASE WHEN key=? THEN position END),-1) AS anchor FROM ranked"
        ") SELECT page.key,page.label,page.source_id,stats.total,stats.anchor FROM stats"
        " LEFT JOIN (SELECT * FROM ranked WHERE position>=? ORDER BY position LIMIT ?) page ON 1"
        " ORDER BY page.position";
    args.push_back(category::IdArgument(this->anchorId));
    args.push_back(category::IdArgument(static_cast<int64_t>(this->pageOffset)));
    args.push_back(category::IdArgument(static_cast<int64_t>(this->pageLimit)));
    Statement stmt(sql.c_str(), db);
    Apply(stmt, args);
    while (stmt.Step() == Row) {
        this->total = static_cast<size_t>(stmt.ColumnInt64(3));
        const auto anchor = stmt.ColumnInt64(4);
        this->anchorIndex = anchor < 0 ? SIZE_MAX : static_cast<size_t>(anchor);
        if (stmt.IsNull(0)) {
            continue;
        }
        std::string label = stmt.ColumnText(1);
        CategoryValue value {
            .value = std::move(label),
            .id = stmt.ColumnInt64(0),
            .type = this->trackField,
            .sourceId = !stmt.IsNull(2) ? std::optional<int32_t>(stmt.ColumnInt32(2)) : std::nullopt,
        };
        this->result->push_back(std::move(value));
    }
}

void CategoryListQuery::ProcessResult(musik::core::db::Statement& stmt) {
    this->total = 0;
    this->anchorIndex = SIZE_MAX;
    while (stmt.Step() == Row) {
        const auto id = stmt.ColumnInt64(0);
        const size_t index = this->total++;
        if (id == this->anchorId) {
            this->anchorIndex = index;
        }
        this->result->push_back({
            .value = stmt.ColumnText(1),
            .id = id,
            .type = this->trackField,
        });
    }
}

bool CategoryListQuery::OnRun(Connection& db) {
    const auto errors = db.ErrorCount();
    result = std::make_shared<CategoryValues>();
    this->total = 0;
    this->anchorIndex = SIZE_MAX;
    if (this->scope.MatchesNoTracks() && this->outputType != OutputType::Playlist) { return true; }


    switch (this->outputType) {
        case OutputType::Playlist: QueryPlaylist(db); break;
        case OutputType::Regular: QueryRegular(db); break;
        case OutputType::Extended: QueryExtended(db); break;
        case OutputType::Numeric:
            QueryNumeric(db);
            break;
    }

    return db.ErrorCount() == errors;
}

/* ISerializableQuery */

std::string CategoryListQuery::SerializeQuery() {
    nlohmann::json query;
    query["name"] = this->Name();
    query["options"] = {
        { "trackField", this->trackField },
        { "filter", this->filter },
        { "matchType", this->matchType },
        { "outputType", this->outputType },
        { "regularPredicateList", PredicateListToJson(this->regular) },
        { "extendedPredicateList", PredicateListToJson(this->extended) }
    };
    if (!this->trackSearch.empty()) {
        query["options"]["trackSearch"] = this->trackSearch;
        query["options"]["trackRegex"] = this->trackMatchMode == MatchType::Regex;
    }
    if (this->paged) {
        query["options"]["offset"] = this->pageOffset;
        query["options"]["limit"] = this->pageLimit;
        query["options"]["anchorId"] = this->anchorId;
    }
    query["options"]["scope"] = EncodeScope(this->scope);
    return query.dump();
}

std::string CategoryListQuery::SerializeResult() {
    nlohmann::json result = {
        { "result", ValueListToJson(*this->result) }
    };
    if (this->paged) {
        result["total"] = this->total;
        result["anchorIndex"] = this->anchorIndex == SIZE_MAX ? -1 : static_cast<int64_t>(this->anchorIndex);
    }
    return result.dump();
}

void CategoryListQuery::DeserializeResult(const std::string& data) {
    this->SetStatus(IQuery::Failed);
    auto json = nlohmann::json::parse(data);
    this->result = std::make_shared<CategoryValues>();
    ValueListFromJson(json["result"], *this->result);
    if (this->paged) {
        if (!json.at("total").is_number_unsigned()) {
            throw std::runtime_error("invalid category total");
        }
        this->total = json.at("total").get<size_t>();
        auto anchor = json.at("anchorIndex").get<int64_t>();
        if (anchor < -1 || (anchor >= 0 && static_cast<uint64_t>(anchor) >= this->total)) {
            throw std::runtime_error("invalid category anchor index");
        }
        this->anchorIndex = anchor < 0 ? SIZE_MAX : static_cast<size_t>(anchor);
        const size_t expected =
            this->pageOffset < this->total ? std::min(this->pageLimit, this->total - this->pageOffset) : 0;
        if (this->result->size() != expected || (this->anchorId < 0 && this->anchorIndex != SIZE_MAX)) {
            throw std::runtime_error("invalid category page");
        }
        if (this->anchorIndex >= this->pageOffset && this->anchorIndex != SIZE_MAX &&
            this->anchorIndex - this->pageOffset < this->result->size() &&
            this->result->at(this->anchorIndex - this->pageOffset).id != this->anchorId)
        {
            throw std::runtime_error("category anchor does not match page");
        }
    }
    this->SetStatus(IQuery::Finished);
}

std::shared_ptr<CategoryListQuery> CategoryListQuery::DeserializeQuery(const std::string& data) {
    const auto message = nlohmann::json::parse(data);
    const auto& options = message["options"];
    std::shared_ptr<CategoryListQuery> result(new CategoryListQuery());
    if (options.contains("trackSearch") || message.value("name", "") == kCategoryPageQueryName) {
        result->SetTrackFilter(options.value("trackSearch", ""),
            options.value("trackRegex", false) ? MatchType::Regex : MatchType::Substring);
    }
    if (message.value("name", "") == kCategoryPageQueryName) {
        result->SetPage(options.at("anchorId").get<int64_t>(), options.at("offset").get<size_t>(),
            options.at("limit").get<size_t>());
    }
    result->trackField = options.value("trackField", "");
    result->filter = options.value("filter", "");
    result->matchType = options.value("matchType", MatchType::Substring);
    result->outputType = static_cast<OutputType>(options.value("outputType", OutputType::Regular));
    PredicateListFromJson(options["regularPredicateList"], result->regular);
    PredicateListFromJson(options["extendedPredicateList"], result->extended);
    result->SetScope(DecodeScope(options));
    return result;
}
