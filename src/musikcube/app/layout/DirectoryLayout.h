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
 * @file DirectoryLayout.h
 * @brief Layout for browsing a local directory tree and playing its tracks.
 * @details Presents the folder hierarchy in a directory list on the left and
 *          the tracks of the selected directory on the right, updating the
 *          title as the user navigates.
 */

#include <cursespp/Colors.h>
#include <cursespp/LayoutBase.h>
#include <cursespp/TextLabel.h>

#include <app/window/CategoryListView.h>
#include <app/window/TrackListView.h>
#include <app/model/DirectoryAdapter.h>
#include <musikcore/audio/PlaybackService.h>

#include <musikcore/library/ILibrary.h>

#include <sigslot/sigslot.h>

namespace musik {
    namespace cube {
        /**
         * @brief Directory browser layout.
         * @details Two-pane layout; the left pane shows the folders below the
         *          root directory, including a parent entry, and the right
         *          pane lists the tracks in the currently selected folder.
         */
        class DirectoryLayout :
            public cursespp::LayoutBase,
            public sigslot::has_slots<>
        {
            public:
                DELETE_CLASS_DEFAULTS(DirectoryLayout)

                /**
                 * @brief Creates the layout with the given playback service
                 *        and library.
                 * @param playback the active playback service
                 * @param library the library used to index and query tracks
                 */
                DirectoryLayout(
                    musik::core::audio::PlaybackService& playback,
                    musik::core::ILibraryPtr library);

                /**
                 * @brief Sets the root directory to browse.
                 * @param directory the absolute path of the root directory
                 */
                void SetDirectory(const std::string& directory);
                /**
                 * @brief Plays the tracks in the track list starting from the
                 *        top.
                 */
                void PlayFromTop();
                /**
                 * @brief Returns the currently browsed root directory.
                 * @return the absolute path of the root directory
                 */
                std::string GetDirectory();

                /* IWindow */
                /**
                 * @brief Called when the layout becomes visible or hidden.
                 * @param visible true if the layout became visible
                 */
                void OnVisibilityChanged(bool visible) override;
                /**
                 * @brief Positions and lays out the child windows.
                 */
                void OnLayout() override;
                /**
                 * @brief Handles keyboard input.
                 * @param key the key sequence that was pressed
                 * @return true if the event was consumed
                 */
                bool KeyPress(const std::string& key) override;

            private:
                void InitializeWindows();
                void Refresh(bool requery = false);
                void Requery(bool invalidate = false);
                void RequeryTrackList(cursespp::ListWindow *view);
                void UpdateTitle();
                bool IsParentSelected();
                bool IsParentRoot();

                cursespp::Color ListItemDecorator(
                    cursespp::ScrollableWindow* scrollable,
                    size_t index,
                    size_t line,
                    cursespp::IScrollAdapter::EntryPtr entry);

                void OnDirectoryChanged(
                    cursespp::ListWindow *view,
                    size_t newIndex,
                    size_t oldIndex);

                void OnIndexerProgress(int count);

                musik::core::audio::PlaybackService& playback; /**< the active playback service */
                musik::core::ILibraryPtr library;              /**< the library used to index tracks */
                std::string rootDirectory;                     /**< the root directory being browsed */
                std::shared_ptr<DirectoryAdapter> adapter;     /**< the adapter feeding the directory list */
                std::shared_ptr<cursespp::ListWindow> directoryList; /**< the directory list window */
                std::shared_ptr<TrackListView> trackList;      /**< the track list window */
                size_t queryHash;                              /**< hash identifying the active track query */
                bool hasSubdirectories;                        /**< true if the current folder has subfolders */
        };
    }
}
