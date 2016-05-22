#include <stdafx.h>
#include "PlaybackService.h"
#include <cursespp/MessageQueue.h>
#include <core/playback/Transport.h>
#include <core/library/LocalLibraryConstants.h>

using musik::core::audio::Transport;
using namespace musik::core::library::constants;

#define URI_AT_INDEX(x) this->playlist.at(index + 1)->URI()

PlaybackService::PlaybackService(Transport& transport)
: transport(transport) {
    transport.PlaybackEvent.connect(this, &PlaybackService::OnTransportEvent);
    this->index = this->next = (size_t) -1;
}


void PlaybackService::ProcessMessage(IMessage &message) {
    
}

void PlaybackService::Start(std::vector<TrackPtr>& tracks, size_t index) {
    this->playlist.clear();
    std::copy(tracks.begin(), tracks.end(), this->playlist.begin());
    this->Start(index);
}

void PlaybackService::Start(size_t index) {
    transport.Start(URI_AT_INDEX(index));
    this->index = index;
}

void PlaybackService::OnTransportEvent(int eventType, std::string uri) {
    if (eventType == Transport::EventAlmostDone) {
        if (this->playlist.size() > index + 1) {
            std::string uri = URI_AT_INDEX(index + 1);
            transport.PrepareNextTrack(uri);
        }
    }
}
