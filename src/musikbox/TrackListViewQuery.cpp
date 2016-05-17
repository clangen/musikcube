#pragma once

#include "stdafx.h"
#include "TrackListViewQuery.h"

#include <core/library/track/LibraryTrack.h>
#include <core/library/LocalLibraryConstants.h>
#include <core/db/Statement.h>

using musik::core::db::Statement;
using musik::core::db::Row;
using musik::core::TrackPtr;
using musik::core::LibraryTrack;
using namespace musik::core::library::constants;

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

    std::string query = boost::str(boost::format(
        "SELECT DISTINCT t.track, t.bpm, t.duration, t.filesize, t.year, t.title, t.filename, t.thumbnail_id, al.name AS album, gn.name AS genre, ar.name AS artist, t.filetime, t.sort_order1 " \
        "FROM tracks t, paths p, albums al, artists ar, genres gn " \
        "WHERE t.%s=? AND t.album_id=al.id AND t.visual_genre_id=gn.id AND t.visual_artist_id=ar.id") % this->column);
        
    Statement trackQuery(query.c_str(), db);

    trackQuery.BindInt(0, this->id);
    while (trackQuery.Step() == Row) {
        TrackPtr track = TrackPtr(new LibraryTrack(this->id, this->library));

        track->SetValue(Track::TRACK_NUM, trackQuery.ColumnText(0));
        track->SetValue(Track::BPM, trackQuery.ColumnText(1));
        track->SetValue(Track::DURATION, trackQuery.ColumnText(2));
        track->SetValue(Track::FILESIZE, trackQuery.ColumnText(3));
        track->SetValue(Track::YEAR, trackQuery.ColumnText(4));
        track->SetValue(Track::TITLE, trackQuery.ColumnText(5));
        track->SetValue(Track::FILENAME, trackQuery.ColumnText(6));
        track->SetValue(Track::THUMBNAIL_ID, trackQuery.ColumnText(7));
        track->SetValue(Track::ALBUM_ID, trackQuery.ColumnText(8));
        track->SetValue(Track::DISPLAY_GENRE_ID, trackQuery.ColumnText(9));
        track->SetValue(Track::DISPLAY_ARTIST_ID, trackQuery.ColumnText(10));
        track->SetValue(Track::FILETIME, trackQuery.ColumnText(11));

        result->push_back(track);
    }

    return true;
}