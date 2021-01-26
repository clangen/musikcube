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

            MasterTransport() noexcept;

            void Start(const std::string& uri, Gain gain, StartMode mode) override;
            void PrepareNextTrack(const std::string& uri, Gain gain) override;

            std::string Uri() override;

            void Stop() override;
            bool Pause() override;
            bool Resume() override;

            double Position() override;
            void SetPosition(double seconds) override;

            double Volume() override;
            void SetVolume(double volume) override;

            double GetDuration() override;

            bool IsMuted() override;
            void SetMuted(bool muted) override;

            void ReloadOutput() override;

            musik::core::sdk::PlaybackState GetPlaybackState() override;
            musik::core::sdk::StreamState GetStreamState() override;

            void SwitchTo(Type type);
            Type GetType() noexcept;

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
