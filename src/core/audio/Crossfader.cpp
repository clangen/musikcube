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

#include "pch.hpp"

#include <core/audio/Crossfader.h>
#include <core/runtime/Message.h>

#include <algorithm>
#include <chrono>

using namespace musik::core::audio;
using namespace musik::core::sdk;
using namespace musik::core::runtime;
using namespace std::chrono;

#define TICKS_PER_SECOND 30
#define TICK_TIME_MILLIS (1000 / TICKS_PER_SECOND)
#define MAX_FADES 3

#define ENQUEUE_TICK() \
    this->messageQueue.Post(Message::Create( \
        this, MESSAGE_TICK, 0, 0), TICK_TIME_MILLIS)

#define ENQUEUE_ADJUSTED_TICK(delay) \
    this->messageQueue.Post(Message::Create( \
        this, MESSAGE_TICK, 0, 0), delay > 0 ? delay : 0)

#define LOCK(x) \
    std::unique_lock<std::recursive_mutex> lock(x);

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

    /* don't add the same player more than once! */
    if (player && output && !this->Contains(player)) {
        std::shared_ptr<FadeContext> context = std::make_shared<FadeContext>();
        context->output = output;
        context->player = player;
        context->direction = direction;
        context->ticksCounted = 0;
        context->ticksTotal = (durationMs / TICK_TIME_MILLIS);
        contextList.push_back(context);

        player->Attach(this);

        /* for performance reasons we don't allow more than a couple
        simultaneous fades. mark extraneous ones as done so they are
        cleaned up during the next tick */
        int toRemove = (int) this->contextList.size() - MAX_FADES;
        if (toRemove > 0) {
            auto it = contextList.begin();
            for (int i = 0; i < toRemove; i++, it++) {
                (*it)->ticksCounted = (*it)->ticksTotal;
            }
        }

        if (contextList.size() == 1) {
            ENQUEUE_TICK();
        }
    }
}

void Crossfader::Stop() {
    LOCK(this->contextListLock);

    for (FadeContextPtr context : this->contextList) {
        if (context->player) {
            context->player->Detach(this);
            context->player->Destroy();
        }
        context->output->Stop();
    }

    this->contextList.clear();
}

void Crossfader::Drain() {
    LOCK(this->contextListLock);

    if (this->contextList.size()) {
        for (FadeContextPtr context : this->contextList) {
            context->direction = FadeOut;
        }

        this->drainCondition.wait(lock);
    }
}

void Crossfader::OnPlayerDestroying(Player* player) {
    if (player) {
        LOCK(this->contextListLock);

        /* the player is destroying, but there may still be buffers
        playing in the output. forget the player so we don't double
        destroy it. */
        for (FadeContextPtr context : this->contextList) {
            if (context->player == player) {
                context->player = nullptr;
            }
        }
    }
}

void Crossfader::Cancel(Player* player, Direction direction) {
    if (player) {
        LOCK(this->contextListLock);

        this->contextList.remove_if(
            [player, direction, this](FadeContextPtr context) {
                bool remove =
                    context->player == player &&
                    context->direction == direction;

                if (remove && context->player) {
                    context->player->Detach(this);
                }

                return remove;
            });
    }
}

bool Crossfader::Contains(Player* player) {
    if (!player) {
        return false;
    }

    LOCK(this->contextListLock);

    return std::any_of(
        this->contextList.begin(),
        this->contextList.end(),
            [player](FadeContextPtr context) {
            return player == context->player;
        });
}

void Crossfader::Pause() {
    LOCK(this->contextListLock);

    this->paused = true;

    for (FadeContextPtr context : this->contextList) {
        context->output->Pause();
    }

    this->messageQueue.Remove(this, MESSAGE_TICK);
}

void Crossfader::Resume() {
    LOCK(this->contextListLock);

    this->paused = false;

    for (FadeContextPtr context : this->contextList) {
        context->output->Resume();
    }

    this->messageQueue.Post(
        Message::Create(this, MESSAGE_TICK, 0, 0), 0);
}

void Crossfader::ProcessMessage(IMessage &message) {
    switch (message.Type()) {
        case MESSAGE_TICK: {
            bool emptied = false;

            auto start = system_clock::now().time_since_epoch();

            {
                LOCK(this->contextListLock);

                auto it = this->contextList.begin();
                auto globalVolume = this->transport.Volume();

                while (it != this->contextList.end()) {
                    auto fade = *it;

                    if (fade->ticksCounted < fade->ticksTotal) {
                        ++fade->ticksCounted;

                        if (this->transport.IsMuted()) {
                            fade->output->SetVolume(0.0);
                        }
                        else {
                            double percent =
                                (float)fade->ticksCounted /
                                (float)fade->ticksTotal;

                            if (fade->direction == FadeOut) {
                                percent = (1.0f - percent);
                            }

                            double outputVolume = globalVolume * percent;
                            fade->output->SetVolume(outputVolume);
                        }
                    }

                    /* if the fade has finished... */
                    if (fade->ticksCounted >= fade->ticksTotal) {
                        auto player = (*it)->player;

                        /* we're done with this player now! detach ourself */
                        if (player) {
                            (*it)->player->Detach(this);
                        }

                        if (fade->direction == FadeOut) {
                            /* if we're fading the player out, we get to destroy
                            it. go ahead and do that now. awkward but efficient,
                            and it works. */
                            if (player) {
                                (*it)->player->Destroy();
                            }

                            /* wait for the output to finish playing what it has
                            buffered -- but do it in the background because it's
                            a blocking call. */
                            auto output = (*it)->output;
                            std::thread drainThread([output]() {
                                output->Drain();
                                output->Stop();
                            });
                            drainThread.detach();
                        }

                        it = this->contextList.erase(it);
                    }
                    else {
                        ++it;
                    }
                }

                emptied = (this->contextList.size() == 0);
            } /* end critical section */

            /* notify outside of the critical section! */
            if (emptied) {
                this->Emptied();
                this->drainCondition.notify_all();
            }
            else {
                auto end = system_clock::now().time_since_epoch();
                int64_t duration = duration_cast<milliseconds>(end - start).count();
                ENQUEUE_ADJUSTED_TICK(TICK_TIME_MILLIS - duration);
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