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
#include <musikcore/audio/PlaybackWorkers.h>
#include <musikcore/audio/Crossfader.h>
#include <musikcore/audio/Output.h>
#include <musikcore/audio/PlaybackTypes.h>

#include <thread>
#include <mutex>


namespace musik { namespace core { namespace audio {

    class CrossfadeTransport :
        public ITransport,
        private Player::EventListener
    {
        public:
            CrossfadeTransport();
            virtual ~CrossfadeTransport();

            void Start(const std::string& uri, Gain gain, StartMode mode) override;
            void PrepareNextTrack(const std::string& uri, Gain gain) override;

            std::string Uri() override;

            void Stop() override;
            void Destroy() override;
            mc_result Pause() override;
            mc_result Resume() override;

            double Position() override;
            void SetPosition(double seconds) override;

            double Volume() noexcept override;
            void SetVolume(double volume) override;

            bool IsMuted() noexcept override;
            void SetMuted(bool muted) override;

            double GetDuration() override;

            void ReloadOutput() override;

            musik::core::audio::PlaybackState GetPlaybackState() override;
            musik::core::audio::StreamState GetStreamState() override;

        private:
            using Lock = std::unique_lock<std::recursive_mutex>;
            using OutputPtr = std::shared_ptr<musik::core::audio::Output>;

            /* decoder readiness and the listener's requested action change independently. */
            enum class Intent { Prepare, Play, Pause };

            struct PlayerContext {
                PlayerContext(
                    CrossfadeTransport& transport,
                    Crossfader& crossfader) noexcept;

                void Reset();

                void Reset(
                    const std::string& url,
                    Gain gain);

                void TransferTo(PlayerContext& context) noexcept;

                void Start(double transportVolume);
                void Stop();
                void StopIf(Player const* player);
                void Play(double transportVolume);
                void SetVolume(double volume);
                bool IsEmpty() noexcept;

                bool started;
                bool canFade;
                OutputPtr output;
                Player::Shared player;
                CrossfadeTransport& transport;
                Crossfader& crossfader;
            };

            void RaiseStreamEvent(musik::core::audio::StreamState type, Player const* player);
            void SetPlaybackState(musik::core::audio::PlaybackState state);

            void OnCrossfaderEmptied();
            void StopImmediately();

            void OnPlayerBuffered(Player* player) override;
            void OnPlayerStarted(Player* player) override;
            void OnPlayerFinished(Player* player) override;
            void OnPlayerOpenFailed(Player* player) override;
            void OnPlayerMixPoint(Player* player, int id, double time) override;
            void OnPlayerStopped(Player* player) override;

            Intent intent { Intent::Prepare };
            musik::core::audio::PlaybackState playbackState;
            musik::core::audio::StreamState activePlayerState { musik::core::audio::StreamState::Stopped };
            std::recursive_mutex stateMutex;
            Crossfader crossfader;
            PlayerContext active;
            PlayerContext next;
            std::atomic<double> volume;
            std::atomic<bool> muted;
            std::atomic<bool> destroyed { false };
            PlaybackWorkers playbackWorkers { *this };
    };

} } }
