//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004-2023 musikcube team
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

#include <musikcore/library/track/LibraryTrack.h>
#include <musikcore/library/query/TrackMetadataQuery.h>
#include <musikcore/library/query/util/Serialization.h>
#include <musikcore/library/LocalLibraryConstants.h>
#include <musikcore/db/ScopedTransaction.h>
#include <musikcore/db/Statement.h>
#include <common/runtime/Message.h>
#include <musikcore/support/Messages.h>

using namespace musik::core;
using namespace musik::core::sdk;
using namespace musik::core::db;
using namespace musik::core::library;
using namespace musik::core::library::query;
using namespace musik::core::library::query::serialization;
using namespace musik::core::runtime;

/* CONSTANTS */

const std::string SavePlaylistQuery::kQueryName = "SavePlaylistQuery";

static std::string CREATE_PLAYLIST_QUERY =
    "INSERT INTO playlists (name,source_id) VALUES (?,?);";

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

    result->op = Operation::Append;

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

    result->op = Operation::Append;

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
    this->tracks = tracks;
    this->op = Operation::Create;
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
    this->op = Operation::Create;
}

SavePlaylistQuery::SavePlaylistQuery(
    musik::core::ILibraryPtr library,
    const int64_t playlistId,
    std::shared_ptr<musik::core::TrackList> tracks)
{
    this->library = library;
    this->playlistId = playlistId;
    this->tracks = tracks;
    this->op = Operation::Replace;
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
    this->op = Operation::Append;
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
    this->op = Operation::Rename;
}

SavePlaylistQuery::SavePlaylistQuery(musik::core::ILibraryPtr library) {
    this->library = library;
    this->categoryId = -1;
}

/* METHODS */

int64_t SavePlaylistQuery::GetPlaylistId() const noexcept {
    return playlistId;
}

bool SavePlaylistQuery::AddTracksToPlaylist(
    musik::core::db::Connection &db,
    int64_t playlistId,
    const std::shared_ptr<TrackList>& tracks)
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
    for (size_t i = 0; tracks && i < tracks->Count(); i++) {
        track = tracks->Get(i);
        if (track) {
            insertTrack.Reset();
            insertTrack.BindText(0, track->GetString("external_id"));
            insertTrack.BindText(1, track->GetString("source_id"));
            insertTrack.BindInt64(2, playlistId);
            insertTrack.BindInt32(3, offset++);

            if (insertTrack.Step() != db::Done) {
                return false;
            }
        }
    }

    return true;
}

bool SavePlaylistQuery::AddCategoryTracksToPlaylist(
    musik::core::db::Connection &db, int64_t playlistId)
{
    auto query = std::make_shared<CategoryTrackListQuery>(library, categoryType, categoryId);

    this->library->EnqueueAndWait(query);

    if (query->GetStatus() == IQuery::Finished) {
        auto tracks = query->GetResult();
        if (this->AddTracksToPlaylist(db, playlistId, tracks)) {
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
    if (this->ownerSource) {
        createPlaylist.BindInt32(1, *this->ownerSource);
    }
    else {
        createPlaylist.BindNull(1);
    }

    if (createPlaylist.Step() != db::Done) {
        transaction.Cancel();
        return false;
    }

    this->playlistId = db.LastInsertedId();

    /* add tracks to playlist */
    if (this->tracks) {
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

    return true;
}

bool SavePlaylistQuery::RenamePlaylist(musik::core::db::Connection &db) {
    Statement renamePlaylist(RENAME_PLAYLIST_QUERY.c_str(), db);
    renamePlaylist.BindText(0, this->playlistName);
    renamePlaylist.BindInt64(1, this->playlistId);
    return (renamePlaylist.Step() == db::Done);
}

bool SavePlaylistQuery::ReplacePlaylist(musik::core::db::Connection &db) {
    ScopedTransaction transaction(db);

    /* delete existing tracks, we'll replace 'em */
    Statement createPlaylist(DELETE_PLAYLIST_TRACKS_QUERY.c_str(), db);
    createPlaylist.BindInt64(0, this->playlistId);

    if (createPlaylist.Step() != db::Done) {
        transaction.Cancel();
        return false;
    }

    /* add tracks to playlist */
    if (!this->AddTracksToPlaylist(db, playlistId, this->tracks)) {
        transaction.Cancel();
        return false;
    }

    return true;
}

bool SavePlaylistQuery::AppendToPlaylist(musik::core::db::Connection& db) {
    ScopedTransaction transaction(db);

    const bool result = this->tracks
        ? this->AddTracksToPlaylist(db, this->playlistId, this->tracks)
        : this->AddCategoryTracksToPlaylist(db, this->playlistId);

    if (!result) {
        transaction.Cancel();
    }

    return result;
}

std::shared_ptr<SavePlaylistQuery> SavePlaylistQuery::CreateOwned(ILibraryPtr library,
    const std::string& name, std::shared_ptr<TrackList> tracks, std::optional<int32_t> source)
{
    auto query = SavePlaylistQuery::Save(library, name, std::move(tracks));
    query->ownerSource = source;
    return query;
}

bool SavePlaylistQuery::ValidateMembership(Connection& db) {
    auto owner = this->ownerSource;
    if (this->op != Operation::Create) {
        Statement playlist("SELECT source_id FROM playlists WHERE id=?", db);
        playlist.BindInt64(0, this->playlistId);
        if (playlist.Step() != Row) {
            this->failure = MC_ERROR_NOT_FOUND;
            return false;
        }
        if (!playlist.IsNull(0)) {
            owner = playlist.ColumnInt32(0);
        }
    }
    if (!owner || this->op == Operation::Rename) {
        return true;
    }
    /* source-owned writes use explicit track identities; validate the entire
    batch before inserting a playlist or deleting its previous entries. */
    if (!this->tracks) {
        this->failure = MC_ERROR_INVALID_ARGUMENT;
        return false;
    }
    Statement track("SELECT source_id FROM tracks WHERE id=?", db);
    for (size_t i = 0; i < this->tracks->Count(); ++i) {
        track.ResetAndUnbind();
        track.BindInt64(0, this->tracks->GetId(i));
        if (track.Step() != Row || track.IsNull(0) || track.ColumnInt32(0) != *owner) {
            this->failure = MC_ERROR_INVALID_ARGUMENT;
            return false;
        }
    }
    return true;
}

bool SavePlaylistQuery::OnRun(Connection& db) {
    this->result = false;
    this->failure = MC_ERROR_IO;
    {
        ScopedTransaction transaction(db);
        if (!this->ValidateMembership(db)) {
            transaction.Cancel();
            /* the request was processed; its result describes the rejected
            mutation. transport failure remains a separate query failure. */
            return this->failure == MC_ERROR_INVALID_ARGUMENT || this->failure == MC_ERROR_NOT_FOUND;
        }
        switch (this->op) {
            case Operation::Rename:
                this->result = this->RenamePlaylist(db);
                break;
            case Operation::Replace:
                this->result = this->ReplacePlaylist(db);
                break;
            case Operation::Create:
                this->result = this->CreatePlaylist(db);
                break;
            case Operation::Append:
                this->result = this->AppendToPlaylist(db);
                break;
        }
        if (!this->result) {
            transaction.Cancel();
        }
    }
    if (this->result) {
        this->SendPlaylistMutationBroadcast();
    }
    return this->result;
}

void SavePlaylistQuery::SendPlaylistMutationBroadcast() {
    switch (this->op) {
        case Operation::Rename:
            this->library->GetMessageQueue().Broadcast(
                Message::Create(nullptr, message::PlaylistRenamed, playlistId));
            break;
        case Operation::Replace:
            this->library->GetMessageQueue().Broadcast(
                Message::Create(nullptr, message::PlaylistModified, playlistId));
            break;
        case Operation::Create:
            this->library->GetMessageQueue().Broadcast(
                Message::Create(nullptr, message::PlaylistCreated, playlistId));
            break;
        case Operation::Append:
            this->library->GetMessageQueue().Broadcast(
                Message::Create(nullptr, message::PlaylistModified, playlistId));
            break;
    }
}

/* ISerializableQuery */

std::string SavePlaylistQuery::SerializeQuery() {
    nlohmann::json tracksJson = tracks
        ? TrackListToJsonIdList(*tracks)
        : nlohmann::json();
    nlohmann::json output = {
        { "name", kQueryName },
        { "options", {
            { "op", this->op },
            { "playlistName", this->playlistName },
            { "categoryType", this->categoryType },
            { "playlistId", this->playlistId },
            { "categoryId", this->categoryId },
            { "tracks", tracksJson }
        }}
    };
    output["options"]["owner_source"] = this->ownerSource ? nlohmann::json(*this->ownerSource) : nlohmann::json();
    return output.dump();
}

std::string SavePlaylistQuery::SerializeResult() {
    nlohmann::json output = { { "result", this->result }, { "playlistId", this->playlistId }, { "failure", this->failure } };
    return output.dump();
}

void SavePlaylistQuery::DeserializeResult(const std::string& data) {
    auto input = nlohmann::json::parse(data);
    this->result = input["result"].get<bool>();
    this->failure = input.value("failure", MC_ERROR_IO);
    this->playlistId = input.value("playlistId", this->playlistId);
    const bool processed = result || this->failure == MC_ERROR_INVALID_ARGUMENT || this->failure == MC_ERROR_NOT_FOUND;
    this->SetStatus(processed ? IQuery::Finished : IQuery::Failed);
    if (result) {
        SendPlaylistMutationBroadcast();
    }
}

std::shared_ptr<SavePlaylistQuery> SavePlaylistQuery::DeserializeQuery(
    musik::core::ILibraryPtr library, const std::string& data)
{
    auto options = nlohmann::json::parse(data)["options"];
    auto output = std::shared_ptr<SavePlaylistQuery>(new SavePlaylistQuery(library));
    const auto& operation = options.at("op");
    if (!operation.is_number_integer() || operation < static_cast<int>(Operation::Create) ||
        operation > static_cast<int>(Operation::Append))
    {
        throw std::invalid_argument("invalid playlist operation");
    }
    output->op = operation.get<Operation>();
    if (options.contains("owner_source") && !options["owner_source"].is_null()) {
        if (!options["owner_source"].is_number_integer()) {
            throw std::invalid_argument("playlist source ID must be an integer");
        }
        const auto id = options["owner_source"].get<int64_t>();
        if (output->op != Operation::Create || id < 0 || id > INT32_MAX) {
            throw std::invalid_argument("invalid playlist owner");
        }
        output->ownerSource = static_cast<int32_t>(id);
    }
    output->playlistName = options["playlistName"].get<std::string>();
    output->categoryType = options["categoryType"].get<std::string>();
    output->playlistId = options["playlistId"].get<int64_t>();
    output->categoryId = options["categoryId"].get<int64_t>();
    output->tracks = std::make_shared<TrackList>(library);
    TrackListFromJson(options["tracks"], *output->tracks, library, true);
    return output;
}
