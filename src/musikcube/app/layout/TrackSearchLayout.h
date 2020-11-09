//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004-2020 musikcube team
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

#include <cursespp/LayoutBase.h>
#include <cursespp/TextInput.h>

#include <app/window/TrackListView.h>

#include <musikcore/audio/PlaybackService.h>
#include <musikcore/library/ILibrary.h>
#include <musikcore/support/Preferences.h>

#include <sigslot/sigslot.h>

namespace musik {
    namespace cube {
        class TrackSearchLayout :
            public cursespp::LayoutBase,
            public sigslot::has_slots<>
        {
            public:
                TrackSearchLayout(
                    musik::core::audio::PlaybackService& playback,
                    musik::core::ILibraryPtr library);

                virtual ~TrackSearchLayout();

                virtual void OnVisibilityChanged(bool visible);
                virtual bool KeyPress(const std::string& key);

                void PlayFromTop();
                void FocusInput();
                void LoadLastSession();

            protected:
                virtual void ProcessMessage(musik::core::runtime::IMessage &message);
                virtual void OnLayout();

            private:
                using MatchType = musik::core::library::query::QueryBase::MatchType;

                void SaveSession();
                void InitializeWindows();
                void Requery();

                void OnRequeried(musik::core::library::query::TrackListQueryBase* query);

                void OnInputChanged(
                    cursespp::TextInput* sender,
                    std::string value);

                void OnEnterPressed(cursespp::TextInput* sender);

                void ToggleMatchType();
                void SetMatchType(MatchType matchType);

                musik::core::audio::PlaybackService& playback;
                musik::core::ILibraryPtr library;
                MatchType matchType{ MatchType::Substring };
                std::shared_ptr<musik::core::Preferences> prefs;
                std::shared_ptr<TrackListView> trackList;
                std::shared_ptr<cursespp::TextInput> input;
        };
    }
}
