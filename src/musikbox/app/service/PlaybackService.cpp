#include <stdafx.h>
#include "PlaybackService.h"

#include <cursespp/MessageQueue.h>
#include <cursespp/Message.h>

#include <core/playback/Transport.h>
#include <core/library/LocalLibraryConstants.h>

using musik::core::audio::Transport;

using namespace musik::core::library::constants;
using namespace musik::box;

#define URI_AT_INDEX(x) this->playlist.at(x)->URI()
#define PREVIOUS_GRACE_PERIOD 2.0f

PlaybackService::PlaybackService(Transport& transport)
: transport(transport) {
    transport.StreamEvent.connect(this, &PlaybackService::OnStreamEvent);
    this->index = (size_t) -1;
}

void PlaybackService::ProcessMessage(IMessage &message) {

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
    this->playlist.clear();
    std::copy(tracks.begin(), tracks.end(), std::back_inserter(this->playlist));
    this->Play(index);
}

void PlaybackService::Play(size_t index) {
    transport.Stop();
    std::string uri = URI_AT_INDEX(index);
    transport.Start(uri);
    this->index = index;
}

void PlaybackService::OnStreamEvent(int eventType, std::string uri) {
    if (eventType == Transport::StreamAlmostDone) {
        if (this->playlist.size() > this->index + 1) {
            this->transport.PrepareNextTrack(URI_AT_INDEX(index + 1));
            index++;
        }
    }
}
