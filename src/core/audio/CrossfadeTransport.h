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

#include <core/config.h>
#include <core/audio/ITransport.h>
#include <core/audio/Player.h>
#include <core/audio/Crossfader.h>
#include <core/runtime/MessageQueue.h>
#include <core/sdk/IOutput.h>
#include <core/sdk/constants.h>

#include <thread>
#include <mutex>

#include <sigslot/sigslot.h>

namespace musik { namespace core { namespace audio {

    class CrossfadeTransport :
        public ITransport,
        private Player::EventListener,
        public sigslot::has_slots<>
    {
        public:
            CrossfadeTransport();
            virtual ~CrossfadeTransport();

            void StopImmediately();

            virtual void Start(const std::string& uri, Gain gain, StartMode mode);
            virtual void PrepareNextTrack(const std::string& uri, Gain gain);

            virtual std::string Uri();

            virtual void Stop();
            virtual bool Pause();
            virtual bool Resume();

            virtual double Position();
            virtual void SetPosition(double seconds);

            virtual double Volume();
            virtual void SetVolume(double volume);

            virtual bool IsMuted();
            virtual void SetMuted(bool muted);

            virtual double GetDuration();

            virtual void ReloadOutput();

            virtual musik::core::sdk::PlaybackState GetPlaybackState();

        private:
            using Lock = std::unique_lock<std::recursive_mutex>;
            using Output = std::shared_ptr<musik::core::sdk::IOutput>;
            using Thread = std::shared_ptr<std::thread>;
            using IMessage = musik::core::runtime::IMessage;
            using IMessageTarget = musik::core::runtime::IMessageTarget;
            using MessageQueue = musik::core::runtime::MessageQueue;

            struct PlayerContext {
                PlayerContext(
                    CrossfadeTransport& transport,
                    Crossfader& crossfader);

                void Reset();

                void Reset(
                    const std::string& url,
                    Player::EventListener* listener,
                    Gain gain,
                    bool startImmediate);

                void TransferTo(PlayerContext& context);

                void Start(double transportVolume);
                void Stop();
                void StopIf(Player* player);
                void Pause();
                void Resume(double transportVolume);
                void SetVolume(double volume);
                bool IsEmpty();

                bool startImmediate;
                bool started;
                bool canFade;
                Output output;
                Player *player;
                CrossfadeTransport& transport;
                Crossfader& crossfader;
            };

            void RaiseStreamEvent(int type, Player* player);
            void SetPlaybackState(int state);

            void OnCrossfaderEmptied();

            virtual void OnPlayerPrepared(Player* player);
            virtual void OnPlayerStarted(Player* player);
            virtual void OnPlayerFinished(Player* player);
            virtual void OnPlayerError(Player* player);
            virtual void OnPlayerMixPoint(Player* player, int id, double time);

            musik::core::sdk::PlaybackState state;
            std::recursive_mutex stateMutex;
            Crossfader crossfader;
            PlayerContext active;
            PlayerContext next;
            double volume;
            bool muted;
    };

} } }
