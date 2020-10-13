//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004-2020 musikcube team
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

#include <musikcore/library/LocalLibraryConstants.h>
#include <musikcore/db/Statement.h>
#include <musikcore/library/query/util/Serialization.h>
#include <nlohmann/json.hpp>

using namespace musik::core;
using namespace musik::core::db;
using namespace musik::core::library::query;
using namespace musik::core::library::query::serialization;
using namespace musik::core::library::constants;

#define RESET_RESULT(x) x.reset(new MetadataMapList())

const std::string AlbumListQuery::kQueryName = "AlbumListQuery";

AlbumListQuery::AlbumListQuery(const std::string& filter)
: AlbumListQuery(category::PredicateList(), filter)
{
}

AlbumListQuery::AlbumListQuery(
    const std::string& fieldIdName,
    int64_t fieldIdValue,
    const std::string& filter)
: AlbumListQuery(category::Predicate{ fieldIdName, fieldIdValue }, filter)
{
}

AlbumListQuery::AlbumListQuery(
    const category::Predicate predicate,
    const std::string& filter)
: AlbumListQuery(category::PredicateList { predicate }, filter)
{
}

AlbumListQuery::AlbumListQuery(
    const category::PredicateList predicates,
    const std::string& filter)
{
    RESET_RESULT(result);

    if (filter.size()) {
        std::string wild = filter;
        std::transform(wild.begin(), wild.end(), wild.begin(), tolower);
        this->filter = "%" + wild + "%";
    }

    category::SplitPredicates(predicates, this->regular, this->extended);
}

AlbumListQuery::~AlbumListQuery() {

}

MetadataMapListPtr AlbumListQuery::GetResult() {
    return this->result;
}

musik::core::sdk::IMapList* AlbumListQuery::GetSdkResult() {
    return this->result->GetSdkValue();
}

bool AlbumListQuery::OnRun(Connection& db) {
    RESET_RESULT(result);

    category::ArgumentList args;

    /* order of operations with args is important! otherwise bind params
    will be out of order! */
    std::string query = category::ALBUM_LIST_QUERY;
    std::string extended = InnerJoinExtended(this->extended, args);
    std::string regular = JoinRegular(this->regular, args, " AND ");
    std::string albumFilter;

    if (this->filter.size()) {
        albumFilter = category::ALBUM_LIST_FILTER;
        args.push_back(category::StringArgument(this->filter));
        args.push_back(category::StringArgument(this->filter));
    }

    category::ReplaceAll(query, "{{extended_predicates}}", extended);
    category::ReplaceAll(query, "{{regular_predicates}}", regular);
    category::ReplaceAll(query, "{{album_list_filter}}", albumFilter);

    Statement stmt(query.c_str(), db);
    Apply(stmt, args);

    while (stmt.Step() == Row) {
        int64_t albumId = stmt.ColumnInt64(0);
        std::string albumName = stmt.ColumnText(1);
        std::shared_ptr<MetadataMap> row(new MetadataMap(albumId, albumName, "album"));

        row->Set(constants::Track::ALBUM_ID, stmt.ColumnText(0));
        row->Set(constants::Track::ALBUM, albumName);
        row->Set(constants::Track::ALBUM_ARTIST_ID, stmt.ColumnText(2));
        row->Set(constants::Track::ALBUM_ARTIST_ID, stmt.ColumnText(2));
        row->Set(constants::Track::ALBUM_ARTIST, stmt.ColumnText(3));
        row->Set(constants::Track::THUMBNAIL_ID, stmt.ColumnText(4));

        result->Add(row);
    }

    return true;
}

/* ISerializableQuery */

std::string AlbumListQuery::SerializeQuery() {
    nlohmann::json query;
    query["name"] = kQueryName;
    query["options"] = {
        { "filter", this->filter },
        { "regularPredicateList", PredicateListToJson(this->regular) },
        { "extendedPredicateList", PredicateListToJson(this->extended) }
    };
    return query.dump();
}

std::string AlbumListQuery::SerializeResult() {
    nlohmann::json result = {
        { "result", MetadataMapListToJson(*this->result) }
    };
    return result.dump();
}

void AlbumListQuery::DeserializeResult(const std::string& data) {
    this->SetStatus(IQuery::Failed);
    auto json = nlohmann::json::parse(data);
    this->result = std::make_shared<MetadataMapList>();
    MetadataMapListFromJson(json["result"], *this->result);
    this->SetStatus(IQuery::Finished);
}

std::shared_ptr<AlbumListQuery> AlbumListQuery::DeserializeQuery(const std::string& data) {
    nlohmann::json options = nlohmann::json::parse(data)["options"];
    std::shared_ptr<AlbumListQuery> result(new AlbumListQuery());
    result->filter = options.value("filter", "");
    PredicateListFromJson(options["regularPredicateList"], result->regular);
    PredicateListFromJson(options["extendedPredicateList"], result->extended);
    return result;
}