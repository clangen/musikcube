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

/**
 * @file MainController.h
 * @brief Controller for the musicwin application's main window.
 *
 * Bridges the Win32 UI (view) with the musikcore playback service and
 * library. It wires user interactions (transport buttons, search box,
 * track list) to the underlying services and processes asynchronous
 * messages delivered through the Win32 message queue by implementing
 * musik::core::runtime::IMessageTarget.
 */

#pragma once

#include <app/view/MainWindow.h>

#include <musikcore/runtime/IMessageQueue.h>
#include <musikcore/audio/PlaybackService.h>
#include <musikcore/library/ILibrary.h>

#include <musikcore/library/query/local/TrackListQueryBase.h>

#include <win32cpp/EditView.hpp>
#include <win32cpp/Button.hpp>
#include <win32cpp/ListView.hpp>

namespace musik { namespace win {
    /** @brief Controller for the main window.
     *  @details Owns the track list model/query and connects the view's
     *           event callbacks to the playback service and library. */
    class MainController :
        public sigslot::has_slots<>,
        public musik::core::runtime::IMessageTarget
    {
        public:
            /** @brief Constructs the controller and wires up the view.
             *  @param mainWindow the main window this controller operates on
             *  @param playback the playback service used for transport control
             *  @param library the library used to query tracks */
            MainController(
                MainWindow& mainWindow,
                musik::core::audio::PlaybackService& playback,
                musik::core::ILibraryPtr library);

            /** @brief Destroys the controller, releasing the view widgets. */
            virtual ~MainController();

            /** @brief Handles a message dispatched to this controller.
             *  @param message the message to process
             *  @note Dispatches playback state changes and library query
             *        completion notifications. */
            virtual void ProcessMessage(musik::core::runtime::IMessage &message);

        private:
            /** @brief Adapts a TrackList into the data source for the ListView. */
            class TrackListModel;

            /** @brief Re-lays-out child controls when the main window resizes. */
            void OnMainWindowResized(win32cpp::Window* window, win32cpp::Size size);
            /** @brief Starts playback of the row's track when activated. */
            void OnTrackListRowActivated(win32cpp::ListView* list, int index);
            /** @brief Refreshes the model when a library query completes. */
            void OnLibraryQueryCompleted(musik::core::db::IQuery* query);
            /** @brief Updates the query when the search text changes. */
            void OnSearchEditChanged(win32cpp::EditView* editView);
            /** @brief Routes transport button clicks to the playback service. */
            void OnTransportButtonClicked(win32cpp::Button* button);
            /** @brief Updates the UI to reflect the current playback state. */
            void OnPlaybackStateChanged(int state);

            /** @brief Positions all child widgets inside the main window. */
            void Layout();

            musik::core::audio::PlaybackService& playback; /**< playback service for transport control */
            musik::core::ILibraryPtr library;             /**< library used to resolve tracks */

            MainWindow& mainWindow;                       /**< the main window being controlled */

            std::shared_ptr<musik::core::db::local::TrackListQueryBase> trackListQuery; /**< active library query for the track list */
            std::shared_ptr<TrackListModel> trackListModel;                             /**< model backing the ListView */
            std::shared_ptr<musik::core::TrackList> trackList;                          /**< latest query result */

            win32cpp::ListView* trackListView; /**< list control showing the track library */
            win32cpp::EditView* editView;      /**< search box filtering the track list */
            win32cpp::Button* prevButton;      /**< previous-track transport button */
            win32cpp::Button* nextButton;      /**< next-track transport button */
            win32cpp::Button* pauseButton;     /**< play/pause transport button */

            bool trackListDirty; /**< true when the track list needs a refresh */
    };
} }