#include "stdafx.h"
#include "GlobalHotkeys.h"

GlobalHotkeys::GlobalHotkeys(Transport& transport) 
: transport(transport) {

}

GlobalHotkeys::~GlobalHotkeys() {

}

bool GlobalHotkeys::Handle(int64 ch) {
    std::string kn = keyname((int) ch);

    if (kn == "ALT_K") {
        int state = this->transport.GetPlaybackState();
        if (state == Transport::StatePaused) {
            this->transport.Resume();
        }
        else if (state == Transport::StatePlaying) {
            this->transport.Pause();
        }
    }
    if (kn == "ALT_L") {
        this->transport.SetVolume(this->transport.Volume() + 0.05); /* 5% */
        return true;
    }
    else if (kn == "ALT_J") {
        this->transport.SetVolume(this->transport.Volume() - 0.05);
        return true;
    }

    return false;
}