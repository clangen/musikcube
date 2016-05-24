#include "stdafx.h"
#include "GlobalHotkeys.h"

using musik::core::LibraryPtr;

GlobalHotkeys::GlobalHotkeys(Transport& transport, LibraryPtr library) 
: transport(transport) {
    this->library = library;
}

GlobalHotkeys::~GlobalHotkeys() {

}

bool GlobalHotkeys::Handle(int64 ch) {
    std::string kn = keyname((int) ch);

    if (kn == "^P") {
        int state = this->transport.GetPlaybackState();
        if (state == Transport::PlaybackPaused) {
            this->transport.Resume();
        }
        else if (state == Transport::PlaybackPlaying) {
            this->transport.Pause();
        }
    }
    if (kn == "ALT_I") {
        this->transport.SetVolume(this->transport.Volume() + 0.05); /* 5% */
        return true;
    }
    else if (kn == "ALT_K") {
        this->transport.SetVolume(this->transport.Volume() - 0.05);
        return true;
    }
    else if (kn == "^R") {
        library->Indexer()->Synchronize(true);
    }

    return false;
}