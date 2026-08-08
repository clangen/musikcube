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
 * @file TrackSearchLayout.h
 * @brief Layout for searching the library for tracks.
 * @details Provides a text input that live-queries the library for matching
 *          tracks, with results shown in a track list that supports playing
 *          and queueing.
 */

#include <cursespp/LayoutBase.h>
#include <cursespp/TextInput.h>

#include <app/window/TrackListView.h>

#include <musikcore/audio/PlaybackService.h>
#include <musikcore/library/ILibrary.h>
#include <musikcore/support/Preferences.h>

#include <sigslot/sigslot.h>

namespace musik {
    namespace cube {
        /**
         * @brief Track search layout.
         * @details Contains a search input and a track list. Typing triggers
         *          live library queries; results can be played or queued, and
         *          the match type and sort order can be changed.
         */
        class TrackSearchLayout :
            public cursespp::LayoutBase,
            public sigslot::has_slots<>
        {
            public:
                DELETE_COPY_AND_ASSIGNMENT_DEFAULTS(TrackSearchLayout)

                /**
                 * @brief Creates the layout with the given playback service
                 *        and library.
                 * @param playback the active playback service
                 * @param library the library to search
                 */
                TrackSearchLayout(
                    musik::core::audio::PlaybackService& playback,
                    musik::core::ILibraryPtr library);

                /**
                 * @brief Destroys the layout and its child views.
                 */
                virtual ~TrackSearchLayout();

                /**
                 * @brief Plays the tracks in the result list starting from the
                 *        top.
                 */
                void PlayFromTop();
                /**
                 * @brief Moves keyboard focus to the search input.
                 */
                void FocusInput();
                /**
                 * @brief Restores the last search from the previous session.
                 */
                void LoadLastSession();

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
                /**
                 * @brief Processes runtime messages.
                 * @param message the message to process
                 */
                void ProcessMessage(musik::core::runtime::IMessage &message) override;
                /**
                 * @brief Positions and lays out the child windows.
                 */
                void OnLayout() override;

            private:
                using MatchType = musik::core::library::query::QueryBase::MatchType;

                void SaveSession();
                void InitializeWindows();
                void Requery();
                void OnIndexerFinished(int);

                void OnRequeried(musik::core::library::query::TrackListQueryBase* query);

                void OnWindowMouseEvent(
                    cursespp::Window* window,
                    const cursespp::IMouseHandler::Event* mouseEvent);

                void OnInputChanged(
                    cursespp::TextInput* sender,
                    std::string value);

                void OnEnterPressed(cursespp::TextInput* sender);

                void ToggleMatchType();
                void SetMatchType(MatchType matchType);
                void ShowTrackSortOverlay();

                musik::core::audio::PlaybackService& playback; /**< the active playback service */
                musik::core::ILibraryPtr library;              /**< the library being searched */
                MatchType matchType{ MatchType::Substring };   /**< the current query match type */
                std::shared_ptr<musik::core::Preferences> prefs; /**< persistent preferences */
                std::shared_ptr<TrackListView> trackList;      /**< the results track list */
                std::shared_ptr<cursespp::TextInput> input;    /**< the search input */
        };
    }
}
