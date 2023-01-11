//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004-2021 musikcube team
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

#include <musikcore/sdk/constants.h>
#include <musikcore/audio/ITransport.h>
#include <musikcore/support/Preferences.h>

namespace musik { namespace core { namespace audio {
    class MasterTransport :
        public sigslot::has_slots<>,
        public musik::core::audio::ITransport
    {
        public:
            using Type = musik::core::sdk::TransportType;

            EXPORT MasterTransport() noexcept;

            EXPORT void Start(const std::string& uri, Gain gain, StartMode mode) override;
            EXPORT void PrepareNextTrack(const std::string& uri, Gain gain) override;

            EXPORT std::string Uri() override;

            EXPORT void Stop() override;
            EXPORT bool Pause() override;
            EXPORT bool Resume() override;

            EXPORT double Position() override;
            EXPORT void SetPosition(double seconds) override;

            EXPORT double Volume() override;
            EXPORT void SetVolume(double volume) override;

            EXPORT double GetDuration() override;

            EXPORT bool IsMuted() override;
            EXPORT void SetMuted(bool muted) override;

            EXPORT void ReloadOutput() override;

            EXPORT musik::core::sdk::PlaybackState GetPlaybackState() override;
            EXPORT musik::core::sdk::StreamState GetStreamState() override;

            EXPORT void SwitchTo(Type type);
            EXPORT Type GetType() noexcept;

        private:
            void OnStreamEvent(musik::core::sdk::StreamState type, std::string url);
            void OnPlaybackEvent(musik::core::sdk::PlaybackState type);
            void OnVolumeChanged();
            void OnTimeChanged(double time);

            std::shared_ptr<musik::core::audio::ITransport> transport;
            std::shared_ptr<musik::core::Preferences> prefs;
            Type type;
    };
} } }
