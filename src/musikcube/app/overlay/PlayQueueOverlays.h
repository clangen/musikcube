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

/**
 * @file PlayQueueOverlays.h
 * @brief Factory for overlays that manage the play queue and playlists.
 * @details Provides static helpers to add tracks, categories or directories
 *          to the queue, and to load, save, rename, delete and create
 *          playlists. Callbacks receive the affected playlist id when
 *          applicable.
 */

#include <musikcore/audio/PlaybackService.h>
#include <musikcore/library/ILibrary.h>
#include <musikcore/library/query/TrackListQueryBase.h>
#include <app/window/TrackListView.h>

namespace musik {
    namespace cube {
        /**
         * @brief Factory for play queue and playlist management overlays.
         * @details All methods are static and show a modal overlay. The class
         *          is not meant to be instantiated.
         */
        class PlayQueueOverlays {
            public:
                using PlaylistSelectedCallback = std::function<void(int64_t)>;
                using QueryCallback = musik::core::ILibrary::Callback;

                /**
                 * @brief Shows a dialog to add a single track to the queue.
                 * @param messageQueue the queue for dispatching playback
                 * @param library the library that owns the track
                 * @param playback the active playback service
                 * @param track the track to add
                 */
                static void ShowAddTrackOverlay(
                    musik::core::runtime::IMessageQueue& messageQueue,
                    musik::core::ILibraryPtr library,
                    musik::core::audio::PlaybackService& playback,
                    musik::core::TrackPtr track);

                /**
                 * @brief Shows a dialog to add a whole category to the queue.
                 * @param messageQueue the queue for dispatching playback
                 * @param playback the active playback service
                 * @param library the library that owns the category
                 * @param fieldColumn the category column name
                 * @param fieldValue the category value
                 * @param fieldId the id of the category value
                 */
                static void ShowAddCategoryOverlay(
                    musik::core::runtime::IMessageQueue& messageQueue,
                    musik::core::audio::PlaybackService& playback,
                    musik::core::ILibraryPtr library,
                    const std::string& fieldColumn,
                    const std::string& fieldValue,
                    int64_t fieldId);

                /**
                 * @brief Shows a dialog to add a directory's tracks.
                 * @param messageQueue the queue for dispatching playback
                 * @param playback the active playback service
                 * @param library the library that indexes the directory
                 * @param directory the directory to add
                 */
                static void ShowAddDirectoryOverlay(
                    musik::core::runtime::IMessageQueue& messageQueue,
                    musik::core::audio::PlaybackService& playback,
                    musik::core::ILibraryPtr library,
                    const std::string& directory);

                /**
                 * @brief Shows a dialog to add all tracks by an album divider.
                 * @param messageQueue the queue for dispatching playback
                 * @param playback the active playback service
                 * @param library the library that owns the tracks
                 * @param firstTrack the first track of the album
                 */
                static void ShowAlbumDividerOverlay(
                    musik::core::runtime::IMessageQueue& messageQueue,
                    musik::core::audio::PlaybackService& playback,
                    musik::core::ILibraryPtr library,
                    musik::core::TrackPtr firstTrack);

                /**
                 * @brief Shows a dialog to pick a playlist to load.
                 * @param playback the active playback service
                 * @param library the library that owns the playlists
                 * @param callback invoked with the id of the loaded playlist
                 */
                static void ShowLoadPlaylistOverlay(
                    musik::core::audio::PlaybackService& playback,
                    musik::core::ILibraryPtr library,
                    PlaylistSelectedCallback callback);

                /**
                 * @brief Shows a dialog to save the queue as a playlist.
                 * @param queue the queue for dispatching playback
                 * @param playback the active playback service
                 * @param library the library that owns the playlists
                 * @param selectedPlaylistId optional playlist to overwrite
                 */
                static void ShowSavePlaylistOverlay(
                    musik::core::runtime::IMessageQueue& queue,
                    musik::core::audio::PlaybackService& playback,
                    musik::core::ILibraryPtr library,
                    int64_t selectedPlaylistId = -1);

                /**
                 * @brief Shows a dialog to rename the selected playlist.
                 * @param library the library that owns the playlists
                 */
                static void ShowRenamePlaylistOverlay(
                    musik::core::ILibraryPtr library);

                /**
                 * @brief Shows a dialog to rename a specific playlist.
                 * @param library the library that owns the playlists
                 * @param playlistId the playlist to rename
                 * @param old the current playlist name
                 * @param callback invoked when the rename completes
                 */
                static void ShowRenamePlaylistOverlay(
                    musik::core::ILibraryPtr library,
                    const int64_t playlistId,
                    const std::string& old,
                    QueryCallback callback = QueryCallback());

                /**
                 * @brief Shows a dialog to delete the selected playlist.
                 * @param library the library that owns the playlists
                 */
                static void ShowDeletePlaylistOverlay(
                    musik::core::ILibraryPtr library);

                /**
                 * @brief Asks for confirmation before deleting a playlist.
                 * @param library the library that owns the playlists
                 * @param playlistName the name of the playlist to delete
                 * @param playlistId the id of the playlist to delete
                 * @param callback invoked when the delete completes
                 */
                static void ShowConfirmDeletePlaylistOverlay(
                    musik::core::ILibraryPtr library,
                    const std::string& playlistName,
                    const int64_t playlistId,
                    QueryCallback callback = QueryCallback());

                /**
                 * @brief Shows a dialog to create a new playlist.
                 * @param queue the queue for dispatching playback
                 * @param library the library that owns the playlists
                 * @param callback invoked when the create completes
                 */
                static void ShowCreatePlaylistOverlay(
                    musik::core::runtime::IMessageQueue& queue,
                    musik::core::ILibraryPtr library,
                    QueryCallback callback = QueryCallback());

            private:
                PlayQueueOverlays() noexcept;
        };
    }
}
