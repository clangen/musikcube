#pragma once

#include "stdafx.h"
#include "TrackListViewQuery.h"

#include <core/library/track/LibraryTrack.h>
#include <core/db/Statement.h>

using musik::core::db::Statement;
using musik::core::db::Row;
using musik::core::TrackPtr;
using musik::core::LibraryTrack;

TrackListViewQuery::TrackListViewQuery(LibraryPtr library, const std::string& column, DBID id) {
    this->library = library;
    this->column = column;
    this->id = id;
    this->result.reset(new std::vector<TrackPtr>());
}

TrackListViewQuery::~TrackListViewQuery() {

}

TrackListViewQuery::Result TrackListViewQuery::GetResult() {
    return this->result;
}

bool TrackListViewQuery::OnRun(Connection& db) {
    if (result) {
        result.reset(new std::vector<TrackPtr>());
    }

    std::string query = boost::str(
        boost::format(
            "SELECT DISTINCT tracks.id, tracks.track, tracks.title "
            "FROM tracks "
            "WHERE %1%=? "
            "ORDER BY tracks.track;") % this->column);

    Statement stmt(query.c_str(), db);
    stmt.BindInt(0, this->id);

    while (stmt.Step() == Row) {
        DBID id = stmt.ColumnInt64(0);
        TrackPtr track = TrackPtr(new LibraryTrack(id, this->library));
        track->SetValue("track", boost::lexical_cast<std::string>(stmt.ColumnInt(1)).c_str());
        track->SetValue("title", stmt.ColumnText(2));
        result->push_back(track);
    }

    return true;
}