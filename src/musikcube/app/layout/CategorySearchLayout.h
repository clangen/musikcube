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
 * @file CategorySearchLayout.h
 * @brief Layout for searching the library by album, artist and genre.
 * @details Provides a text input whose results are live-requeried against the
 *          library, with results shown in three category lists (albums,
 *          artists, genres). Emits SearchResultSelected when a result is
 *          activated.
 */

#include <cursespp/LayoutBase.h>
#include <cursespp/TextInput.h>
#include <cursespp/TextLabel.h>

#include <app/window/CategoryListView.h>
#include <app/window/TrackListView.h>

#include <musikcore/audio/PlaybackService.h>
#include <musikcore/library/ILibrary.h>
#include <musikcore/support/Preferences.h>

#include <sigslot/sigslot.h>

namespace musik {
    namespace cube {
        /**
         * @brief Library search layout.
         * @details Contains a search input and three category result lists
         *          (albums, artists, genres). Typing triggers live library
         *          queries; activating a result emits SearchResultSelected.
         */
        class CategorySearchLayout :
            public cursespp::LayoutBase,
            public sigslot::has_slots<>
        {
            public:
                /**
                 * @brief Emitted when a search result is activated.
                 * @param layout this layout
                 * @param fieldType the type of the activated field
                 * @param fieldId the id of the activated field
                 */
                sigslot::signal3<CategorySearchLayout*, std::string, int64_t> SearchResultSelected;

                DELETE_CLASS_DEFAULTS(CategorySearchLayout)

                /**
                 * @brief Creates the layout with the given playback service
                 *        and library.
                 * @param playback the active playback service
                 * @param library the library to search
                 */
                CategorySearchLayout(
                    musik::core::audio::PlaybackService& playback,
                    musik::core::ILibraryPtr library);

                /**
                 * @brief Destroys the layout and its child views.
                 */
                virtual ~CategorySearchLayout();

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
                 * @brief Positions and lays out the child windows.
                 */
                void OnLayout() override;
                /**
                 * @brief Processes runtime messages.
                 * @param message the message to process
                 */
                void ProcessMessage(musik::core::runtime::IMessage& message) override;

            private:
                using MatchType = musik::core::library::query::QueryBase::MatchType;

                void InitializeWindows(musik::core::audio::PlaybackService& playback);

                void CreateCategoryView(
                    std::shared_ptr<CategoryListView>& view,
                    musik::core::audio::PlaybackService& playback,
                    const std::string& title,
                    const std::string& type,
                    int order);

                void OnCategoryEntryActivated(
                    cursespp::ListWindow* sender,
                    size_t index);

                void OnEnterPressed(cursespp::TextInput* sender);

                void OnInputChanged(
                    cursespp::TextInput* sender,
                    std::string value);

                void Requery();
                void SaveSession();
                void ToggleMatchType();
                void SetMatchType(MatchType matchType);

                musik::core::ILibraryPtr library;                       /**< the library being searched */
                MatchType matchType{ MatchType::Substring };            /**< the current query match type */
                std::shared_ptr<musik::core::Preferences> prefs;        /**< persistent preferences for the layout */
                std::shared_ptr<CategoryListView> albums;               /**< the album results list */
                std::shared_ptr<CategoryListView> artists;              /**< the artist results list */
                std::shared_ptr<CategoryListView> genres;               /**< the genre results list */
                std::shared_ptr<cursespp::TextInput> input;             /**< the search input */
        };
    }
}
