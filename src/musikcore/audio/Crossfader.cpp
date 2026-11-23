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

#include "pch.hpp"

#include <musikcore/audio/Crossfader.h>

#include <algorithm>
#include <chrono>

using namespace musik::core::audio;

namespace {
    constexpr auto TickInterval = std::chrono::milliseconds(1000 / 30);
    constexpr size_t MaximumFades = 3;
}

Crossfader::Crossfader(ITransport& transport, std::function<void()> onEmptied)
: transport(transport)
, onEmptied(std::move(onEmptied))
, thread([this] {
    this->ThreadLoop();
}) {
}

Crossfader::~Crossfader() {
    this->Shutdown();
}

void Crossfader::Shutdown() {
    if (!this->thread.joinable()) {
        return;
    }
    {
        std::lock_guard lock(this->mutex);
        this->quit = true;
        this->StopFades();
    }
    this->changed.notify_one();
    this->thread.join();
}

void Crossfader::Fade(Player::Shared player, std::shared_ptr<Output> output, Direction direction, long durationMs) {
    if (!player || !output) {
        return;
    }
    std::lock_guard lock(this->mutex);
    if (this->quit ||
        std::any_of(this->fades.begin(), this->fades.end(),
            [&](const auto& fade) {
                return fade.player == player;
            }))
    {
        return;
    }

    /* finish the oldest envelope before admitting another, so rapid skips
    cannot leave an unbounded set of audible tails. */
    if (this->fades.size() == MaximumFades) {
        this->Finish(this->fades.front());
        this->fades.pop_front();
    }
    this->fades.push_back({
        .output = std::move(output),
        .player = std::move(player),
        .direction = direction,
        .duration = std::chrono::milliseconds(std::max(0L, durationMs)),
        .elapsed = Duration::zero(),
        .started = Clock::now(),
    });
    this->changed.notify_one();
}

void Crossfader::Cancel(Player* player, Direction direction) {
    std::lock_guard lock(this->mutex);
    this->fades.remove_if([&](const auto& fade) {
        return fade.player.get() == player && fade.direction == direction;
    });
    this->changed.notify_one();
}

bool Crossfader::Contains(Player* player) const {
    std::lock_guard lock(this->mutex);
    return std::any_of(this->fades.begin(), this->fades.end(), [&](const auto& fade) {
        return fade.player.get() == player;
    });
}

bool Crossfader::Empty() const {
    std::lock_guard lock(this->mutex);
    return this->fades.empty();
}

/* a paused tail cannot fade to silence. stopping it now keeps the next
track from waking an output the listener has already stopped. */
bool Crossfader::StopPausedFades() {
    std::lock_guard lock(this->mutex);
    if (!this->paused) {
        return false;
    }
    this->StopFades();
    this->paused = false;
    this->changed.notify_one();
    return true;
}

void Crossfader::StopFades() {
    for (auto& fade : this->fades) {
        fade.player->RequestStop();
        fade.output->Stop();
    }
    this->fades.clear();
}

mc_result Crossfader::SetPaused(bool pause, Output& activeOutput) {
    std::lock_guard lock(this->mutex);
    const auto apply = [](Output& output, bool pause) {
        return pause ? output.Pause() : output.Resume();
    };

    const auto result = apply(activeOutput, pause);
    if (result != MC_OK || this->paused == pause) {
        return result;
    }

    /* the selected track owns the command. a tail that cannot follow it
    must go silent, or the listener could still hear it after pausing. */
    const auto now = Clock::now();
    for (auto fade = this->fades.begin(); fade != this->fades.end();) {
        if (fade->output.get() != &activeOutput && apply(*fade->output, pause) != MC_OK) {
            fade->player->RequestStop();
            fade->output->Stop();
            fade = this->fades.erase(fade);
            continue;
        }
        if (pause) {
            fade->elapsed += now - fade->started;
        }
        else {
            fade->started = now;
        }
        ++fade;
    }
    this->paused = pause;
    this->changed.notify_one();
    return MC_OK;
}

void Crossfader::Finish(FadeContext& fade) {
    const auto volume = this->transport.IsMuted() || fade.direction == FadeOut ? 0.0 : this->transport.Volume();
    const auto result = fade.output->SetVolume(volume);
    if (fade.direction == FadeOut) {
        if (this->paused || result != MC_OK) {
            /* an evicted paused tail cannot resume here. a blocked or failed
            gain change also means the tail could remain audible indefinitely. */
            fade.player->RequestStop(Player::Drain::No);
            fade.output->Stop();
            return;
        }
        /* the job owns the buffered tail until it drains. */
        fade.player->RequestStop(Player::Drain::Yes);
    }
}

void Crossfader::ThreadLoop() {
    std::unique_lock lock(this->mutex);
    while (!this->quit) {
        this->changed.wait(lock, [&] {
            return this->quit || (!this->paused && !this->fades.empty());
        });
        if (this->quit) {
            break;
        }

        /* one deadline owns each update. notifications cannot enqueue extra
        ticks, and a slow backend cannot build a backlog of missed ticks. */
        const auto deadline = Clock::now() + TickInterval;
        if (this->changed.wait_until(lock, deadline,
                [&] {
                    return this->quit || this->paused || this->fades.empty();
                }))
        {
            continue;
        }

        const auto now = Clock::now();
        const auto volume = this->transport.IsMuted() ? 0.0 : this->transport.Volume();
        auto fade = this->fades.begin();
        while (fade != this->fades.end()) {
            const auto elapsed = fade->elapsed + (now - fade->started);
            const auto fraction = fade->duration > Duration::zero() ? std::min(1.0, elapsed / fade->duration) : 1.0;
            if (fraction >= 1.0) {
                this->Finish(*fade);
                fade = this->fades.erase(fade);
            }
            else {
                const auto gain = fade->direction == FadeOut ? 1.0 - fraction : fraction;
                fade->output->SetVolume(volume * gain);
                ++fade;
            }
        }
        if (this->fades.empty()) {
            /* transport callbacks may start another fade. */
            lock.unlock();
            this->onEmptied();
            lock.lock();
        }
    }
}
