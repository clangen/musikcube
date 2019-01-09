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
#include "SavePlaylistQuery.h"
#include "CategoryTrackListQuery.h"

#include <core/library/track/LibraryTrack.h>
#include <core/library/query/local/TrackMetadataQuery.h>
#include <core/db/ScopedTransaction.h>
#include <core/db/Statement.h>
#include <core/runtime/Message.h>
#include <core/support/Messages.h>

using namespace musik::core;
using namespace musik::core::db;
using namespace musik::core::db::local;
using namespace musik::core::runtime;

/* CONSTANTS */

static std::string CREATE_PLAYLIST_QUERY =
    "INSERT INTO playlists (name) VALUES (?);";

static std::string INSERT_PLAYLIST_TRACK_QUERY =
    "INSERT INTO playlist_tracks (track_external_id, source_id, playlist_id, sort_order) VALUES (?, ?, ?, ?);";

static std::string DELETE_PLAYLIST_TRACKS_QUERY =
    "DELETE FROM playlist_tracks WHERE playlist_id=?;";

static std::string RENAME_PLAYLIST_QUERY =
    "UPDATE playlists SET name=? WHERE id=?";

static std::string GET_MAX_SORT_ORDER_QUERY =
    "SELECT MAX(sort_order) from playlist_tracks where playlist_id = ?";

/* STATIC FACTORY METHODS */

std::shared_ptr<SavePlaylistQuery> SavePlaylistQuery::Save(
    musik::core::ILibraryPtr library,
    const std::string& playlistName,
    std::shared_ptr<musik::core::TrackList> tracks)
{
    return std::shared_ptr<SavePlaylistQuery>(
        new SavePlaylistQuery(library, playlistName, tracks));
}

std::shared_ptr<SavePlaylistQuery> SavePlaylistQuery::Save(
    musik::core::ILibraryPtr library,
    const std::string& playlistName,
    musik::core::sdk::ITrackList* tracks)
{
    return std::shared_ptr<SavePlaylistQuery>(
        new SavePlaylistQuery(library, playlistName, tracks));
}

std::shared_ptr<SavePlaylistQuery> SavePlaylistQuery::Save(
    musik::core::ILibraryPtr library,
    const std::string& playlistName,
    const std::string& categoryType,
    int64_t categoryId)
{
    return std::shared_ptr<SavePlaylistQuery>(
        new SavePlaylistQuery(library, playlistName, categoryType, categoryId));
}

std::shared_ptr<SavePlaylistQuery> SavePlaylistQuery::Replace(
    musik::core::ILibraryPtr library,
    const int64_t playlistId,
    std::shared_ptr<musik::core::TrackList> tracks)
{
    return std::shared_ptr<SavePlaylistQuery>(
        new SavePlaylistQuery(library, playlistId, tracks));
}

std::shared_ptr<SavePlaylistQuery> SavePlaylistQuery::Replace(
    musik::core::ILibraryPtr library,
    const int64_t playlistId,
    musik::core::sdk::ITrackList* tracks)
{
    return std::shared_ptr<SavePlaylistQuery>(
        new SavePlaylistQuery(library, playlistId, tracks));
}

std::shared_ptr<SavePlaylistQuery> SavePlaylistQuery::Rename(
    musik::core::ILibraryPtr library,
    const int64_t playlistId,
    const std::string& playlistName)
{
    return std::shared_ptr<SavePlaylistQuery>(
        new SavePlaylistQuery(library, playlistId, playlistName));
}

std::shared_ptr<SavePlaylistQuery> SavePlaylistQuery::Append(
    musik::core::ILibraryPtr library,
    const int64_t playlistId,
    std::shared_ptr<musik::core::TrackList> tracks)
{
    auto result = std::shared_ptr<SavePlaylistQuery>(
        new SavePlaylistQuery(library, playlistId, tracks));

    result->op = AppendOp;

    return result;
}

/* CONSTRUCTORS */

std::shared_ptr<SavePlaylistQuery> SavePlaylistQuery::Append(
    musik::core::ILibraryPtr library,
    const int64_t playlistId,
    const std::string& categoryType,
    int64_t categoryId)
{
    auto result = std::shared_ptr<SavePlaylistQuery>(
        new SavePlaylistQuery(library, playlistId, categoryType, categoryId));

    result->op = AppendOp;

    return result;
}

SavePlaylistQuery::SavePlaylistQuery(
    musik::core::ILibraryPtr library,
    const std::string& playlistName,
    std::shared_ptr<musik::core::TrackList> tracks)
{
    this->library = library;
    this->playlistId = -1;
    this->categoryId = -1;
    this->playlistName = playlistName;
    this->tracks.rawTracks = nullptr;
    this->tracks.sharedTracks = tracks;
    this->op = CreateOp;
}

SavePlaylistQuery::SavePlaylistQuery(
    musik::core::ILibraryPtr library,
    const std::string& playlistName,
    musik::core::sdk::ITrackList* tracks)
{
    this->library = library;
    this->playlistId = -1;
    this->categoryId = -1;
    this->playlistName = playlistName;
    this->tracks.rawTracks = tracks;
    this->op = CreateOp;
}

SavePlaylistQuery::SavePlaylistQuery(
    musik::core::ILibraryPtr library,
    const std::string& playlistName,
    const std::string& categoryType,
    int64_t categoryId)
{
    this->library = library;
    this->playlistId = -1;
    this->categoryId = categoryId;
    this->categoryType = categoryType;
    this->playlistName = playlistName;
    this->op = CreateOp;
}

SavePlaylistQuery::SavePlaylistQuery(
    musik::core::ILibraryPtr library,
    const int64_t playlistId,
    std::shared_ptr<musik::core::TrackList> tracks)
{
    this->library = library;
    this->playlistId = playlistId;
    this->tracks.sharedTracks = tracks;
    this->op = ReplaceOp;
}

SavePlaylistQuery::SavePlaylistQuery(
    musik::core::ILibraryPtr library,
    const int64_t playlistId,
    musik::core::sdk::ITrackList* tracks)
{
    this->library = library;
    this->playlistId = playlistId;
    this->tracks.rawTracks = tracks;
    this->op = ReplaceOp;
}

SavePlaylistQuery::SavePlaylistQuery(
    musik::core::ILibraryPtr library,
    const int64_t playlistId,
    const std::string& categoryType,
    int64_t categoryId)
{
    this->library = library;
    this->playlistId = playlistId;
    this->categoryId = categoryId;
    this->categoryType = categoryType;
    this->op = AppendOp;
}

SavePlaylistQuery::SavePlaylistQuery(
    musik::core::ILibraryPtr library,
    const int64_t playlistId,
    const std::string& playlistName)
{
    this->library = library;
    this->categoryId = -1;
    this->playlistId = playlistId;
    this->playlistName = playlistName;
    this->op = RenameOp;
}

SavePlaylistQuery::~SavePlaylistQuery() {
}

/* METHODS */

int64_t SavePlaylistQuery::GetPlaylistId() const {
    return playlistId;
}

bool SavePlaylistQuery::AddTracksToPlaylist(
    musik::core::db::Connection &db,
    int64_t playlistId,
    TrackListWrapper& tracks)
{
    int offset = 0;

    /* get the max offset, we're appending to the end and don't want
    to screw up our sort order! */
    Statement queryMax(GET_MAX_SORT_ORDER_QUERY.c_str(), db);
    queryMax.BindInt64(0, playlistId);
    if (queryMax.Step() == db::Row) {
        offset = queryMax.ColumnInt32(0) + 1;
    }

    /* insert all the tracks. */
    Statement insertTrack(INSERT_PLAYLIST_TRACK_QUERY.c_str(), db);

    TrackPtr track;
    for (size_t i = 0; i < tracks.Count(); i++) {
        track = tracks.Get(this->library, i);
        if (track) {
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

    return true;
}

bool SavePlaylistQuery::AddCategoryTracksToPlaylist(
    musik::core::db::Connection &db, int64_t playlistId)
{
    auto query = std::shared_ptr<CategoryTrackListQuery>(
        new CategoryTrackListQuery(library, categoryType, categoryId));

    this->library->Enqueue(query, ILibrary::QuerySynchronous);

    if (query->GetStatus() == IQuery::Finished) {
        auto tracks = query->GetResult();
        TrackListWrapper wrapper(tracks);
        if (this->AddTracksToPlaylist(db, playlistId, wrapper)) {
            return true;
        }
    }

    return false;
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

    this->playlistId = db.LastInsertedId();

    /* add tracks to playlist */
    if (this->tracks.Exists()) {
        if (!this->AddTracksToPlaylist(db, this->playlistId, this->tracks)) {
            transaction.Cancel();
            return false;
        }
    }
    else {
        if (!this->AddCategoryTracksToPlaylist(db, this->playlistId)) {
            transaction.Cancel();
            return false;
        }
    }

    this->library->GetMessageQueue().Broadcast(
        Message::Create(nullptr, message::PlaylistCreated, playlistId));

    return true;
}

bool SavePlaylistQuery::RenamePlaylist(musik::core::db::Connection &db) {
    Statement renamePlaylist(RENAME_PLAYLIST_QUERY.c_str(), db);
    renamePlaylist.BindText(0, this->playlistName);
    renamePlaylist.BindInt64(1, this->playlistId);
    bool result = (renamePlaylist.Step() != db::Error);

    if (result) {
        this->library->GetMessageQueue().Broadcast(
            Message::Create(nullptr, message::PlaylistRenamed, playlistId));
    }

    return result;
}

bool SavePlaylistQuery::ReplacePlaylist(musik::core::db::Connection &db) {
    ScopedTransaction transaction(db);

    /* delete existing tracks, we'll replace 'em */
    Statement createPlaylist(DELETE_PLAYLIST_TRACKS_QUERY.c_str(), db);
    createPlaylist.BindInt64(0, this->playlistId);

    if (createPlaylist.Step() == db::Error) {
        transaction.Cancel();
        return false;
    }

    /* add tracks to playlist */
    if (!this->AddTracksToPlaylist(db, playlistId, this->tracks)) {
        transaction.Cancel();
        return false;
    }

    this->library->GetMessageQueue().Broadcast(
        Message::Create(nullptr, message::PlaylistModified, playlistId));

    return true;
}

bool SavePlaylistQuery::AppendToPlaylist(musik::core::db::Connection& db) {
    ScopedTransaction transaction(db);

    bool result = this->tracks.Exists()
        ? this->AddTracksToPlaylist(db, this->playlistId, this->tracks)
        : this->AddCategoryTracksToPlaylist(db, this->playlistId);

    if (!result) {
        transaction.Cancel();
    }

    this->library->GetMessageQueue().Broadcast(
        Message::Create(nullptr, message::PlaylistModified, playlistId));

    return result;
}

bool SavePlaylistQuery::OnRun(musik::core::db::Connection &db) {
    switch (this->op) {
        case RenameOp: return this->RenamePlaylist(db);
        case ReplaceOp: return this->ReplacePlaylist(db);
        case CreateOp: return this->CreatePlaylist(db);
        case AppendOp: return this->AppendToPlaylist(db);
    }
    return false;
}

/* SUPPORTING TYPES */

SavePlaylistQuery::TrackListWrapper::TrackListWrapper() {
    this->rawTracks = nullptr;
}

SavePlaylistQuery::TrackListWrapper::TrackListWrapper(
    std::shared_ptr<musik::core::TrackList> shared)
{
    this->rawTracks = nullptr;
    this->sharedTracks = shared;
}

bool SavePlaylistQuery::TrackListWrapper::Exists() {
    return this->sharedTracks || this->rawTracks;
}

size_t SavePlaylistQuery::TrackListWrapper::Count() {
    if (sharedTracks) {
        return sharedTracks->Count();
    }

    return rawTracks ? rawTracks->Count() : 0;
}

TrackPtr SavePlaylistQuery::TrackListWrapper::Get(
    musik::core::ILibraryPtr library, size_t index)
{
    if (sharedTracks) {
        return sharedTracks->Get(index);
    }

    TrackPtr result = std::make_shared<LibraryTrack>(rawTracks->GetId(index), library);
    if (rawTracks) {
        std::shared_ptr<TrackMetadataQuery> query(
            new TrackMetadataQuery(result, library, TrackMetadataQuery::IdsOnly));

        library->Enqueue(query, ILibrary::QuerySynchronous);
    }

    return result;
}
