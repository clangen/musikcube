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

#include <musikcore/library.hpp>

#include <cursespp/App.h>
#include <cursespp/AppLayout.h>
#include <cursespp/TextInput.h>
#include <cursespp/TextLabel.h>

#include <musikcore/playback.hpp>
#include <musikcore/preferences.hpp>
#include <common/runtime/IMessageTarget.h>
#include <musikcore/indexer.hpp>

#include <app/util/ConsoleLogger.h>
#include <app/util/UpdateCheck.h>
#include <app/util/AsyncCallbackScope.h>

#include <sigslot/sigslot.h>

namespace musik {
    namespace cube {
        class LibraryLayout;
        class LibraryNotConnectedLayout;
        class MainLayout : public cursespp::AppLayout {
            public:
                DELETE_CLASS_DEFAULTS(MainLayout)

                MainLayout(
                    cursespp::App& app,
                    ConsoleLogger* logger,
                    musik::core::api::Context& musikcore);

                virtual ~MainLayout();

                void Start();
                void Shutdown();

                /* IWindow */
                bool KeyPress(const std::string& key) override;
                void OnLayout() override;
                void ProcessMessage(musik::core::runtime::IMessage &message) override;

            private:
                void OnIndexerEvent(mc_indexer_event event, int updates);
                void UpdateWindowTitle();
                void OnLibraryChanged(bool selectionChanged);

                bool IsLibraryConnected();

                void RunUpdateCheck();
                void SetInitialLayout();
                void SwitchToLibraryLayout();
                void SwitchToPlayQueue();

                bool ShowTopBanner();
                void UpdateTopBannerText();


                musik::core::api::Preferences prefs { MC_PREFERENCE_SETTINGS };
                std::shared_ptr<cursespp::TextLabel> topBanner;
                std::shared_ptr<cursespp::LayoutBase> consoleLayout;
                std::shared_ptr<LibraryLayout> libraryLayout;
                std::shared_ptr<LibraryNotConnectedLayout> libraryNotConnectedLayout;
                std::shared_ptr<cursespp::LayoutBase> settingsLayout;
                std::shared_ptr<cursespp::LayoutBase> hotkeysLayout;
                std::shared_ptr<cursespp::LayoutBase> lyricsLayout;
                musik::core::api::Playback playback;
                bool shortcutsFocused;
                int syncUpdateCount;
                musik::core::api::Context& musikcore;
                AsyncCallbackScope titleWork;
                UpdateCheck updateCheck;
        };
    }
}
