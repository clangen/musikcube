//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2007-2016 musikcube team
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

#include "pch.hpp"

#include <core/audio/Crossfader.h>
#include <core/runtime/Message.h>
#include <algorithm>

using namespace musik::core::audio;
using namespace musik::core::sdk;
using namespace musik::core::runtime;

#define TICKS_PER_SECOND 10
#define TICK_TIME_MILLIS (1000 / TICKS_PER_SECOND)

#define ENQUEUE_TICK() \
    this->messageQueue.Post(Message::Create( \
        this, MESSAGE_TICK, 0, 0), TICK_TIME_MILLIS)

#define LOCK(x) \
    std::unique_lock<std::mutex> lock(x);

#define MESSAGE_QUIT 0
#define MESSAGE_TICK 1

Crossfader::Crossfader(ITransport& transport)
: transport(transport) {
    this->quit = false;
    this->paused = false;

    this->thread.reset(new std::thread(
        std::bind(&Crossfader::ThreadLoop, this)));
}

Crossfader::~Crossfader() {
    this->quit = true;
    this->messageQueue.Post(Message::Create(this, MESSAGE_QUIT, 0, 0));
    this->thread->join();
}

void Crossfader::Fade(
    Player* player,
    std::shared_ptr<IOutput> output,
    Direction direction,
    long durationMs)
{
    LOCK(this->contextListLock);

    std::shared_ptr<FadeContext> context = std::make_shared<FadeContext>();
    context->output = output;
    context->player = player;
    context->direction = direction;
    context->ticksCounted = 0;
    context->ticksTotal = (durationMs / TICK_TIME_MILLIS);
    contextList.push_back(context);

    if (contextList.size() == 1) {
        ENQUEUE_TICK();
    }
}

void Crossfader::Stop() {
    LOCK(this->contextListLock);

    auto it = this->contextList.begin();
    while (it != this->contextList.end()) {
        if ((*it)->player) {
            (*it)->player->Destroy();
        }

        (*it)->output->Stop();
        ++it;
    }

    this->contextList.clear();
}

void Crossfader::Pause() {
    LOCK(this->contextListLock);

    this->paused = true;

    std::for_each(
        this->contextList.begin(),
        this->contextList.end(),
        [](FadeContextPtr context) {
            context->output->Pause();
        });

    this->messageQueue.Remove(this, MESSAGE_TICK);
}

void Crossfader::Resume() {
    LOCK(this->contextListLock);

    this->paused = false;

    std::for_each(
        this->contextList.begin(),
        this->contextList.end(),
        [](FadeContextPtr context) {
            context->output->Resume();
        });

    this->messageQueue.Post(
        Message::Create(this, MESSAGE_TICK, 0, 0), 0);
}

void Crossfader::Reset() {
    LOCK(this->contextListLock);
    this->contextList.clear();
}

#include <boost/format.hpp>

void Crossfader::ProcessMessage(IMessage &message) {
    switch (message.Type()) {
        case MESSAGE_TICK: {
            LOCK(this->contextListLock);

            auto it = this->contextList.begin();
            auto globalVolume = this->transport.Volume();

            while (it != this->contextList.end()) {
                auto fade = *it;
                ++fade->ticksCounted;

                double percent =
                    (float) fade->ticksCounted /
                    (float) fade->ticksTotal;

                if (fade->direction == FadeOut) {
                    percent = (1.0f - percent);
                }

                float outputVolume = globalVolume * percent;

#if 0
                std::string dir = (fade->direction == FadeIn) ? "in" : "out";
                std::string dbg = boost::str(boost::format("%s %f\n") % dir % outputVolume);
                OutputDebugStringA(dbg.c_str());
#endif

                fade->output->SetVolume(outputVolume);

                if (fade->ticksCounted >= fade->ticksTotal) {
                    if (fade->direction == FadeOut) {
                        (*it)->player->Destroy();
                        (*it)->output->Stop();
                    }

                    it = this->contextList.erase(it);
                }
                else {
                    ++it;
                }
            }

            if (this->contextList.size()) {
                ENQUEUE_TICK();
            }
        }
        break;
    }
}

void Crossfader::ThreadLoop() {
    while (!this->quit) {
        messageQueue.WaitAndDispatch();
    }
}