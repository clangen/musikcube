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

#include <musikcore/audio/ITransport.h>
#include <musikcore/audio/Player.h>
#include <musikcore/audio/Output.h>

#include <chrono>
#include <condition_variable>
#include <functional>
#include <list>
#include <thread>
#include <mutex>

namespace musik { namespace core { namespace audio {

    class Crossfader {
    public:
        enum Direction { FadeIn, FadeOut };

        Crossfader(ITransport& transport, std::function<void()> onEmptied);

        ~Crossfader();

        void Fade(Player::Shared player, std::shared_ptr<Output> output, Direction direction, long durationMs);
        void Cancel(Player* player, Direction direction);
        bool Contains(Player* player) const;
        bool Empty() const;
        bool StopPausedFades();
        mc_result SetPaused(bool pause, Output& activeOutput);

        /* a paused tail cannot finish its fade. stop remaining outputs and
        join the fade worker before releasing the transport. */
        void Shutdown();

    private:
        using Clock = std::chrono::steady_clock;
        using Duration = std::chrono::duration<double>;

        struct FadeContext {
            std::shared_ptr<Output> output;
            Player::Shared player;
            Direction direction;
            Duration duration;
            Duration elapsed;
            Clock::time_point started;
        };

        void StopFades();
        void Finish(FadeContext& fade);
        void ThreadLoop();

        mutable std::mutex mutex;
        std::condition_variable changed;
        std::list<FadeContext> fades;
        bool quit = false;
        bool paused = false;
        ITransport& transport;
        const std::function<void()> onEmptied;
        std::thread thread;
    };

} } }
