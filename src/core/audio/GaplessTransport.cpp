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
#include <core/audio/GaplessTransport.h>
#include <core/plugin/PluginFactory.h>
#include <algorithm>
#include <boost/thread.hpp>

using namespace musik::core::audio;
using namespace musik::core::sdk;

static std::string TAG = "Transport";

#define RESET_NEXT_PLAYER() \
    delete this->nextPlayer; \
    this->nextPlayer = nullptr;

#define DEFER(x, y) \
  { \
      boost::thread thread(boost::bind(x, this, y)); \
      thread.detach(); \
  }

static void stopPlayer(Player* p) {
    p->Stop();
}

static void deletePlayer(Player* p) {
    delete p;
}

GaplessTransport::GaplessTransport()
: volume(1.0)
, state(PlaybackStopped)
, nextPlayer(nullptr)
, nextCanStart(false)
, muted(false) {
    this->output = Player::CreateDefaultOutput();
}

GaplessTransport::~GaplessTransport() {
}

PlaybackState GaplessTransport::GetPlaybackState() {
    boost::recursive_mutex::scoped_lock lock(this->stateMutex);
    return this->state;
}

void GaplessTransport::PrepareNextTrack(const std::string& trackUrl) {
    bool startNext = false;
    {
        boost::recursive_mutex::scoped_lock lock(this->stateMutex);
        RESET_NEXT_PLAYER();
        this->nextPlayer = new Player(trackUrl, this->output);
        startNext = this->nextCanStart;
    }

    if (startNext) {
        this->StartWithPlayer(this->nextPlayer);
    }
}

void GaplessTransport::Start(const std::string& url) {
    musik::debug::info(TAG, "we were asked to start the track at " + url);

    Player* newPlayer = new Player(url, this->output);
    musik::debug::info(TAG, "Player created successfully");

    this->StartWithPlayer(newPlayer);
}

void GaplessTransport::StartWithPlayer(Player* newPlayer) {
    if (newPlayer) {
        newPlayer->PlaybackStarted.connect(this, &GaplessTransport::OnPlaybackStarted);
        newPlayer->PlaybackAlmostEnded.connect(this, &GaplessTransport::OnPlaybackAlmostEnded);
        newPlayer->PlaybackFinished.connect(this, &GaplessTransport::OnPlaybackFinished);
        newPlayer->PlaybackError.connect(this, &GaplessTransport::OnPlaybackError);

        bool playingNext = false;

        {
            boost::recursive_mutex::scoped_lock lock(this->stateMutex);

            playingNext = (newPlayer == nextPlayer);
            if (newPlayer != nextPlayer) {
                delete nextPlayer;
            }

            this->nextPlayer = nullptr;
            this->active.push_back(newPlayer);
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
        newPlayer->Play();
        musik::debug::info(TAG, "play()");

        this->RaiseStreamEvent(StreamScheduled, newPlayer);
    }
}

void GaplessTransport::Stop() {
    this->StopInternal(false, true);
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
        std::list<Player*> toDelete;

        {
            boost::recursive_mutex::scoped_lock lock(this->stateMutex);
            RESET_NEXT_PLAYER();

            /* move all but the excluded player to the toDelete set. */
            auto it = this->active.begin();
            while (it != this->active.end()) {
                if (*it != exclude) {
                    toDelete.push_back(*it);
                    it = this->active.erase(it);
                }
                else {
                    ++it;
                }
            }
        }

        /* delete these in the background to avoid deadlock in some cases
        where this method is implicitly triggered via Player callback. however,
        we should stop them immediately so they stop producing audio. */
        std::for_each(toDelete.begin(), toDelete.end(), stopPlayer);
        DEFER(&GaplessTransport::DeletePlayers, toDelete);

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

    size_t count = 0;

    this->output->Pause();

    {
        boost::recursive_mutex::scoped_lock lock(this->stateMutex);
        count = this->active.size();
    }

    if (count) {
        this->SetPlaybackState(PlaybackPaused);
        return true;
    }

    return false;
}

bool GaplessTransport::Resume() {
    musik::debug::info(TAG, "resume");

    this->output->Resume();

    size_t count = 0;

    {
        boost::recursive_mutex::scoped_lock lock(this->stateMutex);
        count = this->active.size();

        auto it = this->active.begin();
        while (it != this->active.end()) {
            (*it)->Play();
            ++it;
        }
    }

    if (count) {
        this->SetPlaybackState(PlaybackPlaying);
        return true;
    }

    return false;
}

double GaplessTransport::Position() {
    boost::recursive_mutex::scoped_lock lock(this->stateMutex);

    if (!this->active.empty()) {
        return this->active.front()->Position();
    }

    return 0;
}

void GaplessTransport::SetPosition(double seconds) {
    boost::recursive_mutex::scoped_lock lock(this->stateMutex);

    if (!this->active.empty()) {
        this->active.front()->SetPosition(seconds);
        this->TimeChanged(seconds);
    }
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

    if (oldVolume != this->volume) {
        this->VolumeChanged();
    }

    std::string output = boost::str(
        boost::format("set volume %d%%") % round(volume * 100));

    musik::debug::info(TAG, output);

    this->output->SetVolume(this->volume);
}

void GaplessTransport::RemoveActive(Player* player) {
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

void GaplessTransport::DeletePlayers(std::list<Player*> players) {
    std::for_each(players.begin(), players.end(), deletePlayer);
}

void GaplessTransport::SetNextCanStart(bool nextCanStart) {
    boost::recursive_mutex::scoped_lock lock(this->stateMutex);
    this->nextCanStart = nextCanStart;
}

void GaplessTransport::OnPlaybackStarted(Player* player) {
    this->RaiseStreamEvent(StreamPlaying, player);
    this->SetPlaybackState(PlaybackPlaying);
}

void GaplessTransport::OnPlaybackAlmostEnded(Player* player) {
    this->SetNextCanStart(true);

    {
        boost::recursive_mutex::scoped_lock lock(this->stateMutex);

        /* if another component configured a next player while we were playing,
        go ahead and get it started now. */
        if (this->nextPlayer) {
            this->StartWithPlayer(this->nextPlayer);
        }
    }

    this->RaiseStreamEvent(StreamAlmostDone, player);
}

void GaplessTransport::OnPlaybackFinished(Player* player) {
    this->RaiseStreamEvent(StreamFinished, player);

    bool stopped = false;

    {
        boost::recursive_mutex::scoped_lock lock(this->stateMutex);

        bool startedNext = false;

        size_t count = this->active.size();
        Player* front = count ? this->active.front() : nullptr;
        bool playerIsFront = (player == front);

        /* only start the next player if the currently active player is the
        one that just finished. */
        if (this->nextPlayer && playerIsFront) {
            this->StartWithPlayer(this->nextPlayer);
            startedNext = true;
        }

        /* we're considered stopped if we were unable to automatically start
        the next track, and the number of players is zero... or the number
        of players is one, and it's the current player. remember, we free
        players asynchronously. */
        if (!startedNext) {
            stopped = !count || (count == 1 && playerIsFront);
        }
    }

    if (stopped) {
        this->Stop();
    }

    DEFER(&GaplessTransport::RemoveActive, player);
}

void GaplessTransport::OnPlaybackError(Player* player) {
    this->RaiseStreamEvent(StreamError, player);
    this->SetPlaybackState(PlaybackStopped);
    DEFER(&GaplessTransport::RemoveActive, player);
}

void GaplessTransport::SetPlaybackState(int state) {
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

void GaplessTransport::RaiseStreamEvent(int type, Player* player) {
    this->StreamEvent(type, player->GetUrl());
}
