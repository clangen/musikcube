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

#include <stdafx.h>
#include "PlaybackService.h"

#include <cursespp/MessageQueue.h>
#include <cursespp/Message.h>

#include <core/audio/ITransport.h>
#include <core/library/LocalLibraryConstants.h>

using musik::core::TrackPtr;
using musik::core::LibraryPtr;
using musik::core::audio::ITransport;

using cursespp::IMessageTarget;
using cursespp::IMessage;

using namespace musik::core::library::constants;
using namespace musik::box;

#define NO_POSITION (size_t) -1
#define URI_AT_INDEX(x) this->playlist.Get(x)->URI()
#define PREVIOUS_GRACE_PERIOD 2.0f
#define MESSAGE_STREAM_EVENT 1000
#define MESSAGE_PLAYBACK_EVENT 1001

PlaybackService::PlaybackService(LibraryPtr library, ITransport& transport)
: library(library)
, transport(transport)
, playlist(library) {
    transport.StreamEvent.connect(this, &PlaybackService::OnStreamEvent);
    transport.PlaybackEvent.connect(this, &PlaybackService::OnPlaybackEvent);
    this->index = NO_POSITION;
    this->nextIndex = NO_POSITION;
}

void PlaybackService::ProcessMessage(IMessage &message) {
    if (message.Type() == MESSAGE_STREAM_EVENT) {
        int64 eventType = message.UserData1();

        if (eventType == ITransport::StreamAlmostDone) {
            if (this->playlist.Count() > this->index + 1) {
                if (this->nextIndex != this->index + 1) {
                    this->nextIndex = this->index + 1;
                    this->transport.PrepareNextTrack(URI_AT_INDEX(nextIndex));
                }
            }
        }
        else if (eventType == ITransport::StreamPlaying) {
            if (this->nextIndex != NO_POSITION) {
                this->index = this->nextIndex;
                this->nextIndex = NO_POSITION;
            }

            if (this->index != NO_POSITION) {
                this->TrackChanged(this->index, this->playlist.Get(this->index));
            }
        }
    }
    else if (message.Type() == MESSAGE_PLAYBACK_EVENT) {
        int64 eventType = message.UserData1();

        if (eventType == ITransport::PlaybackStopped) {
            this->TrackChanged(NO_POSITION, TrackPtr());
        }
    }
}

bool PlaybackService::Next() {
    if (transport.GetPlaybackState() == ITransport::PlaybackStopped) {
        return false;
    }

    if (this->playlist.Count() > index + 1) {
        this->Play(index + 1);
        return true;
    }

    return false;
}

bool PlaybackService::Previous() {
    if (transport.GetPlaybackState() == ITransport::PlaybackStopped) {
        return false;
    }

    if (transport.Position() > PREVIOUS_GRACE_PERIOD) {
        this->Play(index);
        return true;
    }

    if (index > 0) {
        this->Play(index - 1);
        return true;
    }

    return false;
}

void PlaybackService::Play(TrackList& tracks, size_t index) {
    /* do the copy outside of the critical section, then swap. */
    TrackList temp(this->library);
    temp.CopyFrom(tracks);

    {
        boost::recursive_mutex::scoped_lock lock(this->stateMutex);
        this->playlist.Swap(temp);
    }

    if (index <= tracks.Count()) {
        this->Play(index);
    }
}

void PlaybackService::CopyTo(TrackList& target) {
    boost::recursive_mutex::scoped_lock lock(this->stateMutex);
    target.CopyFrom(this->playlist);
}

void PlaybackService::Play(size_t index) {
    transport.Start(URI_AT_INDEX(index));
    this->nextIndex = NO_POSITION;
    this->index = index;
}

size_t PlaybackService::GetIndex() {
    return this->index;
}

TrackPtr PlaybackService::GetTrackAtIndex(size_t index) {
    return this->playlist.Get(index);
}

void PlaybackService::OnStreamEvent(int eventType, std::string uri) {
    cursespp::MessageQueue::Instance().Post(
        cursespp::Message::Create(this, MESSAGE_STREAM_EVENT, eventType, 0));
}

void PlaybackService::OnPlaybackEvent(int eventType) {
    cursespp::MessageQueue::Instance().Post(
        cursespp::Message::Create(this, MESSAGE_PLAYBACK_EVENT, eventType, 0));
}
