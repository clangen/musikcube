//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2007-2016 musikcube team
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
#include "GetPlaylistQuery.h"

#include <core/library/track/LibraryTrack.h>
#include <core/library/LocalLibraryConstants.h>
#include <core/db/Statement.h>

using musik::core::db::Statement;
using musik::core::db::Row;
using musik::core::TrackPtr;
using musik::core::LibraryTrack;
using musik::core::ILibraryPtr;

using namespace musik::core::db;
using namespace musik::core::library::constants;
using namespace musik::core::db::local;

GetPlaylistQuery::GetPlaylistQuery(ILibraryPtr library, DBID playlistId) {
    this->library = library;
    this->playlistId = playlistId;
    this->result.reset(new musik::core::TrackList(library));
    this->headers.reset(new std::set<size_t>());
    this->hash = std::hash<DBID>()(this->playlistId);
}

GetPlaylistQuery::~GetPlaylistQuery() {

}

GetPlaylistQuery::Result GetPlaylistQuery::GetResult() {
    return this->result;
}

GetPlaylistQuery::Headers GetPlaylistQuery::GetHeaders() {
    return this->headers;
}

size_t GetPlaylistQuery::GetQueryHash() {
    return this->hash;
}

bool GetPlaylistQuery::OnRun(Connection& db) {
    if (result) {
        result.reset(new musik::core::TrackList(this->library));
        headers.reset(new std::set<size_t>());
    }

    std::string query =
        "SELECT DISTINCT track_id " \
        "FROM playlist_tracks " \
        "WHERE playlist_id=? "
        "ORDER BY sort_order;";

    Statement trackQuery(query.c_str(), db);
    trackQuery.BindInt(0, this->playlistId);

    while (trackQuery.Step() == Row) {
        result->Add(trackQuery.ColumnInt64(0));
    }

    return true;
}
