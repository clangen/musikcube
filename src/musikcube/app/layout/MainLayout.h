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
 * @file MainLayout.h
 * @brief Root application layout for the terminal client.
 * @details Owns the top-level views (library, console, settings, hotkeys,
 *          lyrics, library-not-connected), a top banner, and application-wide
 *          startup and shutdown logic.
 */

#include <cursespp/App.h>
#include <cursespp/AppLayout.h>
#include <cursespp/TextInput.h>
#include <cursespp/TextLabel.h>

#include <musikcore/audio/PlaybackService.h>
#include <musikcore/support/Preferences.h>
#include <musikcore/runtime/IMessageTarget.h>
#include <musikcore/library/MasterLibrary.h>
#include <musikcore/net/PiggyWebSocketClient.h>

#include <app/util/ConsoleLogger.h>
#include <app/util/UpdateCheck.h>

#include <sigslot/sigslot.h>

namespace musik {
    namespace cube {
        /**
         * @brief Root application layout.
         * @details Creates and switches between the top-level layouts, renders
         *          a top banner, reacts to library/indexer state changes and
         *          performs the update check on startup.
         */
        class MainLayout : public cursespp::AppLayout {
            public:
                using MasterLibraryPtr = std::shared_ptr<musik::core::library::MasterLibrary>;

                DELETE_CLASS_DEFAULTS(MainLayout)

                /**
                 * @brief Creates the root layout.
                 * @param app the owning application
                 * @param logger the console logger for the console layout
                 * @param playback the active playback service
                 * @param library the master library
                 */
                MainLayout(
                    cursespp::App& app,
                    ConsoleLogger* logger,
                    musik::core::audio::PlaybackService& playback,
                    MasterLibraryPtr library);

                /**
                 * @brief Destroys the layout and its child views.
                 */
                virtual ~MainLayout();

                /**
                 * @brief Starts the application: connects the library, shows
                 *        the initial layout and runs the update check.
                 */
                void Start();
                /**
                 * @brief Shuts the application down and releases resources.
                 */
                void Shutdown();

                /* IWindow */
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
                void ProcessMessage(musik::core::runtime::IMessage &message) override;

            private:
                void OnIndexerStarted();
                void OnIndexerProgress(int count);
                void OnIndexerFinished(int count);
                void OnTrackChanged(size_t index, musik::core::TrackPtr track);
                void OnLibraryConnectionStateChanged(musik::core::ILibrary::ConnectionState state);
                void OnLibraryChanged(musik::core::ILibraryPtr prev, musik::core::ILibraryPtr curr);

                bool IsLibraryConnected();

                void RunUpdateCheck();
                void SetInitialLayout();
                void SwitchToLibraryLayout();
                void SwitchToPlayQueue();

                bool ShowTopBanner();
                void UpdateTopBannerText();

                void RebindIndexerEventHandlers(musik::core::ILibraryPtr prev, musik::core::ILibraryPtr curr);

                std::shared_ptr<musik::core::Preferences> prefs;        /**< persistent preferences */
                std::shared_ptr<cursespp::TextLabel> topBanner;         /**< the banner shown at the top of the screen */
                std::shared_ptr<cursespp::LayoutBase> consoleLayout;    /**< the console layout */
                std::shared_ptr<cursespp::LayoutBase> libraryLayout;    /**< the library layout */
                std::shared_ptr<cursespp::LayoutBase> libraryNotConnectedLayout; /**< the disconnected library layout */
                std::shared_ptr<cursespp::LayoutBase> settingsLayout;   /**< the settings layout */
                std::shared_ptr<cursespp::LayoutBase> hotkeysLayout;    /**< the hotkeys layout */
                std::shared_ptr<cursespp::LayoutBase> lyricsLayout;     /**< the lyrics layout */
                musik::core::audio::PlaybackService& playback;          /**< the active playback service */
                MasterLibraryPtr library;                               /**< the master library */
                bool shortcutsFocused;                                  /**< true while the shortcuts window has focus */
                int syncUpdateCount;                                    /**< indexer progress counter */
                UpdateCheck updateCheck;                                /**< performs the startup update check */
        };
    }
}
