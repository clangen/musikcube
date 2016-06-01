//////////////////////////////////////////////////////////////////////////////
// Copyright � 2007, Daniel �nnerby
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
#include <core/playback/Transport.h>
#include <core/plugin/PluginFactory.h>
#include <algorithm>
#include <boost/thread.hpp>

using namespace musik::core::audio;

static std::string TAG = "Transport";

#define RESET_NEXT_PLAYER() \
    delete this->nextPlayer; \
    this->nextPlayer = NULL;

#define DEFER(x, y) \
  { \
      boost::thread thread(boost::bind(x, this, y)); \
      thread.detach(); \
  }

static void pausePlayer(Player* p) {
    p->Pause();
}

static void resumePlayer(Player* p) {
    p->Resume();
}

static void stopPlayer(Player* p) {
    p->Stop();
}

static void deletePlayer(Player* p) {
    delete p;
}

Transport::Transport()
: volume(1.0)
, state(PlaybackStopped)
, nextPlayer(NULL) {
    this->output = Player::CreateDefaultOutput();
}

Transport::~Transport() {
}

Transport::PlaybackState Transport::GetPlaybackState() {
    boost::recursive_mutex::scoped_lock lock(this->stateMutex);
    return this->state;
}

void Transport::PrepareNextTrack(const std::string& trackUrl) {
    bool startNext = false;
    {
        boost::recursive_mutex::scoped_lock lock(this->stateMutex);
        this->nextPlayer = new Player(trackUrl, this->volume, this->output);
        startNext = this->state == PlaybackStopped;
    }

    /* we raise an event when the current stream has almost finished, which
    allows the calling app to prepare a new track. by the time this preparation
    is complete, the track may have ended. if we're in the stopped state, start
    playback of this new track immediately. */
    if (startNext) {
        this->StartWithPlayer(this->nextPlayer);
    }
}

void Transport::Start(const std::string& url) {
    musik::debug::info(TAG, "we were asked to start the track at " + url);

    Player* newPlayer = new Player(url, this->volume, this->output);
    musik::debug::info(TAG, "Player created successfully");

    this->StartWithPlayer(newPlayer);
}

void Transport::StartWithPlayer(Player* newPlayer) {
    if (newPlayer) {
        {
            boost::recursive_mutex::scoped_lock lock(this->stateMutex);

            if (newPlayer != nextPlayer) {
                delete nextPlayer;
            }

            this->nextPlayer = NULL;
            this->Stop(true); /* true = suppress stopped event */

            newPlayer->PlaybackStarted.connect(this, &Transport::OnPlaybackStarted);
            newPlayer->PlaybackAlmostEnded.connect(this, &Transport::OnPlaybackAlmostEnded);
            newPlayer->PlaybackFinished.connect(this, &Transport::OnPlaybackFinished);
            newPlayer->PlaybackStopped.connect(this, &Transport::OnPlaybackStopped);
            newPlayer->PlaybackError.connect(this, &Transport::OnPlaybackError);

            musik::debug::info(TAG, "play()");

            this->active.push_front(newPlayer);
            newPlayer->SetVolume(this->volume);
            newPlayer->Play();
        }

        this->RaiseStreamEvent(Transport::StreamScheduled, newPlayer);
    }
}

void Transport::Stop() {
    this->Stop(false);
}

void Transport::Stop(bool suppressEvent) {
    musik::debug::info(TAG, "stop");

    std::list<Player*> toDelete;

    {
        boost::recursive_mutex::scoped_lock lock(this->stateMutex);
        RESET_NEXT_PLAYER();
        std::swap(toDelete, this->active);
    }

    /* delete these in the background to avoid deadlock in some cases
    where this method is implicitly triggered via Player callback. however,
    it's perfectly safe (and actually required) to stop them immediately to
    ensure all pending buffers have been flushed. */
    std::for_each(toDelete.begin(), toDelete.end(), stopPlayer);
    DEFER(&Transport::DeletePlayers, toDelete);

    /* if we know we're starting another track immediately, suppress
    the stop event. this functionality is not available to the public
    interface, it's an internal optimization */
    if (!suppressEvent) {
        this->SetPlaybackState(PlaybackStopped);
    }
}

bool Transport::Pause() {
    musik::debug::info(TAG, "pause");

    size_t count = 0;

    {
        boost::recursive_mutex::scoped_lock lock(this->stateMutex);
        std::for_each(this->active.begin(), this->active.end(), pausePlayer);
        count = this->active.size();
    }

    if (count) {
        this->SetPlaybackState(PlaybackPaused);
        return true;
    }

    return false;
}

bool Transport::Resume() {
    musik::debug::info(TAG, "resume");

    size_t count = 0;

    {
        boost::recursive_mutex::scoped_lock lock(this->stateMutex);
        std::for_each(this->active.begin(), this->active.end(), resumePlayer);
        count = this->active.size();
    }

    if (count) {
        this->SetPlaybackState(Transport::PlaybackPlaying);
        return true;
    }

    return false;
}

double Transport::Position() {
    boost::recursive_mutex::scoped_lock lock(this->stateMutex);

    if (!this->active.empty()) {
        return this->active.front()->Position();
    }

    return 0;
}

void Transport::SetPosition(double seconds) {
    boost::recursive_mutex::scoped_lock lock(this->stateMutex);

    if (!this->active.empty()) {
        this->active.front()->SetPosition(seconds);
        this->TimeChanged(seconds);
    }
}

double Transport::Volume() {
    return this->volume;
}

void Transport::SetVolume(double volume) {
    double oldVolume = this->volume;

    volume = std::max(0.0, std::min(1.0, volume));

    this->volume = volume;

    if (oldVolume != this->volume) {
        this->VolumeChanged();
    }

    musik::debug::info(TAG, boost::str(
        boost::format("set volume %d%%") % round(volume * 100)));

    {
        boost::recursive_mutex::scoped_lock lock(this->stateMutex);

        if (!this->active.empty()) {
            this->active.front()->SetVolume(volume);
        }
    }
}

void Transport::OnPlaybackStarted(Player* player) {
    this->RaiseStreamEvent(Transport::StreamPlaying, player);
    this->SetPlaybackState(Transport::PlaybackPlaying);
}

void Transport::OnPlaybackAlmostEnded(Player* player) {
    this->RaiseStreamEvent(Transport::StreamAlmostDone, player);
}

void Transport::RemoveActive(Player* player) {
    bool found = false;

    {
        boost::recursive_mutex::scoped_lock lock(this->stateMutex);

        std::list<Player*>::iterator it =
            std::find(this->active.begin(), this->active.end(), player);

        if (it != this->active.end()) {
            this->active.erase(it);
            found = true;
        }
    }

    /* outside of the critical section, otherwise potential deadlock */
    if (found) {
        delete player;
    }
}

void Transport::DeletePlayers(std::list<Player*> players) {
    std::for_each(players.begin(), players.end(), deletePlayer);
}

void Transport::OnPlaybackFinished(Player* player) {
    this->RaiseStreamEvent(Transport::StreamFinished, player);

    bool stopped = false;

    {
        boost::recursive_mutex::scoped_lock lock(this->stateMutex);

        bool startedNext = false;

        if (this->nextPlayer) {
            this->StartWithPlayer(this->nextPlayer);
            startedNext = true;
        }

        /* we're considered stopped if we were unable to automatically start
        the next track, and the number of players is zero... or the number
        of players is one, and it's the current player. remember, we free
        players asynchronously. */
        if (!startedNext) {
            stopped =
                !this->active.size() ||
                (this->active.size() == 1 && this->active.front() == player);
        }
    }

    if (stopped) {
        this->SetPlaybackState(Transport::PlaybackStopped);
    }

    DEFER(&Transport::RemoveActive, player);
}

void Transport::OnPlaybackStopped (Player* player) {
    this->RaiseStreamEvent(Transport::StreamStopped, player);

    bool stopped = false;
    {
        boost::recursive_mutex::scoped_lock lock(this->stateMutex);
        stopped = !this->active.size();
    }

    if (stopped) {
        this->SetPlaybackState(Transport::PlaybackStopped);
    }

    DEFER(&Transport::RemoveActive, player);
}

void Transport::OnPlaybackError(Player* player) {
    this->RaiseStreamEvent(Transport::StreamError, player);
    this->SetPlaybackState(Transport::PlaybackStopped);
    DEFER(&Transport::RemoveActive, player);
}

void Transport::SetPlaybackState(int state) {
    bool changed = false;

    {
        boost::recursive_mutex::scoped_lock lock(this->stateMutex);
        changed = (this->state != state);
        this->state = (PlaybackState) state;
    }

    if (changed) {
        this->PlaybackEvent(state);
    }
}

void Transport::RaiseStreamEvent(int type, Player* player) {
    this->StreamEvent(type, player->GetUrl());
}
