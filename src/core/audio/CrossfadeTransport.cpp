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
#include <core/audio/CrossfadeTransport.h>
#include <core/plugin/PluginFactory.h>
#include <core/audio/Outputs.h>
#include <algorithm>

#define CROSSFADE_DURATION_MS 1500
#define END_OF_TRACK_MIXPOINT 1001

using namespace musik::core::audio;
using namespace musik::core::sdk;

static std::string TAG = "CrossfadeTransport";

CrossfadeTransport::CrossfadeTransport()
: volume(1.0)
, state(PlaybackStopped)
, muted(false)
, crossfader(*this)
, active(*this, crossfader)
, next(*this, crossfader) {
    this->crossfader.Emptied.connect(
        this, &CrossfadeTransport::OnCrossfaderEmptied);
}

CrossfadeTransport::~CrossfadeTransport() {
    this->Stop();
    this->crossfader.Drain();
}

PlaybackState CrossfadeTransport::GetPlaybackState() {
    Lock lock(this->stateMutex);
    return this->state;
}

void CrossfadeTransport::PrepareNextTrack(const std::string& uri, Gain gain) {
    Lock lock(this->stateMutex);
    this->next.Reset(uri, this, gain, false);
}

void CrossfadeTransport::Start(const std::string& uri, Gain gain, StartMode mode) {
    {
        Lock lock(this->stateMutex);

        musik::debug::info(TAG, "trying to play " + uri);

        bool immediate = mode == StartMode::Immediate;

        /* in many cases (e.g. the user is skipping through tracks,
        the requested track may already be queued up. use it, if it is */
        if (this->next.player && this->next.player->GetUrl() == uri) {
            this->active.Reset();
            this->next.TransferTo(this->active);

            if (immediate) {
                this->active.Start(this->volume);
            }
        }
        else {
            this->active.Reset(uri, this, gain, immediate);
            this->next.Stop();
        }
    }

    this->RaiseStreamEvent(StreamScheduled, this->active.player);
}

std::string CrossfadeTransport::Uri() {
    auto player = this->active.player;
    return player ? player->GetUrl() : "";
}

void CrossfadeTransport::ReloadOutput() {
    this->Stop();
}

void CrossfadeTransport::StopImmediately() {
    /* like stop, but will not fade out. */
    {
        Lock lock(this->stateMutex);
        this->active.Stop();
        this->next.Stop();
    }

    this->SetPlaybackState(PlaybackStopped);
}

void CrossfadeTransport::Stop() {
    {
        Lock lock(this->stateMutex);
        this->active.Reset();
        this->next.Reset();
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
        this->active.Resume(this->volume);
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
            if (this->state != PlaybackPlaying) {
                this->SetPlaybackState(PlaybackPlaying);
            }
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

    {
        Lock lock(this->stateMutex);
        this->volume = volume;
        active.SetVolume(volume);
        next.SetVolume(volume);
    }

    if (oldVolume != this->volume) {
        this->SetMuted(false);
        this->VolumeChanged();
    }
}

void CrossfadeTransport::OnPlayerPrepared(Player* player) {
    {
        Lock lock(this->stateMutex);

        int durationMs = (int)(player->GetDuration() * 1000.0f);
        double mixpointOffset = 0.0f;

        bool canFade =
            player->HasCapability(Capability::Prebuffer) &&
            (durationMs > CROSSFADE_DURATION_MS * 4);

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
            if (active.startImmediate) {
                active.Start(this->volume);
            }
        }
        else if (player == next.player) {
            next.canFade = canFade;
        }
    }

    if (player == this->active.player) {
        this->RaiseStreamEvent(StreamPrepared, player);
        this->SetPlaybackState(PlaybackPrepared);
    }
}

void CrossfadeTransport::OnPlayerStarted(Player* player) {
    this->RaiseStreamEvent(StreamPlaying, player);
    this->SetPlaybackState(PlaybackPlaying);
}

void CrossfadeTransport::OnPlayerFinished(Player* player) {
    this->RaiseStreamEvent(StreamFinished, player);

    Lock lock(this->stateMutex);

    /* in normal situations OnPlayerMixPoint will deal with the stream switch over,
    but it will not if (1) the stream is too short and there is no mixpoint, or (2)
    the decoder reports an incorrect track duration so the mixpoint is never hit. in
    these cases we need to switch things over manually. if OnPlayerMixPoint is called,
    the Player will be Detach()'d so this callback will never fire. if we get called
    we need to take action! */
    this->active.StopIf(player);
    this->next.StopIf(player);

    if (next.player && next.output) {
        next.TransferTo(active);
        active.Start(this->volume);
    }
    else {
        this->Stop();
    }
}

void CrossfadeTransport::OnPlayerError(Player* player) {
    this->RaiseStreamEvent(StreamError, player);
    this->Stop();
}

void CrossfadeTransport::OnPlayerMixPoint(Player* player, int id, double time) {
    bool stopped = false;

    {
        Lock lock(this->stateMutex);

        if (id == END_OF_TRACK_MIXPOINT) {
            if (player == active.player) {
                active.Reset(); /* fades out automatically */
                next.TransferTo(active);

                if (!active.IsEmpty()) {
                    active.Start(this->volume);
                }
                else {
                    stopped = true;
                }
            }
        }
    }

    if (stopped) {
        this->SetPlaybackState(PlaybackStopped);
    }
}

void CrossfadeTransport::OnCrossfaderEmptied() {
    bool stopped = false;

    {
        Lock lock(this->stateMutex);
        if (this->active.IsEmpty() && this->next.IsEmpty()) {
            stopped = true;
        }
    }

    if (stopped) {
        this->Stop();
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

CrossfadeTransport::PlayerContext::PlayerContext(
    CrossfadeTransport& transport,
    Crossfader& crossfader)
: transport(transport)
, crossfader(crossfader)
, player(nullptr)
, canFade(false)
, startImmediate(false) {
}

void CrossfadeTransport::PlayerContext::StopIf(Player* player) {
    if (player == this->player) {
        this->Stop();
    }
}

void CrossfadeTransport::PlayerContext::Reset() {
    this->Reset("", nullptr, Gain(), false);
}

void CrossfadeTransport::PlayerContext::Reset(
    const std::string& url,
    Player::EventListener* listener,
    Gain gain,
    bool startImmediate)
{
    this->startImmediate = false;

    if (this->player && this->output) {
        this->player->Detach(&this->transport);
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
            /* if we're being started with a new URL and we can't fade,
            drain the current instance! */
            player->Destroy(url.size() ? Player::NoDrain : Player::Drain);
        }
    }

    this->startImmediate = startImmediate;
    this->canFade = this->started = false;
    this->output = url.size() ? outputs::SelectedOutput() : nullptr;
    this->player = url.size() ? Player::Create(url, this->output, Player::Drain, listener, gain) : nullptr;
}

void CrossfadeTransport::PlayerContext::TransferTo(PlayerContext& to) {
    to.player = player;
    to.output = output;
    to.canFade = canFade;
    to.started = started;

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
        this->player->Detach(&this->transport);
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

void CrossfadeTransport::PlayerContext::Resume(double transportVolume) {
    if (!this->started) {
        this->Start(transportVolume);
    }
    else {
        if (this->output) {
            this->output->Resume();

            if (this->player) {
                this->player->Play();
            }
        }
    }
}

void CrossfadeTransport::PlayerContext::SetVolume(double volume) {
    if (this->output) {
        this->output->SetVolume(volume);
    }
}

bool CrossfadeTransport::PlayerContext::IsEmpty() {
    return !this->player && !this->output;
}
