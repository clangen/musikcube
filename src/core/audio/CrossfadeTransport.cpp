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

#define END_OF_TRACK_MIXPOINT 1001

using namespace musik::core::audio;
using namespace musik::core::sdk;

static std::string TAG = "CrossfadeTransport";

CrossfadeTransport::CrossfadeTransport()
: volume(1.0)
, state(PlaybackStopped)
, nextCanStart(false)
, muted(false)
, crossfader(*this) {

}

CrossfadeTransport::~CrossfadeTransport() {
    this->disconnect_all();

    //LockT lock(this->stateMutex);

    //RESET_NEXT_PLAYER(this);
    //RESET_ACTIVE_PLAYER(this);
}

PlaybackState CrossfadeTransport::GetPlaybackState() {
    Lock lock(this->stateMutex);
    return this->state;
}

void CrossfadeTransport::PrepareNextTrack(const std::string& trackUrl) {
    //bool startNext = false;
    //{
    //    LockT lock(this->stateMutex);
    //    RESET_NEXT_PLAYER(this);
    //    this->nextPlayer = Player::Create(trackUrl, this->output, this);
    //    startNext = this->nextCanStart;
    //}

    //if (startNext) {
    //    this->StartWithPlayer(this->nextPlayer);
    //}
}

void CrossfadeTransport::Start(const std::string& url) {

    {
        Lock lock(this->stateMutex);

        musik::debug::info(TAG, "we were asked to start the track at " + url);
        this->nextPlayer.Reset(this->crossfader, Crossfader::SoftCut);
        this->activePlayer.Reset(this->crossfader, Crossfader::SoftCut, url, this);
        musik::debug::info(TAG, "Player created successfully");

        this->activePlayer.Play(this->crossfader);
    }

    this->RaiseStreamEvent(StreamScheduled, this->activePlayer.player);
}

void CrossfadeTransport::Restart() {
    //if (newPlayer) {
    //    bool playingNext = false;

    //    {
    //        LockT lock(this->stateMutex);

    //        playingNext = (newPlayer == nextPlayer);
    //        if (nextPlayer != nullptr && newPlayer != nextPlayer) {
    //            this->nextPlayer->Destroy();
    //        }

    //        RESET_ACTIVE_PLAYER(this);

    //        this->nextPlayer = nullptr;
    //        this->activePlayer = newPlayer;
    //    }

    //    /* first argument suppresses the "Stop" event from getting triggered,
    //    the second param is used for gapless playback -- we won't stop the output
    //    and will allow pending buffers to finish if we're not automatically
    //    playing the next track. note we do this outside of critical section so
    //    outputs *can* stop buffers immediately, and not to worry about causing a
    //    deadlock. */
    //    this->StopInternal(true, !playingNext, newPlayer);
    //    this->SetNextCanStart(false);
    //    this->output->Resume();
    //    newPlayer->Play();
    //    musik::debug::info(TAG, "play()");

    //    this->RaiseStreamEvent(StreamScheduled, newPlayer);
    //}
}

void CrossfadeTransport::ReloadOutput() {
    this->Stop();
}

void CrossfadeTransport::Stop() {
    //this->StopInternal(false, true);
}

void CrossfadeTransport::StopInternal(
    bool suppressStopEvent,
    bool stopOutput,
    Player* exclude)
{
    //musik::debug::info(TAG, "stop");

    ///* if we stop the output, we kill all of the Players immediately.
    //otherwise, we let them finish naturally; RemoveActive() will take
    //care of disposing of them */
    //if (stopOutput) {
    //    {
    //        LockT lock(this->stateMutex);

    //        RESET_NEXT_PLAYER(this);

    //        if (this->activePlayer && this->activePlayer != exclude) {
    //            this->activePlayer->Destroy();
    //            this->activePlayer = nullptr;
    //        }
    //    }

    //    /* stopping the transport will stop any buffers that are currently in
    //    flight. this makes the sound end immediately. */
    //    this->output->Stop();
    //}

    //if (!suppressStopEvent) {
    //    /* if we know we're starting another track immediately, suppress
    //    the stop event. this functionality is not available to the public
    //    interface, it's an internal optimization */
    //    this->SetPlaybackState(PlaybackStopped);
    //}
}

bool CrossfadeTransport::Pause() {
    //musik::debug::info(TAG, "pause");

    //this->output->Pause();

    //if (this->activePlayer) {
    //    this->SetPlaybackState(PlaybackPaused);
    //    return true;
    //}

    return false;
}

bool CrossfadeTransport::Resume() {
    //musik::debug::info(TAG, "resume");

    //this->output->Resume();

    //{
    //    LockT lock(this->stateMutex);

    //    if (this->activePlayer) {
    //        this->activePlayer->Play();
    //    }
    //}

    //if (this->activePlayer) {
    //    this->SetPlaybackState(PlaybackPlaying);
    //    return true;
    //}

    return false;
}

double CrossfadeTransport::Position() {
    Lock lock(this->stateMutex);

    if (this->activePlayer.player) {
        return this->activePlayer.player->GetPosition();
    }

    return 0;
}

void CrossfadeTransport::SetPosition(double seconds) {
    //{
    //    LockT lock(this->stateMutex);

    //    if (this->activePlayer) {
    //        this->activePlayer->SetPosition(seconds);
    //    }
    //}

    //if (this->activePlayer) {
    //    this->TimeChanged(seconds);
    //}
}

double CrossfadeTransport::GetDuration() {
    Lock lock(this->stateMutex);
    return this->activePlayer.player ? this->activePlayer.player->GetDuration() : -1.0f;
}

bool CrossfadeTransport::IsMuted() {
    return this->muted;
}

void CrossfadeTransport::SetMuted(bool muted) {
    //if (this->muted != muted) {
    //    this->muted = muted;
    //    this->output->SetVolume(muted ? 0.0f : this->volume);
    //    this->VolumeChanged();
    //}
}

double CrossfadeTransport::Volume() {
    return this->volume;
}

void CrossfadeTransport::SetVolume(double volume) {
    //double oldVolume = this->volume;

    //volume = std::max(0.0, std::min(1.0, volume));

    //this->volume = volume;

    //if (oldVolume != this->volume) {
    //    this->VolumeChanged();
    //}

    //std::string output = boost::str(
    //    boost::format("set volume %d%%") % round(volume * 100));

    //musik::debug::info(TAG, output);

    //this->output->SetVolume(this->volume);
}

void CrossfadeTransport::SetNextCanStart(bool nextCanStart) {
    //LockT lock(this->stateMutex);
    //this->nextCanStart = nextCanStart;
}

void CrossfadeTransport::OnPlayerStarted(Player* player) {
    this->RaiseStreamEvent(StreamPlaying, player);
    this->SetPlaybackState(PlaybackPlaying);

    /* set up a mix point so we're notified when the track is
    about to end. we'll use this to fade it out */
    {
        Lock lock(this->stateMutex);

        double offset = this->activePlayer.player->GetDuration() - 2.0f;
        if (offset > 0.0) {
            this->activePlayer.player->AddMixPoint(END_OF_TRACK_MIXPOINT, offset);
        }
    }
}

void CrossfadeTransport::OnPlayerAlmostEnded(Player* player) {
    //this->SetNextCanStart(true);

    //{
    //    LockT lock(this->stateMutex);

    //    /* if another component configured a next player while we were playing,
    //    go ahead and get it started now. */
    //    if (this->nextPlayer) {
    //        this->StartWithPlayer(this->nextPlayer);
    //    }
    //}

    //this->RaiseStreamEvent(StreamAlmostDone, player);
}

void CrossfadeTransport::OnPlayerFinished(Player* player) {
    //this->RaiseStreamEvent(StreamFinished, player);

    //bool stopped = false;

    //{
    //    LockT lock(this->stateMutex);

    //    bool startedNext = false;
    //    bool playerIsActive = (player == this->activePlayer);

    //    /* only start the next player if the currently active player is the
    //    one that just finished. */
    //    if (playerIsActive && this->nextPlayer) {
    //        this->StartWithPlayer(this->nextPlayer);
    //        startedNext = true;
    //    }

    //    if (!startedNext) {
    //        stopped = playerIsActive;
    //    }
    //}

    //if (stopped) {
    //    this->Stop();
    //}
}

void CrossfadeTransport::OnPlayerError(Player* player) {
    this->RaiseStreamEvent(StreamError, player);
    this->SetPlaybackState(PlaybackStopped);
}

void CrossfadeTransport::OnPlayerDestroying(Player *player) {
    //LockT lock(this->stateMutex);

    //if (player == this->activePlayer) {
    //    RESET_ACTIVE_PLAYER(this);
    //    return;
    //}
}

void CrossfadeTransport::OnPlayerMixPoint(Player* player, int id, double time) {
    if (id == END_OF_TRACK_MIXPOINT) {
        /* fade out */
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

CrossfadeTransport::PlayerContext::PlayerContext() {
    this->player = nullptr;
}

void CrossfadeTransport::PlayerContext::Reset(
    Crossfader& crossfader, Crossfader::Cut cut)
{
    //if (this->player) {
    //    crossfader.Remove(this->player, cut);
    //}

    this->player = nullptr;
    this->output.reset();
}

void CrossfadeTransport::PlayerContext::Reset(
    Crossfader& crossfader,
    Crossfader::Cut cut,
    const std::string& url,
    Player::PlayerEventListener* listener)
{
    //if (this->player) {
    //    if (this->output) {
    //        crossfader.Remove(this->player, cut);
    //    }
    //}

    this->output = outputs::SelectedOutput();
    this->player = Player::Create(url, this->output, listener);
}

void CrossfadeTransport::PlayerContext::Play(Crossfader& crossfader) {
    if (this->output && this->player) {
        this->output->SetVolume(0.0f);
        this->output->Resume();
        this->player->Play();

        //crossfader.Fade(
        //    this->player,
        //    this->output,
        //    Crossfader::FadeIn,
        //    1.0f,
        //    1000);
    }
}