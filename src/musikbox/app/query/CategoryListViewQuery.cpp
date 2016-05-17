#pragma once

#include "stdafx.h"
#include "CategoryListViewQuery.h"

#include <core/db/Statement.h>

using musik::core::db::Statement;
using musik::core::db::Row;

#define reset(x) x.reset(new std::vector<std::shared_ptr<Result>>);

CategoryListViewQuery::CategoryListViewQuery() {
    reset(result);
}

CategoryListViewQuery::~CategoryListViewQuery() {

}

CategoryListViewQuery::ResultList CategoryListViewQuery::GetResult() {
    return this->result;
}

bool CategoryListViewQuery::OnRun(Connection& db) {
    reset(result);

    std::string query =
        "SELECT DISTINCT albums.id, albums.name "
        "FROM albums, tracks "
        "WHERE albums.id = tracks.album_id "
        "ORDER BY albums.sort_order;";

    Statement stmt(query.c_str(), db);
    
    while (stmt.Step() == Row) {
        std::shared_ptr<Result> row(new Result());
        row->id = stmt.ColumnInt64(0);
        row->displayValue = stmt.ColumnText(1);
        result->push_back(row);
    }

    return true;
}