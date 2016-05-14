#pragma once

#include "stdafx.h"
#include "TracklistQuery.h"

#include <core/db/Statement.h>

using musik::core::db::Statement;
using musik::core::db::Row;
using musik::core::TrackPtr;

TracklistQuery::TracklistQuery() {
    result.reset(new std::vector<TrackPtr>());
}

TracklistQuery::~TracklistQuery() {

}

TracklistQuery::Result TracklistQuery::GetResult() {
    return this->result;
}

bool TracklistQuery::OnRun(Connection& db) {
    if (result) {
        result.reset(new std::vector<TrackPtr>());
    }

    std::string query =
        "SELECT DISTINCT albums.name "
        "FROM albums, tracks "
        "WHERE albums.id = tracks.album_id "
        "ORDER BY albums.sort_order;";

    Statement stmt(query.c_str(), db);

    //while (stmt.Step() == Row) {
    //    result->push_back(stmt.ColumnText(0));
    //}

    return true;
}