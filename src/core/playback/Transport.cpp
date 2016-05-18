//////////////////////////////////////////////////////////////////////////////
// Copyright © 2007, Daniel Önnerby
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

using namespace musik::core::audio;

static std::string TAG = "Transport";

Transport::Transport()
: volume(1.0) 
, state(StateStopped) {
}

Transport::~Transport() {
    this->nextPlayer.reset();
    this->currentPlayer.reset();
}

Transport::PlaybackState Transport::GetPlaybackState() {
    boost::mutex::scoped_lock lock(this->stateMutex);
    return this->state;
}

void Transport::PrepareNextTrack(std::string trackUrl) {
    PlayerPtr player = Player::Create(trackUrl);

    {
        boost::mutex::scoped_lock lock(this->stateMutex);
        this->currentPlayer = player;
    }
}

void Transport::Start(std::string url) {
    musik::debug::info(TAG, "we were asked to start the track at " + url);

    /* TODO FIXME: hack; the player is reference counted, we don't want the count
    to reach zero within the critical section, because its background thread may
    raise an event and cause a deadlock. do we really need shared_ptrs for these
    Player instances? */
    PlayerPtr current;

    PlayerPtr player;

    {
        boost::mutex::scoped_lock lock(this->stateMutex);

        PlayerPtr newPlayer = this->nextPlayer;
        this->nextPlayer.reset();

        musik::debug::info(TAG, "creating a Player...");

        if (!newPlayer || newPlayer->GetUrl() != url) {
            newPlayer = Player::Create(url); /* non-blocking */
            newPlayer->SetVolume(this->volume);
            musik::debug::info(TAG, "Player created successfully");
        }

        current = this->currentPlayer; /* see hack note above. */
        this->currentPlayer = newPlayer;

        this->currentPlayer->PlaybackStarted.connect(this, &Transport::OnPlaybackStarted);
        this->currentPlayer->PlaybackAlmostEnded.connect(this, &Transport::OnPlaybackAlmostEnded);
        this->currentPlayer->PlaybackEnded.connect(this, &Transport::OnPlaybackEnded);
        this->currentPlayer->PlaybackError.connect(this, &Transport::OnPlaybackError);

        musik::debug::info(TAG, "play()");
        this->currentPlayer->Play(); /* non-blocking */

        player = this->currentPlayer;
    }

    this->RaisePlaybackEvent(Transport::EventScheduled, player);
}

void Transport::Stop() {
    musik::debug::info(TAG, "stop");

    PlayerPtr player = NULL;

    {
        boost::mutex::scoped_lock lock(this->stateMutex);

        player = this->currentPlayer;
        this->currentPlayer.reset();
        this->nextPlayer.reset();
    }

    if (player) {
        this->RaisePlaybackEvent(Transport::EventFinished, player);
    }
}

bool Transport::Pause() {
    musik::debug::info(TAG, "pause");

    PlayerPtr player;

    {
        boost::mutex::scoped_lock lock(this->stateMutex);

        if (this->currentPlayer) {
            this->currentPlayer->Pause();
            player = this->currentPlayer;
        }
    }

    if (player) {
        this->RaisePlaybackEvent(Transport::EventPaused, player);
        return true;
    }

    return false;
}

bool Transport::Resume() {
    musik::debug::info(TAG, "resume");

    PlayerPtr player;
    
    {
        boost::mutex::scoped_lock lock(this->stateMutex);

        if (this->currentPlayer) {
            this->currentPlayer->Resume();
            player = this->currentPlayer;
        }
    }

    if (player) {
        this->RaisePlaybackEvent(Transport::EventResumed, player);
        return true;
    }

    return false;
}

double Transport::Position() {
    boost::mutex::scoped_lock lock(this->stateMutex);

    if (this->currentPlayer) {
        return this->currentPlayer->Position();
    }

    return 0;
}

void Transport::SetPosition(double seconds) {
    boost::mutex::scoped_lock lock(this->stateMutex);
    
    if (this->currentPlayer) {
        return this->currentPlayer->SetPosition(seconds);
    }
}

double Transport::Volume() {
    return this->volume;
}

void Transport::SetVolume(double volume) {
    double oldVolume = this->volume;
    
    volume = max(0, min(1.0, volume));

    this->volume = volume;

    if (oldVolume != this->volume) {
        this->VolumeChanged();
    }

    musik::debug::info(TAG, boost::str(
        boost::format("set volume %d%%") % round(volume * 100)));

    {
        boost::mutex::scoped_lock lock(this->stateMutex);

        if (this->currentPlayer) {
            this->currentPlayer->SetVolume(volume);
        }
    }
}

void Transport::OnPlaybackStarted(Player *player) {
    PlayerPtr playerForEvent;

    {
        boost::mutex::scoped_lock lock(this->stateMutex);

        if (this->currentPlayer.get() == player) {
            playerForEvent = this->currentPlayer;
        }
    }

    if (playerForEvent) {
        this->RaisePlaybackEvent(Transport::EventStarted, playerForEvent);
    }
}

void Transport::OnPlaybackAlmostEnded(Player *player) {
    PlayerPtr playerForEvent;

    {
        boost::mutex::scoped_lock lock(this->stateMutex);

        if (this->currentPlayer.get() == player) {
            playerForEvent = this->currentPlayer;
        }
    }

    this->RaisePlaybackEvent(Transport::EventAlmostDone, playerForEvent);
}

void Transport::OnPlaybackEnded(Player *player) {
    PlayerPtr playerForEvent;
    PlayerPtr nextPlayer;

    {
        boost::mutex::scoped_lock lock(this->stateMutex);

        if (this->currentPlayer.get() == player) {
            playerForEvent = this->currentPlayer;
        }

        if (this->nextPlayer) {
            nextPlayer = this->nextPlayer;
        }
    }

    if (playerForEvent) {
        this->RaisePlaybackEvent(Transport::EventFinished, playerForEvent);
    }

    if (nextPlayer) {
        this->Start(nextPlayer->GetUrl().c_str());
    }
}

void Transport::OnPlaybackError(Player *player) {
    PlayerPtr playerForEvent;

    {
        boost::mutex::scoped_lock lock(this->stateMutex);

        if (this->currentPlayer.get() == player) {
            playerForEvent = this->currentPlayer;
        }
    }

    if (playerForEvent) {
        this->RaisePlaybackEvent(Transport::EventError, playerForEvent);
    }
}

void Transport::RaisePlaybackEvent(int type, PlayerPtr player) {
    /* TODO FIXME: should be either decoupled or merged with the playback
    event enum. */
    switch (type) {
        case EventStarted:
        case EventResumed:
            this->state = StatePlaying;
            break;

        case EventFinished:
        case EventError:
            this->state = StateStopped;
            break;

        case EventPaused:
            this->state = StatePaused;
            break;
    }

    std::string uri = player ? player->GetUrl() : "";
    this->PlaybackEvent(type, uri);
}