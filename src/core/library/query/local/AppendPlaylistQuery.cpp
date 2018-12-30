//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004-2019 musikcube team
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
#include "AppendPlaylistQuery.h"
#include <core/library/track/LibraryTrack.h>
#include <core/library/query/local/TrackMetadataQuery.h>
#include <core/library/LocalLibraryConstants.h>
#include <core/runtime/Message.h>
#include <core/support/Messages.h>
#include <core/db/Statement.h>

using musik::core::db::Statement;
using musik::core::db::Row;

using namespace musik::core;
using namespace musik::core::db;
using namespace musik::core::db::local;
using namespace musik::core::library::constants;
using namespace musik::core::sdk;
using namespace musik::core::runtime;

static std::string INSERT_PLAYLIST_TRACK_QUERY =
    "INSERT INTO playlist_tracks (track_external_id, source_id, playlist_id, sort_order) "
    "VALUES (?, ?, ?, ?)";

static std::string UPDATE_OFFSET_QUERY =
    "UPDATE playlist_tracks SET offset = offset + ? WHERE playlist_id = ? AND offset >= ?";

static std::string GET_MAX_SORT_ORDER_QUERY =
    "SELECT COALESCE(MAX(sort_order), -1) from playlist_tracks where playlist_id = ?";

AppendPlaylistQuery::AppendPlaylistQuery(
    musik::core::ILibraryPtr library,
    const int64_t playlistId,
    std::shared_ptr<musik::core::TrackList> tracks,
    const int offset)
: library(library)
, sharedTracks(tracks)
, rawTracks(nullptr)
, playlistId(playlistId)
, offset(offset) {

}

AppendPlaylistQuery::AppendPlaylistQuery(
    musik::core::ILibraryPtr library,
    const int64_t playlistId,
    musik::core::sdk::ITrackList *tracks,
    const int offset)
: library(library)
, rawTracks(tracks)
, playlistId(playlistId)
, offset(offset) {

}

bool AppendPlaylistQuery::OnRun(musik::core::db::Connection &db) {
    ITrackList* tracks = sharedTracks ? sharedTracks.get() : rawTracks;

    if (!tracks || !tracks->Count() || playlistId == 0) {
        return true;
    }

    ScopedTransaction transaction(db);

    int offset = this->offset;

    if (offset < 0) {
        /* get the max offset, we're appending to the end and don't want
        to screw up our sort order! */
        Statement queryMax(GET_MAX_SORT_ORDER_QUERY.c_str(), db);
        queryMax.BindInt64(0, playlistId);
        if (queryMax.Step() == db::Row) {
            offset = queryMax.ColumnInt32(0) + 1;
        }
    }

    {
        Statement updateOffsets(UPDATE_OFFSET_QUERY.c_str(), db);
        updateOffsets.BindInt32(0, offset);
        updateOffsets.BindInt64(1, playlistId);
        updateOffsets.BindInt32(2, offset);

        if (updateOffsets.Step() == db::Error) {
            return false;
        }
    }

    Statement insertTrack(INSERT_PLAYLIST_TRACK_QUERY.c_str(), db);

    for (size_t i = 0; i < tracks->Count(); i++) {
        auto id = tracks->GetId(i);
        auto target = TrackPtr(new LibraryTrack(id, this->library));

        std::shared_ptr<TrackMetadataQuery> query(
            new TrackMetadataQuery(
                target,
                this->library,
                TrackMetadataQuery::IdsOnly));

        this->library->Enqueue(query, ILibrary::QuerySynchronous);

        if (query->GetStatus() == IQuery::Finished) {
            auto track = query->Result();
            insertTrack.Reset();
            insertTrack.BindText(0, track->GetString("external_id"));
            insertTrack.BindText(1, track->GetString("source_id"));
            insertTrack.BindInt64(2, playlistId);
            insertTrack.BindInt32(3, offset++);

            if (insertTrack.Step() == db::Error) {
                return false;
            }
        }
    }

    transaction.CommitAndRestart();

    this->library->GetMessageQueue().Broadcast(
        Message::Create(nullptr, message::PlaylistModified, playlistId));

    return true;
}