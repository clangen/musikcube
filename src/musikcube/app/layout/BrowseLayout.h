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
 * @file BrowseLayout.h
 * @brief Library browsing layout with a category pane and a track pane.
 * @details Lets the user navigate the library by category (album, artist,
 *          genre), filter categories, and play or queue tracks from the
 *          results.
 */

#include <cursespp/LayoutBase.h>
#include <cursespp/TextLabel.h>
#include <cursespp/TextInput.h>

#include <app/window/CategoryListView.h>
#include <app/window/TrackListView.h>
#include <musikcore/audio/PlaybackService.h>
#include <musikcore/support/Preferences.h>
#include <musikcore/library/ILibrary.h>

#include <sigslot/sigslot.h>

#include <functional>

namespace musik {
    namespace cube {
        /**
         * @brief Main library browser layout.
         * @details Two-pane layout; the left pane lists library categories
         *          (albums, artists, genres) and the right pane lists the
         *          matching tracks. Supports playlist and category edit
         *          operations and remembers the last visited category.
         */
        class BrowseLayout :
            public cursespp::LayoutBase,
            public sigslot::has_slots<>
        {
            public:
                DELETE_CLASS_DEFAULTS(BrowseLayout)

                using HeaderClickHandler = std::function<void()>;

                /**
                 * @brief Creates the layout with the given playback service
                 *        and library.
                 * @param playback the active playback service
                 * @param library the library that provides the categories
                 */
                BrowseLayout(
                    musik::core::audio::PlaybackService& playback,
                    musik::core::ILibraryPtr library);

                /**
                 * @brief Destroys the layout and its child views.
                 */
                virtual ~BrowseLayout();

                /* IWindow */
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
                 * @brief Scrolls the category list to the given field.
                 * @param fieldType the type of the field to scroll to
                 * @param fieldId the id of the field to scroll to
                 */
                void ScrollTo(const std::string& fieldType, int64_t fieldId);
                /**
                 * @brief Switches the active library category.
                 * @param fieldName the name of the category to switch to
                 */
                void SwitchCategory(const std::string& fieldName);
                /**
                 * @brief Plays the tracks in the track list starting from the
                 *        top.
                 */
                void PlayFromTop();

                /**
                 * @brief Sets a callback invoked when the category list header
                 *        is clicked.
                 * @param handler the click handler
                 */
                void SetOnHeaderClicked(HeaderClickHandler handler);

                /**
                 * @brief Restores the last browsed category from the previous
                 *        session.
                 */
                void LoadLastSession();

            protected:
                /* LayoutBase */
                /**
                 * @brief Positions and lays out the child windows.
                 */
                void OnLayout() override;

            private:
                void InitializeWindows();
                void SaveSession();

                void OnIndexerProgress(int count);
                void RequeryTrackList(cursespp::ListWindow *view);

                void OnCategoryViewSelectionChanged(
                    cursespp::ListWindow *view, size_t newIndex, size_t oldIndex);

                void OnCategoryViewInvalidated(
                    cursespp::ListWindow *view, size_t selectedIndex);

                void OnCategoryFilterChanged(
                    cursespp::TextInput* sender, std::string value);

                void OnCategoryFilterEnterPressed(cursespp::TextInput* sender);

                void OnWindowMouseEvent(
                    cursespp::Window* window, const cursespp::IMouseHandler::Event* mouseEvent);

                void OnRequeried(musik::core::library::query::TrackListQueryBase* query);

                bool IsPlaylist();
                bool ProcessEditOperation(const std::string& key);
                bool ProcessPlaylistOperation(const std::string& key);
                void ShowModifiedLabel(bool show);
                void ShowTrackSortOverlay();

                bool playlistModified{ false };                          /**< true while the playlist has unsaved edits */
                bool showCategoryListFilter{ false };                    /**< true while the category filter input is visible */
                std::string currentFilter;                               /**< the active category filter text */
                musik::core::audio::PlaybackService& playback;           /**< the active playback service */
                musik::core::ILibraryPtr library;                        /**< the library being browsed */
                std::shared_ptr<musik::core::Preferences> prefs;         /**< persistent preferences for the layout */
                std::shared_ptr<CategoryListView> categoryList;          /**< the category list window */
                std::shared_ptr<cursespp::TextInput> categoryListFilter; /**< the category filter input */
                std::shared_ptr<TrackListView> trackList;                /**< the track list window */
                std::shared_ptr<cursespp::TextLabel> modifiedLabel;      /**< label shown when the playlist is modified */
                HeaderClickHandler categoryListHeaderClickHandler;       /**< callback for category header clicks */
        };
    }
}
