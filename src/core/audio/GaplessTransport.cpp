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

#include <core/debug.h>
#include <core/audio/GaplessTransport.h>
#include <core/plugin/PluginFactory.h>
#include <core/audio/Outputs.h>
#include <algorithm>

using namespace musik::core::audio;
using namespace musik::core::sdk;

static std::string TAG = "GaplessTransport";

#define RESET_NEXT_PLAYER(instance) \
    if (instance->nextPlayer) { \
        instance->nextPlayer->Detach(instance); \
        instance->nextPlayer->Destroy(); \
        instance->nextPlayer = nullptr; \
    }

#define RESET_ACTIVE_PLAYER(instance) \
    if (instance->activePlayer) { \
        instance->activePlayer->Detach(instance); \
        instance->activePlayer->Destroy(); \
        instance->activePlayer = nullptr; \
    }

GaplessTransport::GaplessTransport()
: volume(1.0)
, state(PlaybackStopped)
, activePlayer(nullptr)
, nextPlayer(nullptr)
, nextCanStart(false)
, muted(false) {
    this->output = outputs::SelectedOutput();
}

GaplessTransport::~GaplessTransport() {
    LockT lock(this->stateMutex);
    RESET_NEXT_PLAYER(this);
    RESET_ACTIVE_PLAYER(this);
}

PlaybackState GaplessTransport::GetPlaybackState() {
    LockT lock(this->stateMutex);
    return this->state;
}

void GaplessTransport::PrepareNextTrack(const std::string& uri, Gain gain) {
    bool startNext = false;
    {
        LockT lock(this->stateMutex);

        RESET_NEXT_PLAYER(this);

        if (uri.size()) {
            this->nextPlayer = Player::Create(uri, this->output, Player::NoDrain, this, gain);
            startNext = this->nextCanStart;
        }
    }

    if (startNext) {
        this->StartWithPlayer(this->nextPlayer);
    }
}

void GaplessTransport::Start(const std::string& uri, Gain gain, StartMode mode) {
    musik::debug::info(TAG, "starting track at " + uri);
    Player* newPlayer = Player::Create(uri, this->output, Player::NoDrain, this, gain);
    this->StartWithPlayer(newPlayer, mode);
}

void GaplessTransport::StartWithPlayer(Player* newPlayer, StartMode mode) {
    if (newPlayer) {
        bool playingNext = false;

        {
            LockT lock(this->stateMutex);

            playingNext = (newPlayer == nextPlayer);
            if (newPlayer != nextPlayer) {
                RESET_NEXT_PLAYER(this);
            }

            RESET_ACTIVE_PLAYER(this);

            this->nextPlayer = nullptr;
            this->activePlayer = newPlayer;
        }

        /* first argument suppresses the "Stop" event from getting triggered,
        the second param is used for gapless playback -- we won't stop the output
        and will allow pending buffers to finish if we're not automatically
        playing the next track. note we do this outside of critical section so
        outputs *can* stop buffers immediately, and not to worry about causing a
        deadlock. */
        this->StopInternal(true, !playingNext, newPlayer);
        this->SetNextCanStart(false);
        this->output->Resume();

        if (mode == StartMode::Immediate) {
            newPlayer->Play();
        }

        this->RaiseStreamEvent(StreamScheduled, newPlayer);
    }
}

void GaplessTransport::ReloadOutput() {
    this->Stop();
    this->output = outputs::SelectedOutput();
    this->output->SetVolume(volume);
}

void GaplessTransport::Stop() {
    this->StopInternal(false, true);
}

std::string GaplessTransport::Uri() {
    auto player = this->activePlayer;
    return player ? player->GetUrl() : "";
}

void GaplessTransport::StopInternal(
    bool suppressStopEvent,
    bool stopOutput,
    Player* exclude)
{
    musik::debug::info(TAG, "stop");

    /* if we stop the output, we kill all of the Players immediately.
    otherwise, we let them finish naturally; RemoveActive() will take
    care of disposing of them */
    if (stopOutput) {
        {
            LockT lock(this->stateMutex);

            RESET_NEXT_PLAYER(this);
            if (this->activePlayer != exclude) {
                RESET_ACTIVE_PLAYER(this);
            }
        }

        /* stopping the transport will stop any buffers that are currently in
        flight. this makes the sound end immediately. */
        this->output->Stop();
    }

    if (!suppressStopEvent) {
        /* if we know we're starting another track immediately, suppress
        the stop event. this functionality is not available to the public
        interface, it's an internal optimization */
        this->SetPlaybackState(PlaybackStopped);
    }
}

bool GaplessTransport::Pause() {
    musik::debug::info(TAG, "pause");

    this->output->Pause();

    if (this->activePlayer) {
        this->SetPlaybackState(PlaybackPaused);
        return true;
    }

    return false;
}

bool GaplessTransport::Resume() {
    musik::debug::info(TAG, "resume");

    this->output->Resume();

    {
        LockT lock(this->stateMutex);

        if (this->activePlayer) {
            this->activePlayer->Play();
        }
    }

    if (this->activePlayer) {
        this->SetPlaybackState(PlaybackPlaying);
        return true;
    }

    return false;
}

double GaplessTransport::Position() {
    LockT lock(this->stateMutex);

    if (this->activePlayer) {
        return this->activePlayer->GetPosition();
    }

    return 0;
}

void GaplessTransport::SetPosition(double seconds) {
    {
        LockT lock(this->stateMutex);

        if (this->activePlayer) {
            if (this->state != PlaybackPlaying) {
                this->SetPlaybackState(PlaybackPlaying);
            }
            this->activePlayer->SetPosition(seconds);
        }
    }

    if (this->activePlayer) {
        this->TimeChanged(seconds);
    }
}

double GaplessTransport::GetDuration() {
    LockT lock(this->stateMutex);
    return this->activePlayer ? this->activePlayer->GetDuration() : -1.0f;
}

bool GaplessTransport::IsMuted() {
    return this->muted;
}

void GaplessTransport::SetMuted(bool muted) {
    if (this->muted != muted) {
        this->muted = muted;
        this->output->SetVolume(muted ? 0.0f : this->volume);
        this->VolumeChanged();
    }
}

double GaplessTransport::Volume() {
    return this->volume;
}

void GaplessTransport::SetVolume(double volume) {
    double oldVolume = this->volume;

    volume = std::max(0.0, std::min(1.0, volume));

    this->volume = volume;
    this->output->SetVolume(this->volume);

    if (oldVolume != this->volume) {
        this->SetMuted(false);
        this->VolumeChanged();
    }
}

void GaplessTransport::SetNextCanStart(bool nextCanStart) {
    LockT lock(this->stateMutex);
    this->nextCanStart = nextCanStart;
}

void GaplessTransport::OnPlayerPrepared(Player* player) {
    if (player == this->activePlayer) {
        this->RaiseStreamEvent(StreamPrepared, player);
        this->SetPlaybackState(PlaybackPrepared);
    }
}

void GaplessTransport::OnPlayerStarted(Player* player) {
    this->RaiseStreamEvent(StreamPlaying, player);
    this->SetPlaybackState(PlaybackPlaying);
}

void GaplessTransport::OnPlayerAlmostEnded(Player* player) {
    this->SetNextCanStart(true);

    {
        LockT lock(this->stateMutex);

        /* if another component configured a next player while we were playing,
        go ahead and get it started now. */
        if (this->nextPlayer) {
            this->StartWithPlayer(this->nextPlayer);
        }
    }

    this->RaiseStreamEvent(StreamAlmostDone, player);
}

void GaplessTransport::OnPlayerFinished(Player* player) {
    this->RaiseStreamEvent(StreamFinished, player);

    bool stopped = false;

    {
        LockT lock(this->stateMutex);

        bool startedNext = false;
        bool playerIsActive = (player == this->activePlayer);

        /* only start the next player if the currently active player is the
        one that just finished. */
        if (playerIsActive && this->nextPlayer) {
            this->StartWithPlayer(this->nextPlayer);
            startedNext = true;
        }

        if (!startedNext) {
            stopped = playerIsActive;
        }
    }

    if (stopped) {
        /* note we call through to StopInternal() because we don't
        want to stop the output immediately, it may still have some
        trailing samples queued up */
        this->StopInternal(false, false);
    }
}

void GaplessTransport::OnPlayerError(Player* player) {
    if (player == this->activePlayer) {
        this->RaiseStreamEvent(StreamError, player);
        this->SetPlaybackState(PlaybackStopped);
    }
    else if (player == this->nextPlayer) {
        RESET_NEXT_PLAYER(this);
    }
}

void GaplessTransport::OnPlayerDestroying(Player *player) {
    LockT lock(this->stateMutex);

    if (player == this->activePlayer) {
        RESET_ACTIVE_PLAYER(this);
        return;
    }
}

void GaplessTransport::SetPlaybackState(int state) {
    bool changed = false;

    {
        LockT lock(this->stateMutex);
        changed = (this->state != state);
        this->state = (PlaybackState) state;
    }

    if (changed) {
        this->PlaybackEvent(state);
    }
}

void GaplessTransport::RaiseStreamEvent(int type, Player* player) {
    this->StreamEvent(type, player->GetUrl());
}
