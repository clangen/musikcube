//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004-2019 musikcube team
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

#include <cursespp/App.h>
#include <cursespp/AppLayout.h>
#include <cursespp/TextInput.h>
#include <cursespp/TextLabel.h>

#include <core/audio/PlaybackService.h>
#include <core/support/Preferences.h>
#include <core/library/ILibrary.h>
#include <core/runtime/IMessageTarget.h>

#include <core/audio/MasterTransport.h>

#include <app/util/ConsoleLogger.h>

#include <sigslot/sigslot.h>

namespace musik {
    namespace cube {
        class MainLayout : public cursespp::AppLayout {
            public:
                MainLayout(
                    cursespp::App& app,
                    ConsoleLogger* logger,
                    musik::core::audio::PlaybackService& playback,
                    musik::core::ILibraryPtr library);

                virtual ~MainLayout();

                void Start();
                void Stop();

                virtual bool KeyPress(const std::string& key) override;
                virtual void OnLayout() override;
                virtual void ProcessMessage(musik::core::runtime::IMessage &message) override;

            private:
                void OnIndexerStarted();
                void OnIndexerProgress(int count);
                void OnIndexerFinished(int count);
                void OnTrackChanged(size_t index, musik::core::TrackPtr track);

                void Initialize();
                void RunUpdateCheck();

                std::shared_ptr<musik::core::Preferences> prefs;
                std::shared_ptr<cursespp::TextLabel> syncing;
                std::shared_ptr<cursespp::LayoutBase> consoleLayout;
                std::shared_ptr<cursespp::LayoutBase> libraryLayout;
                std::shared_ptr<cursespp::LayoutBase> settingsLayout;
                std::shared_ptr<cursespp::LayoutBase> hotkeysLayout;
                std::shared_ptr<cursespp::LayoutBase> lyricsLayout;
                musik::core::ILibraryPtr library;
                bool shortcutsFocused;
                int syncUpdateCount;
        };
    }
}
