//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2007-2017 musikcube team
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

#include <core/audio/ITransport.h>
#include <core/audio/PlaybackService.h>

namespace musik {
    namespace core {
        namespace playback {
            void VolumeUp(musik::core::audio::ITransport& transport);
            void VolumeDown(musik::core::audio::ITransport& transport);
            void SeekForward(musik::core::sdk::IPlaybackService& playback);
            void SeekBack(musik::core::sdk::IPlaybackService& playback);
            void SeekForwardProportional(musik::core::sdk::IPlaybackService& playback);
            void SeekBackProportional(musik::core::sdk::IPlaybackService& playback);
            void PauseOrResume(musik::core::audio::ITransport& transport);

            void LoadPlaybackContext(
                std::shared_ptr<musik::core::Preferences> prefs,
                musik::core::ILibraryPtr library,
                musik::core::audio::PlaybackService& playback);

            void SavePlaybackContext(
                std::shared_ptr<musik::core::Preferences> prefs,
                musik::core::ILibraryPtr library,
                musik::core::audio::PlaybackService& playback);
        }
    }
}
