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

#include "stdafx.h"
#include "SearchTrackListQuery.h"

#include <core/library/track/LibraryTrack.h>
#include <core/library/LocalLibraryConstants.h>
#include <core/db/Statement.h>

#include <boost/algorithm/string/case_conv.hpp>

using musik::core::db::Statement;
using musik::core::db::Row;
using musik::core::TrackPtr;
using musik::core::LibraryTrack;
using musik::core::LibraryPtr;

using namespace musik::core::db;
using namespace musik::core::library::constants;
using namespace musik::box;

SearchTrackListQuery::SearchTrackListQuery(LibraryPtr library, const std::string& filter) {
    this->library = library;
    this->filter = "%" + boost::algorithm::to_lower_copy(filter) + "%";
    this->result.reset(new TrackList(library));
    this->headers.reset(new std::set<size_t>());
    this->hash = 0;
}

SearchTrackListQuery::~SearchTrackListQuery() {
}

SearchTrackListQuery::Result SearchTrackListQuery::GetResult() {
    return this->result;
}

SearchTrackListQuery::Headers SearchTrackListQuery::GetHeaders() {
    return this->headers;
}

size_t SearchTrackListQuery::GetQueryHash() {
    this->hash = std::hash<std::string>()(this->filter);
    return this->hash;
}

bool SearchTrackListQuery::OnRun(Connection& db) {
    if (result) {
        result.reset(new TrackList(this->library));
        headers.reset(new std::set<size_t>());
    }

    std::string lastAlbum;
    size_t index = 0;

    std::string query =
        "SELECT DISTINCT t.id, al.name " \
        "FROM tracks t, albums al, artists ar, genres gn " \
        "WHERE "
            "(t.title LIKE ? OR al.name LIKE ? OR ar.name LIKE ? OR gn.name LIKE ?) "
            " AND t.album_id=al.id AND t.visual_genre_id=gn.id AND t.visual_artist_id=ar.id "
        "ORDER BY al.name, disc, track, ar.name";

    Statement trackQuery(query.c_str(), db);
    trackQuery.BindText(0, this->filter);
    trackQuery.BindText(1, this->filter);
    trackQuery.BindText(2, this->filter);
    trackQuery.BindText(3, this->filter);

    while (trackQuery.Step() == Row) {
        DBID id = trackQuery.ColumnInt64(0);
        std::string album = trackQuery.ColumnText(1);

        if (album != lastAlbum) {
            headers->insert(index);
            lastAlbum = album;
        }

        result->Add(id);
        ++index;
    }

    return true;
}
