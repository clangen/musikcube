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

#include <core/debug.h>
#include <core/audio/CrossfadeTransport.h>
#include <core/plugin/PluginFactory.h>
#include <core/audio/Outputs.h>
#include <algorithm>

#define CROSSFADE_DURATION_MS 3000
#define END_OF_TRACK_MIXPOINT 1001

using namespace musik::core::audio;
using namespace musik::core::sdk;

static std::string TAG = "CrossfadeTransport";

CrossfadeTransport::CrossfadeTransport()
: volume(1.0)
, state(PlaybackStopped)
, nextCanStart(false)
, muted(false)
, crossfader(*this)
, active(crossfader)
, next(crossfader) {

}

CrossfadeTransport::~CrossfadeTransport() {
    this->disconnect_all();
    this->Stop();
}

PlaybackState CrossfadeTransport::GetPlaybackState() {
    Lock lock(this->stateMutex);
    return this->state;
}

void CrossfadeTransport::PrepareNextTrack(const std::string& trackUrl) {
    Lock lock(this->stateMutex);
    this->next.Reset(trackUrl, this);
}

void CrossfadeTransport::Start(const std::string& url) {
    {
        Lock lock(this->stateMutex);

        musik::debug::info(TAG, "trying to play " + url);

        this->next.Stop();
        this->active.Reset(url, this);
    }

    this->RaiseStreamEvent(StreamScheduled, this->active.player);
}

void CrossfadeTransport::ReloadOutput() {
    this->Stop();
}

void CrossfadeTransport::Stop() {
    {
        Lock lock(this->stateMutex);
        this->crossfader.Stop();
        this->active.Stop();
        this->next.Stop();
    }

    this->SetPlaybackState(PlaybackStopped);
}

bool CrossfadeTransport::Pause() {
    {
        Lock lock(this->stateMutex);
        this->crossfader.Pause();
        this->active.Pause();
    }

    if (this->active.player) {
        this->SetPlaybackState(PlaybackPaused);
        return true;
    }

    return false;
}

bool CrossfadeTransport::Resume() {
    {
        Lock lock(this->stateMutex);
        this->crossfader.Resume();
        this->active.Resume();
    }

    if (this->active.player) {
        this->SetPlaybackState(PlaybackPlaying);
        return true;
    }

    return false;
}

double CrossfadeTransport::Position() {
    Lock lock(this->stateMutex);

    if (this->active.player) {
        return this->active.player->GetPosition();
    }

    return 0;
}

void CrossfadeTransport::SetPosition(double seconds) {
    {
        Lock lock(this->stateMutex);

        if (this->active.player) {
            this->active.player->SetPosition(seconds);
        }
    }

    if (this->active.player) {
        this->TimeChanged(seconds);
    }
}

double CrossfadeTransport::GetDuration() {
    Lock lock(this->stateMutex);
    return this->active.player ? this->active.player->GetDuration() : -1.0f;
}

bool CrossfadeTransport::IsMuted() {
    return this->muted;
}

void CrossfadeTransport::SetMuted(bool muted) {
    if (this->muted != muted) {
        this->muted = muted;

        /* unconditionally set the volume to zero when we mute. */
        if (muted) {
            this->active.SetVolume(0.0f);
            this->next.SetVolume(0.0f);
        }
        /* if we're no longer muted, and the streams aren't currently
        in the crossfader, set their volume appropriately. */
        else {
            if (!crossfader.Contains(this->active.player)) {
                this->active.SetVolume(this->volume);
            }

            if (!crossfader.Contains(this->next.player)) {
                this->next.SetVolume(this->volume);
            }
        }

        this->VolumeChanged();
    }
}

double CrossfadeTransport::Volume() {
    return this->volume;
}

void CrossfadeTransport::SetVolume(double volume) {
    double oldVolume = this->volume;

    volume = std::max(0.0, std::min(1.0, volume));

    this->volume = volume;
    if (oldVolume != this->volume) {
        this->VolumeChanged();
    }

    {
        Lock lock(this->stateMutex);
        active.SetVolume(volume);
        next.SetVolume(volume);
    }
}

void CrossfadeTransport::SetNextCanStart(bool nextCanStart) {
    //LockT lock(this->stateMutex);
    //this->nextCanStart = nextCanStart;
}

void CrossfadeTransport::OnPlayerPrepared(Player* player) {
    {
        Lock lock(this->stateMutex);

        int durationMs = (int)(player->GetDuration() * 1000.0f);
        double mixpointOffset = 0.0f;
        bool canFade = (durationMs > CROSSFADE_DURATION_MS * 4);

        /* if the track isn't long enough don't set a mixpoint, and
        disable the crossfade functionality */
        if (canFade) {
            mixpointOffset =
                player->GetDuration() -
                ((float) CROSSFADE_DURATION_MS / 1000.0f);
        }

        /* set up a mix point so we're notified when the track is
        about to end. we'll use this to fade it out */
        if (canFade) {
            player->AddMixPoint(END_OF_TRACK_MIXPOINT, mixpointOffset);
        }

        if (player == active.player) {
            active.canFade = canFade;
            active.Start(this->volume);
        }
        else if (player == next.player) {
            next.canFade = canFade;
        }
    }
}

void CrossfadeTransport::OnPlayerStarted(Player* player) {
    this->RaiseStreamEvent(StreamPlaying, player);
    this->SetPlaybackState(PlaybackPlaying);
}

void CrossfadeTransport::OnPlayerAlmostEnded(Player* player) {
    /* we don't need to do anything here. we'll handle starting
    the next track in OnPlayerFinished() or OnPlayerMixpoint(),
    whichever is applicable for the current stream. */
}

void CrossfadeTransport::OnPlayerFinished(Player* player) {
    this->RaiseStreamEvent(StreamFinished, player);

    /* we only need to handle this event if `canFade` is set to
    false. otherwise it was taken care of in OnPlayerMixPoint() */
    if (player == active.player && !active.canFade) {
        if (next.player && next.output) {
            next.TransferTo(active);
            active.Start(this->volume);
        }
        else {
            this->Stop();
        }
    }
}

void CrossfadeTransport::OnPlayerError(Player* player) {
    this->RaiseStreamEvent(StreamError, player);
    this->Stop();
}

void CrossfadeTransport::OnPlayerDestroying(Player *player) {
    Lock lock(this->stateMutex);

    crossfader.OnPlayerDestroyed(player);

    if (active.player == player) {
        active.Reset();
    }

    if (next.player == player) {
        next.Reset();
    }
}

void CrossfadeTransport::OnPlayerMixPoint(Player* player, int id, double time) {
    if (id == END_OF_TRACK_MIXPOINT) {
        if (player == active.player) {
            active.Reset(); /* fades out automatically */
            next.TransferTo(active);
            active.Start(this->volume);
        }
    }
}

void CrossfadeTransport::SetPlaybackState(int state) {
    bool changed = false;

    {
        Lock lock(this->stateMutex);
        changed = (this->state != state);
        this->state = (PlaybackState) state;
    }

    if (changed) {
        this->PlaybackEvent(state);
    }
}

void CrossfadeTransport::RaiseStreamEvent(int type, Player* player) {
    this->StreamEvent(type, player->GetUrl());
}

CrossfadeTransport::PlayerContext::PlayerContext(Crossfader& crossfader)
: crossfader(crossfader)
, player(nullptr)
, canFade(false) {
}

void CrossfadeTransport::PlayerContext::Reset() {
    this->Reset("", nullptr);
}

void CrossfadeTransport::PlayerContext::Reset(
    const std::string& url,
    Player::PlayerEventListener* listener)
{
    if (this->player && this->output) {
        if (this->started && this->canFade) {
            crossfader.Cancel(
                this->player,
                Crossfader::FadeIn);

            crossfader.Fade(
                this->player,
                this->output,
                Crossfader::FadeOut,
                CROSSFADE_DURATION_MS);
        }
        else {
            this->player->Destroy();
        }
    }

    this->canFade = this->started = false;
    this->output = url.size() ? outputs::SelectedOutput() : nullptr;
    this->player = url.size() ? Player::Create(url, this->output, listener) : nullptr;
}

void CrossfadeTransport::PlayerContext::TransferTo(PlayerContext& to) {
    to.player = player;
    to.output = output;

    /* don't call Reset() here! it'll trigger a fade. */
    this->canFade = false;
    this->output.reset();
    this->player = nullptr;
}

void CrossfadeTransport::PlayerContext::Start(double transportVolume) {
    if (this->output && this->player) {
        this->started = true;
        this->output->SetVolume(0.0f);
        this->output->Resume();
        this->player->Play();

        if (this->canFade) {
            crossfader.Fade(
                this->player,
                this->output,
                Crossfader::FadeIn,
                CROSSFADE_DURATION_MS);
        }
        else {
            this->output->SetVolume(transportVolume);
        }
    }
}

void CrossfadeTransport::PlayerContext::Stop() {
    if (this->output && this->player) {
        this->output->Stop();
        this->player->Destroy();
    }

    this->canFade = this->started = false;
    this->player = nullptr;
    this->output.reset();
}

void CrossfadeTransport::PlayerContext::Pause() {
    if (this->output) {
        this->output->Pause();
    }
}

void CrossfadeTransport::PlayerContext::Resume() {
    if (this->output) {
        this->output->Resume();
    }
}

void CrossfadeTransport::PlayerContext::SetVolume(double volume) {
    if (this->output) {
        this->output->SetVolume(volume);
    }
}