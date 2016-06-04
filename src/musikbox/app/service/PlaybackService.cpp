#include <stdafx.h>
#include "PlaybackService.h"

#include <cursespp/MessageQueue.h>
#include <cursespp/Message.h>

#include <core/playback/Transport.h>
#include <core/library/LocalLibraryConstants.h>

using musik::core::TrackPtr;
using musik::core::audio::Transport;

using cursespp::IMessageTarget;
using cursespp::IMessage;

using namespace musik::core::library::constants;
using namespace musik::box;

#define NO_POSITION (size_t) -1
#define URI_AT_INDEX(x) this->playlist.at(x)->URI()
#define PREVIOUS_GRACE_PERIOD 2.0f
#define MESSAGE_STREAM_EVENT 1000
#define MESSAGE_PLAYBACK_EVENT 1001

PlaybackService::PlaybackService(Transport& transport)
: transport(transport) {
    transport.StreamEvent.connect(this, &PlaybackService::OnStreamEvent);
    transport.PlaybackEvent.connect(this, &PlaybackService::OnPlaybackEvent);
    this->index = NO_POSITION;
    this->nextIndex = NO_POSITION;
}

void PlaybackService::ProcessMessage(IMessage &message) {
    if (message.Type() == MESSAGE_STREAM_EVENT) {
        int64 eventType = message.UserData1();

        if (eventType == Transport::StreamAlmostDone) {
            if (this->playlist.size() > this->index + 1) {
                if (this->nextIndex != this->index + 1) {
                    this->nextIndex = this->index + 1;
                    this->transport.PrepareNextTrack(URI_AT_INDEX(nextIndex));
                }
            }
        }
        else if (eventType == Transport::StreamPlaying) {
            if (this->nextIndex != NO_POSITION) {
                this->index = this->nextIndex;
                this->nextIndex = NO_POSITION;
            }

            if (this->index != NO_POSITION) {
                this->TrackChanged(this->index, this->playlist.at(this->index));
            }
        }
    }
    else if (message.Type() == MESSAGE_PLAYBACK_EVENT) {
        int64 eventType = message.UserData1();

        if (eventType == Transport::PlaybackStopped) {
            this->TrackChanged(NO_POSITION, TrackPtr());
        }
    }
}

bool PlaybackService::Next() {
    if (transport.GetPlaybackState() == Transport::PlaybackStopped) {
        return false;
    }

    if (this->playlist.size() > index + 1) {
        this->Play(index + 1);
        return true;
    }

    return false;
}

bool PlaybackService::Previous() {
    if (transport.GetPlaybackState() == Transport::PlaybackStopped) {
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

void PlaybackService::Play(std::vector<TrackPtr>& tracks, size_t index) {
    /* do the copy outside of the critical section, then swap. */
    std::vector<TrackPtr> temp;
    std::copy(tracks.begin(), tracks.end(), std::back_inserter(temp));

    {
        boost::recursive_mutex::scoped_lock lock(this->stateMutex);
        std::swap(temp, this->playlist);
    }

    this->Play(index);
}

void PlaybackService::Copy(std::vector<musik::core::TrackPtr>& target) {
    boost::recursive_mutex::scoped_lock lock(this->stateMutex);
    std::copy(this->playlist.begin(), this->playlist.end(), std::back_inserter(target));
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
    return this->playlist.at(index);
}

void PlaybackService::OnStreamEvent(int eventType, std::string uri) {
    cursespp::MessageQueue::Instance().Post(
        cursespp::Message::Create(this, MESSAGE_STREAM_EVENT, eventType, 0));
}

void PlaybackService::OnPlaybackEvent(int eventType) {
    cursespp::MessageQueue::Instance().Post(
        cursespp::Message::Create(this, MESSAGE_PLAYBACK_EVENT, eventType, 0));
}
