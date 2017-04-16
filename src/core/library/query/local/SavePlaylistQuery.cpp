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
#include "SavePlaylistQuery.h"

#include <core/db/ScopedTransaction.h>
#include <core/db/Statement.h>

using namespace musik::core;
using namespace musik::core::db;
using namespace musik::core::db::local;

static std::string CREATE_PLAYLIST_QUERY =
    "INSERT INTO playlists (name) VALUES (?);";

static std::string INSERT_PLAYLIST_TRACK_QUERY =
    "INSERT INTO playlist_tracks (track_external_id, source_id, playlist_id, sort_order) VALUES (?, ?, ?, ?);";

static std::string DELETE_PLAYLIST_TRACKS_QUERY =
    "DELETE FROM playlist_tracks WHERE playlist_id=?;";

static std::string RENAME_PLAYLIST_QUERY =
    "UPDATE playlists SET name=? WHERE id=?";

std::shared_ptr<SavePlaylistQuery> SavePlaylistQuery::Save(
    const std::string& playlistName,
    std::shared_ptr<musik::core::TrackList> tracks)
{
    return std::shared_ptr<SavePlaylistQuery>(
        new SavePlaylistQuery(playlistName, tracks));
}

std::shared_ptr<SavePlaylistQuery> SavePlaylistQuery::Replace(
    const DBID playlistId,
    std::shared_ptr<musik::core::TrackList> tracks)
{
    return std::shared_ptr<SavePlaylistQuery>(
        new SavePlaylistQuery(playlistId, tracks));
}

std::shared_ptr<SavePlaylistQuery> SavePlaylistQuery::Rename(
    const DBID playlistId,
    const std::string& playlistName)
{
    return std::shared_ptr<SavePlaylistQuery>(
        new SavePlaylistQuery(playlistId, playlistName));
}

SavePlaylistQuery::SavePlaylistQuery(
    const std::string& playlistName,
    std::shared_ptr<musik::core::TrackList> tracks)
{
    this->playlistId = -1;
    this->playlistName = playlistName;
    this->tracks = tracks;
}

SavePlaylistQuery::SavePlaylistQuery(
    const DBID playlistId,
    std::shared_ptr<musik::core::TrackList> tracks)
{
    this->playlistId = playlistId;
    this->tracks = tracks;
}

SavePlaylistQuery::SavePlaylistQuery(
    const DBID playlistId,
    const std::string& playlistName)
{
    this->playlistId = playlistId;
    this->playlistName = playlistName;
}

SavePlaylistQuery::~SavePlaylistQuery() {
}

bool SavePlaylistQuery::AddTracksToPlaylist(musik::core::db::Connection &db, DBID playlistId) {
    Statement insertTrack(INSERT_PLAYLIST_TRACK_QUERY.c_str(), db);

    TrackPtr track;
    TrackList& tracks = *this->tracks;
    for (size_t i = 0; i < tracks.Count(); i++) {
        track = tracks.Get(i);
        insertTrack.Reset();
        insertTrack.BindText(0, track->GetValue("external_id"));
        insertTrack.BindText(1, track->GetValue("source_id"));
        insertTrack.BindInt(2, playlistId);
        insertTrack.BindInt(3, (int) i);

        if (insertTrack.Step() == db::Error) {
            return false;
        }
    }

    return true;
}

bool SavePlaylistQuery::CreatePlaylist(musik::core::db::Connection &db) {
    ScopedTransaction transaction(db);

    /* create playlist */
    Statement createPlaylist(CREATE_PLAYLIST_QUERY.c_str(), db);
    createPlaylist.BindText(0, this->playlistName);

    if (createPlaylist.Step() == db::Error) {
        transaction.Cancel();
        return false;
    }

    DBID playlistId = db.LastInsertedId();

    /* add tracks to playlist */
    if (!this->AddTracksToPlaylist(db, playlistId)) {
        transaction.Cancel();
        return false;
    }

    return true;
}

bool SavePlaylistQuery::RenamePlaylist(musik::core::db::Connection &db) {
    Statement renamePlaylist(RENAME_PLAYLIST_QUERY.c_str(), db);
    renamePlaylist.BindText(0, this->playlistName);
    renamePlaylist.BindInt(1, this->playlistId);
    return (renamePlaylist.Step() != db::Error);
}

bool SavePlaylistQuery::ReplacePlaylist(musik::core::db::Connection &db) {
    ScopedTransaction transaction(db);

    /* delete existing tracks, we'll replace 'em */
    Statement createPlaylist(DELETE_PLAYLIST_TRACKS_QUERY.c_str(), db);
    createPlaylist.BindInt(0, this->playlistId);

    if (createPlaylist.Step() == db::Error) {
        transaction.Cancel();
        return false;
    }

    /* add tracks to playlist */
    if (!this->AddTracksToPlaylist(db, playlistId)) {
        transaction.Cancel();
        return false;
    }

    return true;
}

bool SavePlaylistQuery::OnRun(musik::core::db::Connection &db) {
    if (playlistName.size() && playlistId != -1) {
        return this->RenamePlaylist(db);
    }
    else if (playlistId != -1) {
        return this->ReplacePlaylist(db);
    }

    return this->CreatePlaylist(db);
}
