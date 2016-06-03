#include "stdafx.h"
#include "NowPlayingTrackListQuery.h"

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

NowPlayingTrackListQuery::NowPlayingTrackListQuery(PlaybackService& playback)
: playback(playback) {
    this->result.reset(new std::vector<TrackPtr>());
    this->headers.reset(new std::set<size_t>());
    this->hash = 0;
}

NowPlayingTrackListQuery::~NowPlayingTrackListQuery() {

}

NowPlayingTrackListQuery::Result NowPlayingTrackListQuery::GetResult() {
    return this->result;
}

NowPlayingTrackListQuery::Headers NowPlayingTrackListQuery::GetHeaders() {
    return this->headers;
}

size_t NowPlayingTrackListQuery::GetQueryHash() {
    if (this->hash == 0) {
        this->hash = std::hash<std::string>()(this->Name());
    }

    return this->hash;
}

bool NowPlayingTrackListQuery::OnRun(Connection& db) {
    if (result) {
        result.reset(new std::vector<TrackPtr>());
        headers.reset(new std::set<size_t>());
    }

    this->playback.Copy(*result);

    return true;
}
