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

#define TICK_TIME_MILLIS 100

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

    this->thread = std::make_unique<std::thread>(
        std::bind(&Crossfader::ThreadLoop, this));
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
    float targetPercent,
    long durationMs)
{
    LOCK(this->contextListLock);

    std::shared_ptr<FadeContext> context = std::make_shared<FadeContext>();
    context->targetPercent = targetPercent;
    context->output = output;
    context->player = player;
    context->durationMs = durationMs;
    context->direction = direction;
    context->ticksCounted = 0;
    context->ticksTotal = context->durationMs / TICK_TIME_MILLIS;
    contextList.push_back(context);

    if (contextList.size() == 1) {
        ENQUEUE_TICK();
    }
}

void Crossfader::Reset() {
    LOCK(this->contextListLock);
    this->contextList.clear();
}

bool Crossfader::Remove(Player* player, Cut cut) {
    LOCK(this->contextListLock);

    bool found = false;

    if (cut == SoftCut) {
        std::for_each(
            this->contextList.begin(),
            this->contextList.end(),
            [player, &found](auto item) {
                /* don't worry about the player! it'll get cleaned up
                automatically. just worry about fading the output's volume
                to nothing. */
                if (item->player == player) {
                    item->direction = FadeOut;
                    item->player = nullptr;
                    found = true;
                }
            });
    }
    else {
        std::remove_if(
            this->contextList.begin(),
            this->contextList.end(),
            [player, &found](auto item) {
                /* hard cut stops the output and destroys the player
                immediately! */
                if (item->player == player) {
                    item->player->Destroy();
                    item->output->Stop();
                    found = true;
                }

                return item->player == player;
            });
    }

    return found;
}

void Crossfader::ProcessMessage(IMessage &message) {
    switch (message.Type()) {
        case MESSAGE_TICK: {
            LOCK(this->contextListLock);

            auto it = this->contextList.begin();
            auto globalVolume = this->transport.Volume();

            while (it != this->contextList.end()) {
                auto fade = *it;

                float percent =
                    (float) fade->ticksCounted /
                    (float) fade->ticksTotal;

                fade->output->SetVolume(globalVolume * percent);
                ++fade->ticksCounted;

                if (fade->ticksCounted >= fade->ticksTotal) {
                    /* contract: if we're fading the player out, we are responsible for
                    destroying it. if it gets destroyed before we get here, the reference
                    here will be null. */
                    if (fade->direction == FadeOut && fade->player) {
                        fade->player->Destroy();
                    }

                    it = this->contextList.erase(it);
                    continue;
                }

                ++it;
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