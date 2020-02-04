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
#include <core/audio/IStream.h>
#include <core/sdk/constants.h>
#include <core/sdk/IOutput.h>
#include <core/sdk/IBufferProvider.h>

#include <sigslot/sigslot.h>

#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>

namespace musik { namespace core { namespace audio {

    struct FftContext;

    class Player : public musik::core::sdk::IBufferProvider {
        public:
            enum DestroyMode { Drain = 0, NoDrain = 1 };

            struct Gain {
                Gain() {
                    this->preamp = this->gain = this->peak = 1.0f;
                    this->peakValid = false;
                }
                float preamp;
                float gain;
                float peak;
                bool peakValid;
            };

            struct EventListener {
                virtual void OnPlayerPrepared(Player *player) { }
                virtual void OnPlayerStarted(Player *player) { }
                virtual void OnPlayerAlmostEnded(Player *player) { }
                virtual void OnPlayerFinished(Player *player) { }
                virtual void OnPlayerError(Player *player) { }
                virtual void OnPlayerDestroying(Player *player) { }
                virtual void OnPlayerMixPoint(Player *player, int id, double time) { }
            };

            static Player* Create(
                const std::string &url,
                std::shared_ptr<musik::core::sdk::IOutput> output,
                DestroyMode destroyMode,
                EventListener *listener,
                Gain gain = Gain());

            virtual void OnBufferProcessed(musik::core::sdk::IBuffer *buffer);

            void Detach(EventListener *listener);
            void Attach(EventListener *listener);

            void Play();
            void Destroy();
            void Destroy(DestroyMode mode);

            double GetPosition();
            void SetPosition(double seconds);
            double GetDuration();

            void AddMixPoint(int id, double time);

            bool HasCapability(musik::core::sdk::Capability capability);

            std::string GetUrl() const { return this->url; }

        private:
            friend void playerThreadLoop(Player* player);

            double GetPositionInternal();

            Player(
                const std::string &url,
                std::shared_ptr<musik::core::sdk::IOutput> output,
                DestroyMode finishMode,
                EventListener *listener,
                Gain gain);

            virtual ~Player();

            struct MixPoint {
                MixPoint(int id, double time) {
                    this->id = id;
                    this->time = time;
                }

                int id;
                double time;
            };

            using MixPointPtr = std::shared_ptr<MixPoint>;
            using MixPointList = std::list<MixPointPtr>;
            using ListenerList = std::list<EventListener*>;
            using OutputPtr = std::shared_ptr<musik::core::sdk::IOutput>;

            typedef enum {
                Idle = 0,
                Playing = 1,
                Quit = 2
            } States;

            bool Exited();
            int State();
            ListenerList Listeners();

            std::thread* thread;

            OutputPtr output;
            IStreamPtr stream;
            ListenerList listeners;
            MixPointList pendingMixPoints;
            MixPointList processedMixPoints;
            MixPointList mixPointsHitTemp; /* so we don't have to keep alloc'ing it */

            void UpdateNextMixPointTime();

            std::string url;

            /* granular mutexes for better performance */
            std::mutex queueMutex, listenerMutex;
            std::condition_variable writeToOutputCondition;

            double volume;
            double nextMixPoint;
            std::atomic<double> currentPosition;
            std::atomic<double> seekToPosition;
            int state;
            bool notifiedStarted;
            float* spectrum;
            DestroyMode destroyMode;
            Gain gain;
            int pendingBufferCount;
            bool threadFinished;

            FftContext* fftContext;
    };

} } }
