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
#include <core/runtime/MessageQueue.h>
#include <core/sdk/IOutput.h>
#include <core/sdk/constants.h>

#include <thread>
#include <mutex>
#include <atomic>

#include <sigslot/sigslot.h>

namespace musik { namespace core { namespace audio {

    class Crossfader:
        private musik::core::runtime::IMessageTarget,
        private Player::EventListener
    {
        public:
            enum Direction { FadeIn, FadeOut };

            sigslot::signal0<> Emptied;

            Crossfader(ITransport& transport);
            virtual ~Crossfader();

            void Fade(
                Player* player,
                std::shared_ptr<musik::core::sdk::IOutput> output,
                Direction direction,
                long durationMs);

            void Cancel(Player* player, Direction direction);
            bool Contains(Player* player);
            void Pause();
            void Resume();
            void Stop();
            void Drain();

        private:
            void ThreadLoop();

            virtual void ProcessMessage(
                musik::core::runtime::IMessage &message);

            virtual void OnPlayerDestroying(musik::core::audio::Player* player);

            struct FadeContext {
                std::shared_ptr<musik::core::sdk::IOutput> output;
                Player* player;
                Direction direction;
                long ticksCounted;
                long ticksTotal;
            };

            using FadeContextPtr = std::shared_ptr<FadeContext>;

            std::recursive_mutex contextListLock;
            std::unique_ptr<std::thread> thread;
            musik::core::runtime::MessageQueue messageQueue;
            std::list<FadeContextPtr> contextList;
            std::atomic<bool> quit, paused;
            std::condition_variable_any drainCondition;
            ITransport& transport;
    };

} } }
