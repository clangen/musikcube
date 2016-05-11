#pragma once

#include "stdafx.h"
#include "CategoryListQuery.h"

#include <core/db/Statement.h>

using musik::core::db::Statement;
using musik::core::db::Row;

CategoryListQuery::CategoryListQuery() {
    result.reset(new std::vector<std::string>());
}

CategoryListQuery::~CategoryListQuery() {

}

CategoryListQuery::Result CategoryListQuery::GetResult() {
    return this->result;
}

bool CategoryListQuery::OnRun(Connection& db) {
    if (result) {
        result.reset(new std::vector<std::string>());
    }

    std::string query =
        "SELECT DISTINCT artists.name "
        "FROM artists, tracks "
        "WHERE artists.id = tracks.visual_artist_id;";

    Statement stmt(query.c_str(), db);
    
    while (stmt.Step() == Row) {
        result->push_back(stmt.ColumnText(0));
    }

    return false;
}