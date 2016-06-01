#include "stdafx.h"
#include "TrackListViewQuery.h"

#include <core/library/track/LibraryTrack.h>
#include <core/library/LocalLibraryConstants.h>
#include <core/db/Statement.h>

using musik::core::db::Statement;
using musik::core::db::Row;
using musik::core::TrackPtr;
using musik::core::LibraryTrack;
using musik::core::LibraryPtr;

using namespace musik::core::db;
using namespace musik::core::library::constants;
using namespace musik::box;

TrackListViewQuery::TrackListViewQuery(LibraryPtr library, const std::string& column, DBID id) {
    this->library = library;
    this->column = column;
    this->id = id;
    this->result.reset(new std::vector<TrackPtr>());
    this->headers.reset(new std::set<size_t>());
}

TrackListViewQuery::~TrackListViewQuery() {

}

TrackListViewQuery::Result TrackListViewQuery::GetResult() {
    return this->result;
}

TrackListViewQuery::Headers TrackListViewQuery::GetHeaders() {
    return this->headers;
}

bool TrackListViewQuery::OnRun(Connection& db) {
    if (result) {
        result.reset(new std::vector<TrackPtr>());
        headers.reset(new std::set<size_t>());
    }

    std::string query = boost::str(boost::format(
        "SELECT DISTINCT t.track, t.bpm, t.duration, t.filesize, t.year, t.title, t.filename, t.thumbnail_id, al.name AS album, gn.name AS genre, ar.name AS artist, t.filetime " \
        "FROM tracks t, paths p, albums al, artists ar, genres gn " \
        "WHERE t.%s=? AND t.album_id=al.id AND t.visual_genre_id=gn.id AND t.visual_artist_id=ar.id "
        "ORDER BY album, track, artist") % this->column);

    std::string lastAlbum;
    size_t index = 0;

    Statement trackQuery(query.c_str(), db);

    trackQuery.BindInt(0, this->id);
    while (trackQuery.Step() == Row) {
        TrackPtr track = TrackPtr(new LibraryTrack(this->id, this->library));
        std::string album = trackQuery.ColumnText(8);

        if (album != lastAlbum) {
            headers->insert(index);
            lastAlbum = album;
        }

        track->SetValue(Track::TRACK_NUM, trackQuery.ColumnText(0));
        track->SetValue(Track::BPM, trackQuery.ColumnText(1));
        track->SetValue(Track::DURATION, trackQuery.ColumnText(2));
        track->SetValue(Track::FILESIZE, trackQuery.ColumnText(3));
        track->SetValue(Track::YEAR, trackQuery.ColumnText(4));
        track->SetValue(Track::TITLE, trackQuery.ColumnText(5));
        track->SetValue(Track::FILENAME, trackQuery.ColumnText(6));
        track->SetValue(Track::THUMBNAIL_ID, trackQuery.ColumnText(7));
        track->SetValue(Track::ALBUM_ID, album.c_str());
        track->SetValue(Track::GENRE_ID, trackQuery.ColumnText(9));
        track->SetValue(Track::ARTIST_ID, trackQuery.ColumnText(10));
        track->SetValue(Track::FILETIME, trackQuery.ColumnText(11));

        result->push_back(track);
        ++index;
    }

    return true;
}
