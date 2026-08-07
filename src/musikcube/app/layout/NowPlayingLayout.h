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
 * @file NowPlayingLayout.h
 * @brief Layout that shows the currently playing track and the play queue.
 * @details Displays the active playlist/queue in a track list and supports
 *          editing the queue (removing, moving and clearing entries).
 */

#include <cursespp/Colors.h>
#include <cursespp/LayoutBase.h>
#include <app/window/CategoryListView.h>
#include <app/window/TrackListView.h>
#include <musikcore/audio/PlaybackService.h>
#include <musikcore/support/Preferences.h>

#include <musikcore/library/ILibrary.h>

#include <sigslot/sigslot.h>

namespace musik {
    namespace cube {
        /**
         * @brief Now playing and play queue layout.
         * @details Renders the active play queue as a track list, highlighting
         *          the current track and supporting queue edit operations such
         *          as removing, moving and clearing entries.
         */
        class NowPlayingLayout :
            public cursespp::LayoutBase,
            public sigslot::has_slots<>
        {
            public:
                DELETE_CLASS_DEFAULTS(NowPlayingLayout)

                /**
                 * @brief Creates the layout.
                 * @param playback the active playback service
                 * @param library the library used to query tracks
                 */
                NowPlayingLayout(
                    musik::core::audio::PlaybackService& playback,
                    musik::core::ILibraryPtr library);

                /* IWindow */
                /**
                 * @brief Called when the layout becomes visible or hidden.
                 * @param visible true if the layout became visible
                 */
                void OnVisibilityChanged(bool visible) override;
                /**
                 * @brief Handles keyboard input.
                 * @param key the key sequence that was pressed
                 * @return true if the event was consumed
                 */
                bool KeyPress(const std::string& key) override;

            protected:
                /* LayoutBase */
                /**
                 * @brief Positions and lays out the child windows.
                 */
                void OnLayout() override;

            private:
                void InitializeWindows();
                void RequeryTrackList();
                bool ProcessEditOperation(const std::string& key);

                /* callbacks */
                void OnTrackListRequeried(musik::core::library::query::TrackListQueryBase* query);
                cursespp::Color RowDecorator(musik::core::TrackPtr track, size_t index);
                void OnPlaylistSelected(int64_t playlistId);

                musik::core::audio::PlaybackService& playback; /**< the active playback service */
                musik::core::ILibraryPtr library;              /**< the library used to query tracks */
                std::shared_ptr<TrackListView> trackListView;  /**< the track list window */
                std::shared_ptr<musik::core::Preferences> prefs; /**< persistent preferences */
                int reselectIndex; /* sigh... */               /**< index used to restore the selection */
                int lastPlaylistQueryId;                       /**< id of the last playlist query */
        };
    }
}
