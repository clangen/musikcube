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
#include "DeletePlaylistQuery.h"

#include <core/db/ScopedTransaction.h>
#include <core/db/Statement.h>
#include <core/support/Messages.h>
#include <core/runtime/Message.h>

using namespace musik::core;
using namespace musik::core::db;
using namespace musik::core::db::local;
using namespace musik::core::runtime;

static std::string DELETE_PLAYLIST_TRACKS_QUERY =
    "DELETE FROM playlist_tracks WHERE playlist_id=?;";

static std::string DELETE_PLAYLIST_QUERY =
    "DELETE FROM playlists WHERE id=?;";

DeletePlaylistQuery::DeletePlaylistQuery(musik::core::ILibraryPtr library, int64_t playlistId) {
    this->library = library;
    this->playlistId = playlistId;
}

DeletePlaylistQuery::~DeletePlaylistQuery() {
}

bool DeletePlaylistQuery::OnRun(musik::core::db::Connection &db) {
    ScopedTransaction transaction(db);

    /* delete the tracks */
    Statement deleteTracks(DELETE_PLAYLIST_TRACKS_QUERY.c_str(), db);
    deleteTracks.BindInt64(0, this->playlistId);

    if (deleteTracks.Step() == db::Error) {
        transaction.Cancel();
        return false;
    }

    /* delete the container */
    Statement deletePlaylist(DELETE_PLAYLIST_QUERY.c_str(), db);
    deletePlaylist.BindInt64(0, this->playlistId);

    if (deletePlaylist.Step() == db::Error) {
        transaction.Cancel();
        return false;
    }

    this->library->GetMessageQueue().Broadcast(
        Message::Create(nullptr, message::PlaylistDeleted, playlistId));

    return true;
}