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
#define MESSAGE_PREPARE_NEXT_TRACK 1002

class StreamMessage : public cursespp::Message {
    public:
        StreamMessage(IMessageTarget* target, int eventType, const std::string& uri)
        : Message(target, MESSAGE_STREAM_EVENT, eventType, 0) {
            this->uri = uri;
        }

        virtual ~StreamMessage() {
        }

        std::string GetUri() { return this->uri; }
        int GetEventType() { return (int) this->UserData1(); }

    private:
        std::string uri;
};

#define POST(instance, type, user1, user2) \
    cursespp::MessageQueue::Instance().Post( \
        cursespp::Message::Create(instance, type, user1, user2));

#define POST_STREAM_MESSAGE(instance, eventType, uri) \
    cursespp::MessageQueue::Instance().Post( \
        cursespp::IMessagePtr(new StreamMessage(instance, eventType, uri)));

PlaybackService::PlaybackService(LibraryPtr library, ITransport& transport)
: library(library)
, transport(transport)
, playlist(library)
, unshuffled(library)
, repeatMode(RepeatNone) {
    transport.StreamEvent.connect(this, &PlaybackService::OnStreamEvent);
    transport.PlaybackEvent.connect(this, &PlaybackService::OnPlaybackEvent);
    this->index = NO_POSITION;
    this->nextIndex = NO_POSITION;
}

void PlaybackService::PrepareNextTrack() {
    boost::recursive_mutex::scoped_lock lock(this->playlistMutex);

    if (this->Count() > 0) {
        /* repeat track, just keep playing the same thing over and over */
        if (this->repeatMode == RepeatTrack) {
            this->nextIndex = this->index;
            this->transport.PrepareNextTrack(URI_AT_INDEX(this->index));
        }
        else {
            /* normal case, just move forward */
            if (this->playlist.Count() > this->index + 1) {
                if (this->nextIndex != this->index + 1) {
                    this->nextIndex = this->index + 1;
                    this->transport.PrepareNextTrack(URI_AT_INDEX(nextIndex));
                }
            }
            /* repeat list case, wrap around to the beginning if necessary */
            else if (this->repeatMode == RepeatList) {
                if (this->nextIndex != 0) {
                    this->nextIndex = 0;
                    this->transport.PrepareNextTrack(URI_AT_INDEX(nextIndex));
                }
            }
        }
    }
}

void PlaybackService::SetRepeatMode(RepeatMode mode) {
    if (this->repeatMode != mode) {
        this->repeatMode = mode;
        this->ModeChanged();
        POST(this, MESSAGE_PREPARE_NEXT_TRACK, 0, 0);
    }
}

void PlaybackService::ToggleShuffle() {
    boost::recursive_mutex::scoped_lock lock(this->playlistMutex);

    /* remember the ID of the playing track -- we're going to need to look
    it up after the shuffle */
    DBID id = -1;
    if (this->index < this->playlist.Count()) {
        id = this->playlist.GetId(this->index);
    }

    if (this->unshuffled.Count() > 0) { /* shuffled -> unshuffled */
        this->playlist.Clear();
        this->playlist.Swap(this->unshuffled);
        this->Shuffled(false);
    }
    else { /* unshuffled -> shuffle */
        this->unshuffled.CopyFrom(this->playlist);
        this->playlist.Shuffle();
        this->Shuffled(true);
    }

    /* find the new playback index and prefetch the next track */
    if (id != -1) {
        int index = this->playlist.IndexOf(id);
        if (index != -1) {
            this->index = index;
            POST(this, MESSAGE_PREPARE_NEXT_TRACK, 0, 0);
        }
    }
}

void PlaybackService::ProcessMessage(IMessage &message) {
    if (message.Type() == MESSAGE_STREAM_EVENT) {
        StreamMessage* streamMessage = static_cast<StreamMessage*>(&message);

        int64 eventType = streamMessage->GetEventType();

        if (eventType == ITransport::StreamPlaying) {
            if (this->nextIndex != NO_POSITION) {
                /* in most cases when we get here it means that the next track is
                starting, so we want to update our internal index. however, because
                things are asynchronous, this may not always be the case, especially if
                the tracks are very short, or the user is advancing through songs very
                quickly. make compare the track URIs before we update internal state. */
                if (this->GetTrackAtIndex(this->nextIndex)->URI() == streamMessage->GetUri()) {
                    this->index = this->nextIndex;
                    this->nextIndex = NO_POSITION;
                }
            }

            if (this->index != NO_POSITION) {
                TrackPtr track;

                {
                    boost::recursive_mutex::scoped_lock lock(this->playlistMutex);
                    track = this->playlist.Get(this->index);
                }

                this->TrackChanged(this->index, track);
            }

            this->PrepareNextTrack();
        }
    }
    else if (message.Type() == MESSAGE_PLAYBACK_EVENT) {
        int64 eventType = message.UserData1();

        if (eventType == ITransport::PlaybackStopped) {
            this->TrackChanged(NO_POSITION, TrackPtr());
        }
    }
    else if (message.Type() == MESSAGE_PREPARE_NEXT_TRACK) {
        if (transport.GetPlaybackState() != ITransport::PlaybackStopped) {
            this->PrepareNextTrack();
        }
    }
}

bool PlaybackService::Next() {
    if (transport.GetPlaybackState() == ITransport::PlaybackStopped) {
        return false;
    }

    boost::recursive_mutex::scoped_lock lock(this->playlistMutex);

    if (this->playlist.Count() > index + 1) {
        this->Play(index + 1);
        return true;
    }
    else if (this->repeatMode == RepeatList) {
        this->Play(0); /* wrap around */
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
    else if (this->repeatMode == RepeatList) {
        this->Play(this->Count() - 1); /* wrap around */
        return true;
    }

    return false;
}

void PlaybackService::Play(TrackList& tracks, size_t index) {
    /* do the copy outside of the critical section, then swap. */
    TrackList temp(this->library);
    temp.CopyFrom(tracks);

    {
        boost::recursive_mutex::scoped_lock lock(this->playlistMutex);
        this->playlist.Swap(temp);
        this->unshuffled.Clear();
    }

    if (index <= tracks.Count()) {
        this->Play(index);
    }
}

void PlaybackService::CopyTo(TrackList& target) {
    boost::recursive_mutex::scoped_lock lock(this->playlistMutex);
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
    boost::recursive_mutex::scoped_lock lock(this->playlistMutex);
    return this->playlist.Get(index);
}

void PlaybackService::OnStreamEvent(int eventType, std::string uri) {
    POST_STREAM_MESSAGE(this, eventType, uri);
}

void PlaybackService::OnPlaybackEvent(int eventType) {
    POST(this, MESSAGE_PLAYBACK_EVENT, eventType, 0);
}
