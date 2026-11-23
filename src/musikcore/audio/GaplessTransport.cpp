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
#include <musikcore/audio/GaplessTransport.h>
#include <musikcore/plugin/PluginFactory.h>
#include <musikcore/audio/Outputs.h>
#include <algorithm>

using namespace musik::core::audio;
using namespace musik::core::sdk;

static std::string TAG = "GaplessTransport";

GaplessTransport::GaplessTransport()
: playbackState(PlaybackState::Stopped)
, activePlayer(nullptr)
, nextPlayer(nullptr)
, volume(1.0)
, nextCanStart(false)
, muted(false) {
    this->output = outputs::SelectedOutput();
}

GaplessTransport::~GaplessTransport() { this->Destroy(); }

void GaplessTransport::Destroy() {
    if (this->destroyed.exchange(true)) {
        return;
    }
    /* keep the transport alive until its own callbacks finish, without
    holding stateMutex or waiting on unrelated transports. */
    this->playbackWorkers.Close();
    this->StopInternal(true, true);
    LockT lock(this->stateMutex);
    this->output.reset();
}

PlaybackState GaplessTransport::GetPlaybackState() {
    LockT lock(this->stateMutex);
    return this->playbackState;
}

StreamState GaplessTransport::GetStreamState() {
    LockT lock(this->stateMutex);
    return this->activePlayerState;
}

void GaplessTransport::PrepareNextTrack(const std::string& uri, Gain gain) {
    LockT lock(this->stateMutex);

    this->ResetNextPlayer();

    if (uri.size()) {
        musik::contract::Assert(!this->destroyed.load(), "preparing a destroyed transport");
        this->nextPlayer = this->playbackWorkers.Start(
            uri,
            this->output, Player::Drain::No,
            gain);
        if (this->nextCanStart) {
            this->StartWithPlayer(this->nextPlayer);
        }
    }
}

void GaplessTransport::Start(const std::string& uri, Gain gain, StartMode mode) {
    /* publish activePlayer before a fast open failure can destroy the worker. */
    LockT lock(this->stateMutex);
    musik::contract::Assert(!this->destroyed.load(), "starting a destroyed transport");
    musik::debug::info(TAG, "starting track at " + uri);
    auto newPlayer = this->playbackWorkers.Start(
        uri,
        this->output,
        Player::Drain::No,
        gain);
    this->StartWithPlayer(newPlayer, mode);
}

void GaplessTransport::StartWithPlayer(Player::Shared newPlayer, StartMode mode) {
    if (newPlayer) {
        bool playingNext = false;

        {
            LockT lock(this->stateMutex);

            playingNext = (newPlayer == nextPlayer);
            if (newPlayer != nextPlayer) {
                this->ResetNextPlayer();
            }

            this->ResetActivePlayer();

            this->nextPlayer = nullptr;
            this->activePlayer = newPlayer;
            this->terminalState = StreamState::Stopped;
            this->activeStartMode = mode;

            if (newPlayer) {
                this->RaiseStreamEvent(newPlayer->GetStreamState(), newPlayer.get());
            }

            /* first argument suppresses the "Stop" event from getting triggered,
            the second param is used for gapless playback -- we won't stop the output
            and will allow pending buffers to finish if we're not automatically
            playing the next track. */
            this->StopInternal(true, !playingNext, newPlayer.get());
            this->SetNextCanStart(false);
            if (this->output && this->output->Resume() != MC_OK) {
                this->OnPlayerOpenFailed(newPlayer.get());
                return;
            }

            if (mode == StartMode::Immediate) {
                newPlayer->Play();
                if (this->playbackState == PlaybackState::Paused ||
                    this->playbackState == PlaybackState::Prepared)
                {
                    this->SetPlaybackState(PlaybackState::Playing);
                }
            }
            else if (this->activePlayerState == StreamState::Buffered) {
                this->SetPlaybackState(PlaybackState::Prepared);
            }
        }
    }
}

void GaplessTransport::ReloadOutput() {
    this->Stop();
    LockT lock(this->stateMutex);
    this->output = outputs::SelectedOutput();
    this->output->SetVolume(volume);
}

void GaplessTransport::Stop() {
    this->StopInternal(false, true);
    LockT lock(this->stateMutex);
    this->terminalState = StreamState::Stopped;
}

std::string GaplessTransport::Uri() {
    LockT lock(this->stateMutex);
    const auto player = this->activePlayer;
    return player ? player->GetUrl() : "";
}

void GaplessTransport::StopInternal(
    bool suppressStopEvent,
    bool stopOutput,
    Player const* exclude)
{
    musik::debug::info(TAG, "stop");

    /* if we stop the output, we kill all of the Players immediately.
    otherwise, we let them finish naturally; RemoveActive() will take
    care of disposing of them */
    if (stopOutput) {
        std::shared_ptr<Output> output;
        {
            LockT lock(this->stateMutex);
            output = this->output;

            this->ResetNextPlayer();
            if (this->activePlayer.get() != exclude) {
                this->ResetActivePlayer();
            }
        }

        /* stopping the transport will stop any buffers that are currently in
        flight. this makes the sound end immediately. */
        if (output) {
            output->Stop();
        }
    }

    if (!suppressStopEvent) {
        /* if we know we're starting another track immediately, suppress
        the stop event. this functionality is not available to the public
        interface, it's an internal optimization */
        this->SetPlaybackState(PlaybackState::Stopped);
    }
}

mc_result GaplessTransport::Pause() {
    LockT lock(this->stateMutex);
    if (!this->activePlayer) {
        return MC_ERROR_NOT_FOUND;
    }
    const auto result = this->output->Pause();
    if (result == MC_OK) {
        this->SetPlaybackState(PlaybackState::Paused);
    }
    return result;
}

mc_result GaplessTransport::Resume() {
    LockT lock(this->stateMutex);
    if (!this->activePlayer) {
        return MC_ERROR_NOT_FOUND;
    }
    const auto result = this->output->Resume();
    if (result == MC_OK) {
        this->activeStartMode = StartMode::Immediate;
        this->activePlayer->Play();
        this->SetPlaybackState(PlaybackState::Playing);
    }
    return result;
}

double GaplessTransport::Position() {
    LockT lock(this->stateMutex);

    if (this->activePlayer) {
        return this->activePlayer->GetPosition();
    }

    return 0;
}

void GaplessTransport::SetPosition(double seconds) {
    this->SetPositionInternal(seconds, false);
}

GaplessTransport::SeekResult GaplessTransport::Seek(double seconds) {
    return this->SetPositionInternal(seconds, true);
}

GaplessTransport::SeekResult GaplessTransport::SetPositionInternal(double seconds, bool requireReady) {
    {
        LockT lock(this->stateMutex);

        if (!this->activePlayer || (requireReady && this->terminalState != StreamState::Stopped)) {
            return SeekResult::NoTrack;
        }
        if (requireReady && this->activePlayerState == StreamState::Buffering) {
            return SeekResult::Buffering;
        }

        if (!this->activePlayer->SetPosition(seconds)) {
            return SeekResult::NoTrack;
        }
        this->nextCanStart = false;
        if (this->playbackState != PlaybackState::Playing) {
            this->SetPlaybackState(PlaybackState::Playing);
        }
    }

    this->TimeChanged.Publish(seconds);
    return SeekResult::Accepted;
}

GaplessTransport::Snapshot GaplessTransport::ReadSnapshot() {
    LockT lock(this->stateMutex);
    const auto stream = this->activePlayerState;
    const auto player = this->activePlayer;
    const bool ready = stream == StreamState::Buffered || stream == StreamState::Playing ||
        stream == StreamState::AlmostDone || stream == StreamState::Finished;

    /* Buffered publishes decoder metadata. Before then only read the position. */
    return { this->playbackState, stream, this->terminalState,
        player ? player->GetPosition() : 0.0, player && ready ? player->GetDuration() : -1.0, this->volume };
}

double GaplessTransport::GetDuration() {
    LockT lock(this->stateMutex);
    return this->activePlayer ? this->activePlayer->GetDuration() : -1.0f;
}

bool GaplessTransport::IsMuted() noexcept {
    return this->muted;
}

void GaplessTransport::SetMuted(bool muted) {
    LockT lock(this->stateMutex);
    if (this->muted != muted) {
        this->muted = muted;
        if (this->output) {
            this->output->SetVolume(muted ? 0.0f : this->volume.load());
        }
        this->VolumeChanged.Publish();
    }
}

double GaplessTransport::Volume() noexcept {
    return this->volume;
}

void GaplessTransport::SetVolume(double volume) {
    LockT lock(this->stateMutex);
    const double oldVolume = this->volume;

    volume = std::max(0.0, std::min(1.0, volume));

    this->volume = volume;
    if (this->output) {
        this->output->SetVolume(this->volume.load());
    }

    if (oldVolume != this->volume) {
        this->SetMuted(false);
        this->VolumeChanged.Publish();
    }
}

void GaplessTransport::SetNextCanStart(bool nextCanStart) {
    LockT lock(this->stateMutex);
    this->nextCanStart = nextCanStart;
}

void GaplessTransport::OnPlayerBuffered(Player* player) {
    LockT lock(this->stateMutex);
    if (player == this->activePlayer.get()) {
        this->RaiseStreamEvent(StreamState::Buffered, player);
        if (this->activeStartMode == StartMode::Wait) {
            this->SetPlaybackState(PlaybackState::Prepared);
        }
    }
}

void GaplessTransport::OnPlayerStarted(Player* player) {
    LockT lock(this->stateMutex);
    if (player == this->activePlayer.get()) {
        this->RaiseStreamEvent(StreamState::Playing, player);
        /* a completion already in flight can arrive after Pause returned. */
        if (this->playbackState != PlaybackState::Paused) {
            this->SetPlaybackState(PlaybackState::Playing);
        }
    }
}

void GaplessTransport::OnPlayerStreamEof(Player* player) {
    {
        LockT lock(this->stateMutex);

        /* EOF can arrive after a seek, stop, or a replacement player. */
        if (player != this->activePlayer.get() || player->HasPendingSeek()) {
            return;
        }
        this->nextCanStart = true;

        /* if another component configured a next player while we were playing,
        go ahead and get it started now. */
        if (this->nextPlayer) {
            this->StartWithPlayer(this->nextPlayer);
        }
    }

    this->RaiseStreamEvent(StreamState::AlmostDone, player);
}

void GaplessTransport::OnPlayerFinished(Player* player) {
    /* a replacement Start must not interleave with the old stream's stop event. */
    LockT lock(this->stateMutex);
    this->RaiseStreamEvent(StreamState::Finished, player);

    bool stopped = false;

    {
        LockT lock(this->stateMutex);

        bool startedNext = false;
        const bool playerIsActive = (player == this->activePlayer.get());

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

void GaplessTransport::OnPlayerOpenFailed(Player* player) {
    LockT lock(this->stateMutex);
    if (player == this->activePlayer.get()) {
        /* publish failure while this stream is still selected. serialize its
        stop event with replacement Start so it cannot stop the new URI. */
        this->RaiseStreamEvent(StreamState::OpenFailed, player);
        this->ResetActivePlayer();
        this->ResetNextPlayer();
        this->SetPlaybackState(PlaybackState::Stopped);
    }
    else if (player == this->nextPlayer.get()) {
        this->ResetNextPlayer();
    }
}

void GaplessTransport::OnPlayerStopped(Player *player) {
    LockT lock(this->stateMutex);

    if (player == this->activePlayer.get()) {
        this->ResetActivePlayer();
        return;
    }
}

void GaplessTransport::SetPlaybackState(PlaybackState state) {
    bool changed = false;

    {
        LockT lock(this->stateMutex);
        changed = (this->playbackState != state);
        this->playbackState = static_cast<PlaybackState>(state);
    }

    if (changed) {
        this->PlaybackEvent.Publish(state);
    }
}

void GaplessTransport::RaiseStreamEvent(StreamState type, Player const* player) {
    bool eventIsFromActivePlayer = false;
    {
        LockT lock(this->stateMutex);
        eventIsFromActivePlayer = (player == activePlayer.get());
        if (eventIsFromActivePlayer) {
            this->activePlayerState = static_cast<StreamState>(type);
            if (type == StreamState::Finished || type == StreamState::OpenFailed) {
                this->terminalState = type;
            }
        }
    }

    if (eventIsFromActivePlayer) {
        this->StreamEvent.Publish(type, player->GetUrl());
    }
}

void GaplessTransport::ResetNextPlayer() {
    if (this->nextPlayer) {
        this->RaiseStreamEvent(StreamState::Destroyed, this->nextPlayer.get());
        this->nextPlayer->RequestStop();
        this->nextPlayer = nullptr;
    }
}

void GaplessTransport::ResetActivePlayer() {
    if (this->activePlayer) {
        this->RaiseStreamEvent(StreamState::Destroyed, this->activePlayer.get());
        this->activePlayer->RequestStop();
        this->activePlayer = nullptr;
    }
}
