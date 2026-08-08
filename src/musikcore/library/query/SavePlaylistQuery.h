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

#pragma once

/** @file SavePlaylistQuery.h
 *  @brief Query that creates, renames, replaces or appends to playlists.
 *  @details Supports saving a playlist from a TrackList, an SDK ITrackList, or
 *      all tracks in a category. The operation is selected by the static factory
 *      used (Save, Replace, Rename, Append). */

#include <musikcore/library/QueryBase.h>
#include <musikcore/library/track/TrackList.h>
#include <musikcore/db/Connection.h>
#include <musikcore/library/ILibrary.h>
#include <memory>

/** @namespace musik::core::library::query
 *  @brief Query classes and helpers executed against a library. */
namespace musik { namespace core { namespace library { namespace query {

    /** @brief Mutates playlists (create/replace/rename/append).
     *  @details Use the static Save(), Replace(), Rename() and Append() factories
     *      to build the desired operation. A mutation broadcast is sent on success. */
    class SavePlaylistQuery : public musik::core::library::query::QueryBase {
        public:
            static const std::string kQueryName; /**< Query type name. */

            /** @brief Creates a query that saves a new playlist from a TrackList.
             *  @param library The library to mutate.
             *  @param playlistName The new playlist name.
             *  @param tracks The tracks to include.
             *  @return A shared query. */
            static std::shared_ptr<SavePlaylistQuery> Save(
                musik::core::ILibraryPtr library,
                const std::string& playlistName,
                std::shared_ptr<musik::core::TrackList> tracks);

            /** @brief Creates a query that saves a new playlist from an SDK track list.
             *  @param library The library to mutate.
             *  @param playlistName The new playlist name.
             *  @param tracks The tracks to include.
             *  @return A shared query. */
            static std::shared_ptr<SavePlaylistQuery> Save(
                musik::core::ILibraryPtr library,
                const std::string& playlistName,
                musik::core::sdk::ITrackList* tracks);

            /** @brief Creates a query that saves a new playlist from a category.
             *  @param library The library to mutate.
             *  @param playlistName The new playlist name.
             *  @param categoryType The category type (e.g. "artist").
             *  @param categoryId The category value id.
             *  @return A shared query. */
            static std::shared_ptr<SavePlaylistQuery> Save(
                musik::core::ILibraryPtr library,
                const std::string& playlistName,
                const std::string& categoryType,
                int64_t categoryId);

            /** @brief Creates a query that replaces an existing playlist from a TrackList.
             *  @param library The library to mutate.
             *  @param playlistId The playlist to replace.
             *  @param tracks The replacement tracks.
             *  @return A shared query. */
            static std::shared_ptr<SavePlaylistQuery> Replace(
                musik::core::ILibraryPtr library,
                const int64_t playlistId,
                std::shared_ptr<musik::core::TrackList> tracks);

            /** @brief Creates a query that replaces an existing playlist from an SDK track list.
             *  @param library The library to mutate.
             *  @param playlistId The playlist to replace.
             *  @param tracks The replacement tracks.
             *  @return A shared query. */
            static std::shared_ptr<SavePlaylistQuery> Replace(
                musik::core::ILibraryPtr library,
                const int64_t playlistId,
                musik::core::sdk::ITrackList* tracks);

            /** @brief Creates a query that renames a playlist.
             *  @param library The library to mutate.
             *  @param playlistId The playlist to rename.
             *  @param playlistName The new name.
             *  @return A shared query. */
            static std::shared_ptr<SavePlaylistQuery> Rename(
                musik::core::ILibraryPtr library,
                const int64_t playlistId,
                const std::string& playlistName);

            /** @brief Creates a query that appends tracks to a playlist.
             *  @param library The library to mutate.
             *  @param playlistId The playlist to append to.
             *  @param tracks The tracks to append.
             *  @return A shared query. */
            static std::shared_ptr<SavePlaylistQuery> Append(
                musik::core::ILibraryPtr library,
                const int64_t playlistId,
                std::shared_ptr<musik::core::TrackList> tracks);

            /** @brief Creates a query that appends a category's tracks to a playlist.
             *  @param library The library to mutate.
             *  @param playlistId The playlist to append to.
             *  @param categoryType The category type (e.g. "artist").
             *  @param categoryId The category value id.
             *  @return A shared query. */
            static std::shared_ptr<SavePlaylistQuery> Append(
                musik::core::ILibraryPtr library,
                const int64_t playlistId,
                const std::string& categoryType,
                int64_t categoryId);

            DELETE_CLASS_DEFAULTS(SavePlaylistQuery)

            /** @return The id of the affected playlist (for create operations). */
            int64_t GetPlaylistId() const noexcept;

            /* IQuery */
            /** @return The query type name. */
            std::string Name() override { return kQueryName; }

            /* ISerializableQuery */
            /** @return The serialized query parameters. */
            std::string SerializeQuery() override;
            /** @return The serialized result. */
            std::string SerializeResult() override;
            /** @brief Populates the result from serialized data.
             *  @param data The serialized result. */
            void DeserializeResult(const std::string& data) override;
            /** @brief Recreates a query from serialized parameters.
             *  @param library The library the query will run on.
             *  @param data The serialized query.
             *  @return The deserialized query. */
            static std::shared_ptr<SavePlaylistQuery> DeserializeQuery(
                musik::core::ILibraryPtr library, const std::string& data);

        protected:
            /* QueryBase */
            /** @brief Runs the query against the database.
             *  @param db The connection to run on.
             *  @return true on success. */
            bool OnRun(musik::core::db::Connection &db) override;

        private:
            /** @brief Creates a save-from-TrackList query.
             *  @param library The library to mutate.
             *  @param playlistName The new playlist name.
             *  @param tracks The tracks to include. */
            SavePlaylistQuery(
                musik::core::ILibraryPtr library,
                const std::string& playlistName,
                std::shared_ptr<musik::core::TrackList> tracks);

            /** @brief Creates a save-from-SDK-list query.
             *  @param library The library to mutate.
             *  @param playlistName The new playlist name.
             *  @param tracks The tracks to include. */
            SavePlaylistQuery(
                musik::core::ILibraryPtr library,
                const std::string& playlistName,
                musik::core::sdk::ITrackList* tracks);

            /** @brief Creates a save-from-category query.
             *  @param library The library to mutate.
             *  @param playlistName The new playlist name.
             *  @param categoryType The category type.
             *  @param categoryId The category value id. */
            SavePlaylistQuery(
                musik::core::ILibraryPtr library,
                const std::string& playlistName,
                const std::string& categoryType,
                int64_t categoryId);

            /** @brief Creates a replace-from-TrackList query.
             *  @param library The library to mutate.
             *  @param playlistId The playlist to replace.
             *  @param tracks The replacement tracks. */
            SavePlaylistQuery(
                musik::core::ILibraryPtr library,
                const int64_t playlistId,
                std::shared_ptr<musik::core::TrackList> tracks);

            /** @brief Creates a replace-from-SDK-list query.
             *  @param library The library to mutate.
             *  @param playlistId The playlist to replace.
             *  @param tracks The replacement tracks. */
            SavePlaylistQuery(
                musik::core::ILibraryPtr library,
                const int64_t playlistId,
                musik::core::sdk::ITrackList* tracks);

            /** @brief Creates a save-from-category query targeting a playlist.
             *  @param library The library to mutate.
             *  @param playlistId The target playlist id.
             *  @param categoryType The category type.
             *  @param categoryId The category value id. */
            SavePlaylistQuery(
                musik::core::ILibraryPtr library,
                const int64_t playlistId,
                const std::string& categoryType,
                int64_t categoryId);

            /** @brief Creates a rename query.
             *  @param library The library to mutate.
             *  @param playlistId The playlist to rename.
             *  @param newName The new name. */
            SavePlaylistQuery(
                musik::core::ILibraryPtr library,
                const int64_t playlistId,
                const std::string& newName);

            /** @brief Creates an empty query (for (de)serialization). */
            SavePlaylistQuery(musik::core::ILibraryPtr library);

            /** @brief Broadcasts a playlist mutation to listeners. */
            void SendPlaylistMutationBroadcast();

            /** @brief Wraps either a shared TrackList or an SDK ITrackList. */
            struct TrackListWrapper {
                DELETE_COPY_AND_ASSIGNMENT_DEFAULTS(TrackListWrapper)

                /** @brief Creates an empty wrapper. */
                TrackListWrapper() noexcept;
                /** @brief Wraps a shared TrackList.
                 *  @param shared The shared track list. */
                TrackListWrapper(std::shared_ptr<musik::core::TrackList> shared) noexcept;

                /** @return true if a track list is present. */
                bool Exists() noexcept;
                /** @return The number of tracks. */
                size_t Count();
                /** @return The track at the given index.
                 *  @param library Library used to resolve metadata.
                 *  @param index Zero-based index. */
                TrackPtr Get(musik::core::ILibraryPtr library, size_t index);
                /** @return The wrapped SDK track list, or nullptr. */
                musik::core::sdk::ITrackList* Get() noexcept;

                std::shared_ptr<musik::core::TrackList> sharedTracks; /**< Shared track list variant. */
                musik::core::sdk::ITrackList* rawTracks; /**< SDK track list variant. */
            };

            /** @brief The playlist operation to perform. */
            enum class Operation: int {
                Create = 1, /**< Create a new playlist. */
                Rename = 2, /**< Rename an existing playlist. */
                Replace = 3,/**< Replace a playlist's tracks. */
                Append = 4  /**< Append to a playlist. */
            };

            /** @brief Creates a new playlist from the tracked tracks.
             *  @param db The connection to run on.
             *  @return true on success. */
            bool CreatePlaylist(musik::core::db::Connection &db);
            /** @brief Renames the target playlist.
             *  @param db The connection to run on.
             *  @return true on success. */
            bool RenamePlaylist(musik::core::db::Connection &db);
            /** @brief Replaces the target playlist's tracks.
             *  @param db The connection to run on.
             *  @return true on success. */
            bool ReplacePlaylist(musik::core::db::Connection &db);
            /** @brief Appends to the target playlist.
             *  @param db The connection to run on.
             *  @return true on success. */
            bool AppendToPlaylist(musik::core::db::Connection &db);

            /** @brief Adds a category's tracks to a playlist.
             *  @param db The connection to run on.
             *  @param playlistId The target playlist.
             *  @return true on success. */
            bool AddCategoryTracksToPlaylist(musik::core::db::Connection &db, int64_t playlistId);

            /** @brief Adds tracks to a playlist.
             *  @param db The connection to run on.
             *  @param playlistId The target playlist.
             *  @param tracks The tracks to add.
             *  @return true on success. */
            bool AddTracksToPlaylist(
                musik::core::db::Connection &db,
                int64_t playlistId,
                TrackListWrapper& tracks);

            bool result{ false }; /**< Whether the operation succeeded. */
            Operation op; /**< The operation to perform. */
            musik::core::ILibraryPtr library; /**< Library to mutate. */
            std::string playlistName, categoryType; /**< Playlist name and category type. */
            int64_t playlistId, categoryId; /**< Playlist id and category value id. */
            TrackListWrapper tracks; /**< The tracked tracks. */
    };

} } } }
