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

#include <musikcore/config.h>
#include <musikcore/audio/ITransport.h>
#include <musikcore/audio/Player.h>
#include <musikcore/sdk/IOutput.h>
#include <musikcore/sdk/constants.h>

#include <thread>
#include <mutex>

namespace musik { namespace core { namespace audio {

    class GaplessTransport :
        public ITransport,
        private Player::EventListener
    {
        public:
            GaplessTransport();
            virtual ~GaplessTransport();

            void Start(const std::string& uri, Gain gain, StartMode mode) override;
            void PrepareNextTrack(const std::string& uri, Gain gain) override;

            std::string Uri() override;

            void Stop() override;
            bool Pause() override;
            bool Resume() override;

            double Position() override;
            void SetPosition(double seconds) override;

            double Volume() noexcept override;
            void SetVolume(double volume) override;

            bool IsMuted() noexcept override;
            void SetMuted(bool muted) override;

            double GetDuration() override;

            void ReloadOutput() override;

            musik::core::sdk::PlaybackState GetPlaybackState() override;
            musik::core::sdk::StreamState GetStreamState() override;

        private:
            using LockT = std::unique_lock<std::recursive_mutex>;

            void StartWithPlayer(Player* player, StartMode mode = StartMode::Immediate);

            void StopInternal(
                bool suppressStopEvent,
                bool stopOutput,
                Player const* exclude = nullptr);

            void SetNextCanStart(bool nextCanStart);

            void RaiseStreamEvent(musik::core::sdk::StreamState type, Player const* player);
            void SetPlaybackState(musik::core::sdk::PlaybackState state);

            void OnPlayerStarted(Player* player) override;
            void OnPlayerBuffered(Player* player) override;
            void OnPlayerStreamEof(Player* player) override;
            void OnPlayerFinished(Player* player) override;
            void OnPlayerOpenFailed(Player* player) override;
            void OnPlayerDestroying(Player* player) override;

            void ResetActivePlayer();
            void ResetNextPlayer();

            musik::core::sdk::PlaybackState playbackState;
            musik::core::sdk::StreamState activePlayerState;
            std::recursive_mutex stateMutex;
            std::shared_ptr<musik::core::sdk::IOutput> output;
            Player* activePlayer;
            Player* nextPlayer;
            double volume;
            bool nextCanStart;
            bool muted;
    };

} } }
