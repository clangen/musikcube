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
 * @file LibraryLayout.h
 * @brief Top-level container that hosts the library browsing layouts.
 * @details Switches between the Browse, Directory, CategorySearch,
 *          TrackSearch, NowPlaying and transport views and forwards keyboard
 *          and focus handling to the active sub-layout.
 */

#include <cursespp/LayoutBase.h>
#include <cursespp/ShortcutsWindow.h>
#include <cursespp/ITopLevelLayout.h>
#include <app/layout/BrowseLayout.h>
#include <app/layout/DirectoryLayout.h>
#include <app/layout/NowPlayingLayout.h>
#include <app/layout/CategorySearchLayout.h>
#include <app/layout/TrackSearchLayout.h>
#include <app/window/TransportWindow.h>
#include <musikcore/audio/PlaybackService.h>
#include <musikcore/support/Preferences.h>
#include <musikcore/library/ILibrary.h>

#include <sigslot/sigslot.h>

namespace musik {
    namespace cube {
        /**
         * @brief Top-level library layout.
         * @details Hosts the different library views (browse, directories,
         *          category search, track search, now playing) plus a
         *          transport window, delegating focus, keyboard and message
         *          handling to the currently visible layout.
         */
        class LibraryLayout :
            public cursespp::LayoutBase,
            public cursespp::ITopLevelLayout,
            public sigslot::has_slots<>
        {
            public:
                DELETE_CLASS_DEFAULTS(LibraryLayout)

                /**
                 * @brief Creates the top-level library layout.
                 * @param playback the active playback service
                 * @param library the library to operate on
                 */
                LibraryLayout(
                    musik::core::audio::PlaybackService& playback,
                    musik::core::ILibraryPtr library);

                /**
                 * @brief Moves focus to the next focusable window.
                 * @return the window that received focus
                 */
                cursespp::IWindowPtr FocusNext() override;
                /**
                 * @brief Moves focus to the previous focusable window.
                 * @return the window that received focus
                 */
                cursespp::IWindowPtr FocusPrev() override;
                /**
                 * @brief Returns the currently focused window.
                 * @return the focused window
                 */
                cursespp::IWindowPtr GetFocus() override;
                /**
                 * @brief Sets the focused window.
                 * @param window the window to focus
                 * @return true if the window accepted focus
                 */
                bool SetFocus(cursespp::IWindowPtr window) override;
                /**
                 * @brief Processes runtime messages.
                 * @param message the message to process
                 */
                void ProcessMessage(musik::core::runtime::IMessage &message) override;
                /**
                 * @brief Called when the layout becomes visible or hidden.
                 * @param visible true if the layout became visible
                 */
                void OnVisibilityChanged(bool visible) override;

                /**
                 * @brief Attaches the shortcuts window shown at the bottom.
                 * @param w the shortcuts window
                 */
                void SetShortcutsWindow(
                    cursespp::ShortcutsWindow* w) override;

                /**
                 * @brief Handles keyboard input.
                 * @param key the key sequence that was pressed
                 * @return true if the event was consumed
                 */
                bool KeyPress(const std::string& key) override;

            protected:
                /**
                 * @brief Positions and lays out the child windows.
                 */
                void OnLayout() override;
                /**
                 * @brief Called when the layout is added to a parent window.
                 * @param newParent the parent window
                 */
                void OnAddedToParent(IWindow* newParent) override;
                /**
                 * @brief Called when the layout is removed from a parent window.
                 * @param oldParent the former parent window
                 */
                void OnRemovedFromParent(IWindow* oldParent) override;

            private:
                void LoadLastSession();

                void OnCategorySearchResultSelected(
                    CategorySearchLayout* layout,
                    std::string fieldType,
                    int64_t fieldId);

                void OnMainLayoutFocusTerminated(
                    LayoutBase::FocusDirection direction);

                void InitializeWindows();

                void ShowNowPlaying();
                void ShowBrowse(const std::string& category = "");
                void ShowCategorySearch();
                void ShowCategoryChooser();
                void ShowTrackSearch();
                void ShowDirectoryChooser();
                void ShowDirectories(const std::string& directory = "");

                void ChangeMainLayout(std::shared_ptr<cursespp::LayoutBase> newLayout);
                void OnLayoutChanged();
                void UpdateShortcutsWindow();

                musik::core::audio::PlaybackService& playback;      /**< the active playback service */
                musik::core::audio::ITransport& transport;          /**< the playback transport */
                musik::core::ILibraryPtr library;                   /**< the library operated on */
                std::shared_ptr<musik::core::Preferences> prefs;    /**< persistent preferences */
                std::shared_ptr<BrowseLayout> browseLayout;         /**< the browse sub-layout */
                std::shared_ptr<DirectoryLayout> directoryLayout;   /**< the directory sub-layout */
                std::shared_ptr<TransportWindow> transportView;     /**< the transport window */
                std::shared_ptr<NowPlayingLayout> nowPlayingLayout; /**< the now playing sub-layout */
                std::shared_ptr<CategorySearchLayout> categorySearchLayout; /**< the category search sub-layout */
                std::shared_ptr<TrackSearchLayout> trackSearchLayout;      /**< the track search sub-layout */
                std::shared_ptr<cursespp::LayoutBase> visibleLayout;       /**< the currently visible sub-layout */
                cursespp::ShortcutsWindow* shortcuts;                     /**< the shortcuts window, not owned */
                std::string lastBrowseCategoryType;                       /**< the last browsed category type */
        };
    }
}
