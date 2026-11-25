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
#include <common/Contract.h>

#include <musikcore/debug.h>
#include <musikcore/audio/CrossfadeTransport.h>
#include <musikcore/audio/Outputs.h>
#include <algorithm>

#define CROSSFADE_DURATION_MS 1500
#define END_OF_TRACK_MIXPOINT 1001

using namespace musik::core::audio;
using namespace musik::core::sdk;

static std::string TAG = "CrossfadeTransport";

CrossfadeTransport::CrossfadeTransport()
: playbackState(PlaybackState::Stopped)
, crossfader(*this, [this] { this->OnCrossfaderEmptied(); })
, active(*this, crossfader)
, next(*this, crossfader)
, volume(1.0)
, muted(false) {
}

CrossfadeTransport::~CrossfadeTransport() {
    this->Destroy();
}

void CrossfadeTransport::Destroy() {
    if (this->destroyed.exchange(true)) {
        return;
    }
    this->playbackWorkers.Close();
    this->crossfader.Shutdown();
    this->StopImmediately();
}

PlaybackState CrossfadeTransport::GetPlaybackState() {
    Lock lock(this->stateMutex);
    return this->playbackState;
}

StreamState CrossfadeTransport::GetStreamState() {
    Lock lock(this->stateMutex);
    return this->activePlayerState;
}

void CrossfadeTransport::PrepareNextTrack(const std::string& uri, Gain gain) {
    Lock lock(this->stateMutex);
    musik::contract::Assert(!this->destroyed.load(), "preparing a destroyed transport");
    this->next.Reset(uri, gain);
}

void CrossfadeTransport::Start(const std::string& uri, Gain gain, StartMode mode) {
    {
        Lock lock(this->stateMutex);
        musik::contract::Assert(!this->destroyed.load(), "starting a destroyed transport");

        musik::debug::info(TAG, "trying to play " + uri);

        this->intent = mode == StartMode::Immediate ? Intent::Play : Intent::Prepare;

        /* in many cases (e.g. the user is skipping through tracks,
        the requested track may already be queued up. use it, if it is */
        if (this->next.player && this->next.player->GetUrl() == uri) {
            this->active.Reset();
            this->next.TransferTo(this->active);

            if (this->active.player) {
                this->RaiseStreamEvent(
                    this->active.player->GetStreamState(),
                    this->active.player.get());
            }

            if (this->intent == Intent::Play) {
                this->active.Start(this->volume);
            }
            else if (this->intent == Intent::Prepare && this->activePlayerState == StreamState::Buffered) {
                this->SetPlaybackState(PlaybackState::Prepared);
            }
        }
        else {
            this->active.Reset(uri, gain);
            this->next.Stop();
            if (this->active.player) {
                this->RaiseStreamEvent(StreamState::Buffering, this->active.player.get());
            }
        }
    }
}

std::string CrossfadeTransport::Uri() {
    Lock lock(this->stateMutex);
    auto const player = this->active.player;
    return player ? player->GetUrl() : "";
}

void CrossfadeTransport::ReloadOutput() {
    this->Stop();
}

void CrossfadeTransport::StopImmediately() {
    Lock lock(this->stateMutex);
    /* like stop, but will not fade out. */
    this->active.Stop();
    this->next.Stop();

    this->SetPlaybackState(PlaybackState::Stopped);
}

void CrossfadeTransport::Stop() {
    Lock lock(this->stateMutex);
    if (this->crossfader.StopPausedFades()) {
        this->StopImmediately();
        return;
    }
    this->active.Reset();
    this->next.Reset();

    this->SetPlaybackState(PlaybackState::Stopped);
}

mc_result CrossfadeTransport::Pause() {
    Lock lock(this->stateMutex);
    if (!this->active.player) {
        return MC_ERROR_NOT_FOUND;
    }
    const auto result = this->crossfader.SetPaused(true, *this->active.output);
    if (result == MC_OK) {
        this->intent = Intent::Pause;
        this->SetPlaybackState(PlaybackState::Paused);
    }
    return result;
}

mc_result CrossfadeTransport::Resume() {
    Lock lock(this->stateMutex);
    if (!this->active.player) {
        return MC_ERROR_NOT_FOUND;
    }
    const auto result = this->crossfader.SetPaused(false, *this->active.output);
    if (result == MC_OK) {
        this->intent = Intent::Play;
        this->active.Play(this->volume);
        this->SetPlaybackState(PlaybackState::Playing);
    }
    return result;
}

double CrossfadeTransport::Position() {
    Lock lock(this->stateMutex);

    if (this->active.player) {
        return this->active.player->GetPosition();
    }

    return 0;
}

void CrossfadeTransport::SetPosition(double seconds) {
    Lock lock(this->stateMutex);
    if (!this->active.player || !this->active.player->SetPosition(seconds)) {
        return;
    }

    if (this->intent == Intent::Pause) {
        /* the seek discards queued audio. retire the old track with it and
        restore the selected track's gain at the new position. */
        this->crossfader.Cancel(this->active.player.get(), Crossfader::FadeIn);
        this->crossfader.StopPausedFades();
        const auto volume = this->IsMuted() ? 0.0 : this->Volume();
        this->active.SetVolume(volume);
        this->intent = Intent::Play;
        this->active.Play(volume);
    }
    /* restoring a prepared queue entry also sets its saved position. it
    stays ready for an explicit Play, including when the open is still pending. */
    if (this->intent == Intent::Play) {
        this->SetPlaybackState(PlaybackState::Playing);
    }
    this->TimeChanged.Publish(seconds);
}

double CrossfadeTransport::GetDuration() {
    Lock lock(this->stateMutex);
    return this->active.player ? this->active.player->GetDuration() : -1.0f;
}

bool CrossfadeTransport::IsMuted() noexcept {
    return this->muted;
}

void CrossfadeTransport::SetMuted(bool muted) {
    Lock lock(this->stateMutex);
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
            if (!crossfader.Contains(this->active.player.get())) {
                this->active.SetVolume(this->volume);
            }

            if (!crossfader.Contains(this->next.player.get())) {
                this->next.SetVolume(this->volume);
            }
        }

        this->VolumeChanged.Publish();
    }
}

double CrossfadeTransport::Volume() noexcept {
    return this->volume;
}

void CrossfadeTransport::SetVolume(double volume) {
    const double oldVolume = this->volume;
    volume = std::max(0.0, std::min(1.0, volume));

    {
        Lock lock(this->stateMutex);
        this->volume = volume;
        active.SetVolume(volume);
        next.SetVolume(volume);
    }

    if (oldVolume != this->volume) {
        this->SetMuted(false);
        this->VolumeChanged.Publish();
    }
}

void CrossfadeTransport::OnPlayerBuffered(Player* player) {
    {
        Lock lock(this->stateMutex);
        if (this->destroyed.load()) {
            return;
        }

        const int durationMs = static_cast<int>(player->GetDuration() * 1000.0f);
        double mixpointOffset = 0.0f;

        const bool canFade =
            player->HasCapability(Capability::Prebuffer) &&
            (durationMs > CROSSFADE_DURATION_MS * 4);

        /* if the track isn't long enough don't set a mixpoint, and
        disable the crossfade functionality */
        if (canFade) {
            mixpointOffset =
                player->GetDuration() -
                (static_cast<float>(CROSSFADE_DURATION_MS / 1000.0f));
        }

        /* set up a mix point so we're notified when the track is
        about to end. we'll use this to fade it out */
        if (canFade) {
            player->AddMixPoint(END_OF_TRACK_MIXPOINT, mixpointOffset);
        }

        if (player == active.player.get()) {
            active.canFade = canFade;
            this->RaiseStreamEvent(StreamState::Buffered, player);
            if (this->intent == Intent::Play && !active.started) {
                active.Start(this->volume);
            }
            else if (this->intent == Intent::Prepare) {
                this->SetPlaybackState(PlaybackState::Prepared);
            }
        }
        else if (player == next.player.get()) {
            next.canFade = canFade;
        }
    }
}

void CrossfadeTransport::OnPlayerStarted(Player* player) {
    Lock lock(this->stateMutex);
    if (!this->destroyed.load() && player == this->active.player.get()) {
        this->RaiseStreamEvent(StreamState::Playing, player);
        /* a completion already in flight can arrive after Pause returned. */
        if (this->intent == Intent::Play) {
            this->SetPlaybackState(PlaybackState::Playing);
        }
    }
}

void CrossfadeTransport::OnPlayerFinished(Player* player) {
    Lock lock(this->stateMutex);
    if (player != this->active.player.get() && player != this->next.player.get()) {
        return;
    }
    this->RaiseStreamEvent(StreamState::Finished, player);

    /* in normal situations OnPlayerMixPoint will deal with the stream switch over,
    but it will not if (1) the stream is too short and there is no mixpoint, or (2)
    the decoder reports an incorrect track duration so the mixpoint is never hit. in
    these cases we need to switch things over manually. if OnPlayerMixPoint is called,
    the old player has already left both selections and its callback is ignored. */
    this->active.StopIf(player);
    this->next.StopIf(player);

    if (next.player && next.output) {
        next.TransferTo(active);
        if (this->intent == Intent::Play) {
            active.Start(this->volume);
        }
    }
    else {
        this->Stop();
    }
}

void CrossfadeTransport::OnPlayerOpenFailed(Player* player) {
    Lock lock(this->stateMutex);
    if (player == active.player.get()) {
        /* publish failure while this player is still selected, and stop it
        before a replacement Start can publish a different active player. */
        this->RaiseStreamEvent(StreamState::OpenFailed, player);
        this->crossfader.StopPausedFades();
        this->StopImmediately();
    }
    else if (player == next.player.get()) {
        /* failure prebuffering the next source must not stop audible audio. */
        next.Reset();
    }
}

void CrossfadeTransport::OnPlayerStopped(Player* player) {
    this->RaiseStreamEvent(StreamState::Destroyed, player);
}

void CrossfadeTransport::OnPlayerMixPoint(Player* player, int id, double time) {
    Lock lock(this->stateMutex);
    /* a queued seek can overtake this notification after the worker has
    detected the old mix point. let it seek before starting the next track. */
    if (id != END_OF_TRACK_MIXPOINT || player != active.player.get() || player->HasPendingSeek()) {
        return;
    }
    active.Reset();
    next.TransferTo(active);
    if (active.IsEmpty()) {
        this->SetPlaybackState(PlaybackState::Stopped);
    }
    else if (this->intent == Intent::Play) {
        active.Start(this->volume);
    }
}

void CrossfadeTransport::OnCrossfaderEmptied() {
    Lock lock(this->stateMutex);
    /* another fade may have arrived while the worker released its lock. */
    if (this->active.IsEmpty() && this->next.IsEmpty() && this->crossfader.Empty()) {
        this->Stop();
    }
}

void CrossfadeTransport::SetPlaybackState(PlaybackState state) {
    bool changed = false;

    {
        Lock lock(this->stateMutex);
        changed = (this->playbackState != state);
        this->playbackState = static_cast<PlaybackState>(state);
    }

    if (changed) {
        this->PlaybackEvent.Publish(state);
    }
}

void CrossfadeTransport::RaiseStreamEvent(StreamState type, Player const* player) {
    bool eventIsFromActivePlayer = false;
    {
        Lock lock(this->stateMutex);
        eventIsFromActivePlayer = (player == active.player.get());
        if (eventIsFromActivePlayer) {
            this->activePlayerState = static_cast<StreamState>(type);
        }
    }

    if (eventIsFromActivePlayer) {
        this->StreamEvent.Publish(type, player->GetUrl());
    }
}

CrossfadeTransport::PlayerContext::PlayerContext(
    CrossfadeTransport& transport,
    Crossfader& crossfader) noexcept
: started(false)
, canFade(false)
, player(nullptr)
, transport(transport)
, crossfader(crossfader) {
}

void CrossfadeTransport::PlayerContext::StopIf(Player const* player) {
    if (player == this->player.get()) {
        this->Stop();
    }
}

void CrossfadeTransport::PlayerContext::Reset() {
    this->Reset("", Gain());
}

void CrossfadeTransport::PlayerContext::Reset(
    const std::string& url,
    Gain gain)
{
    if (this->player && this->output) {
        this->transport.RaiseStreamEvent(StreamState::Destroyed, this->player.get());
        if (this->started && this->canFade) {
            crossfader.Cancel(
                this->player.get(),
                Crossfader::FadeIn);

            crossfader.Fade(
                this->player,
                this->output,
                Crossfader::FadeOut,
                CROSSFADE_DURATION_MS);
        }
        else {
            /* a non-fadeable stopped player must not retain paused audio that
            can no longer be reached from active/next contexts. */
            this->output->Stop();
            player->RequestStop(Player::Drain::No);
        }
    }

    this->canFade = this->started = false;
    this->output = url.size() ? outputs::SelectedOutput() : nullptr;
    this->player = url.size()
        ? this->transport.playbackWorkers.Start(url, this->output, Player::Drain::Yes, gain)
        : nullptr;
}

void CrossfadeTransport::PlayerContext::TransferTo(PlayerContext& to) noexcept {
    to.player = std::move(player);
    to.output = std::move(output);
    to.canFade = canFade;
    to.started = started;

    /* don't call Reset() here! it'll trigger a fade. */
    this->canFade = false;
}

void CrossfadeTransport::PlayerContext::Start(double transportVolume) {
    if (this->output && this->player) {
        /* an explicit replacement must also restart the outgoing fade and its clock. */
        if (this->crossfader.SetPaused(false, *this->output) != MC_OK) {
            this->transport.OnPlayerOpenFailed(this->player.get());
            return;
        }
        this->Play(transportVolume);
    }
}

void CrossfadeTransport::PlayerContext::Play(double transportVolume) {
    if (this->started) {
        this->player->Play();
        return;
    }
    this->started = true;
    this->output->SetVolume(this->canFade ? 0.0 : transportVolume);
    this->player->Play();
    if (this->canFade) {
        crossfader.Fade(this->player, this->output, Crossfader::FadeIn, CROSSFADE_DURATION_MS);
    }
}

void CrossfadeTransport::PlayerContext::Stop() {
    if (this->output && this->player) {
        this->output->Stop();
        this->transport.RaiseStreamEvent(StreamState::Destroyed, this->player.get());
        this->player->RequestStop();
    }

    this->canFade = this->started = false;
    this->player = nullptr;
    this->output.reset();
}

void CrossfadeTransport::PlayerContext::SetVolume(double volume) {
    if (this->output) {
        this->output->SetVolume(volume);
    }
}

bool CrossfadeTransport::PlayerContext::IsEmpty() noexcept {
    return !this->player && !this->output;
}
