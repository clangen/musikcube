#include "stdafx.h"
#include "GlobalHotkeys.h"

using musik::core::LibraryPtr;
using musik::core::audio::Transport;
using namespace musik::box;

GlobalHotkeys::GlobalHotkeys(PlaybackService& playback, LibraryPtr library)
: playback(playback)
, transport(playback.GetTransport()) {
    this->library = library;
}

GlobalHotkeys::~GlobalHotkeys() {

}

bool GlobalHotkeys::Handle(const std::string& kn) {
    if (kn == "^P") {
        int state = this->transport.GetPlaybackState();
        if (state == Transport::PlaybackPaused) {
            this->transport.Resume();
        }
        else if (state == Transport::PlaybackPlaying) {
            this->transport.Pause();
        }
        return true;
    }
    if (kn == "ALT_I" || kn == "M-i") {
        this->transport.SetVolume(this->transport.Volume() + 0.05); /* 5% */
        return true;
    }
    else if (kn == "ALT_K" || kn == "M-k") {
        this->transport.SetVolume(this->transport.Volume() - 0.05);
        return true;
    }
    else if (kn == "ALT_J" || kn == "M-j") {
        this->playback.Previous();
        return true;
    }
    else if (kn == "ALT_L" || kn == "M-l") {
        this->playback.Next();
        return true;
    }
    else if (kn == "ALT_U" || kn == "M-u") {
        double time = this->transport.Position();
        this->transport.SetPosition(time - 10.0f);
        return true;
    }
    else if (kn == "ALT_O" || kn == "M-o") {
        double time = this->transport.Position();
        this->transport.SetPosition(time + 10.0f);
        return true;
    }
    else if (kn == "^R") {
        library->Indexer()->Synchronize(true);
        return true;
    }

    return false;
}
