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

#include "stdafx.h"
#include "GlobalHotkeys.h"
#include "Hotkeys.h"
#include <app/util/Playback.h>

using musik::core::LibraryPtr;
using musik::core::audio::ITransport;
using namespace musik::box;

Hotkeys hotkeys;

GlobalHotkeys::GlobalHotkeys(PlaybackService& playback, LibraryPtr library)
: playback(playback)
, transport(playback.GetTransport()) {
    this->library = library;
}

GlobalHotkeys::~GlobalHotkeys() {

}

bool GlobalHotkeys::Handle(const std::string& kn) {
    if (hotkeys.Is(Hotkeys::TogglePause, kn)) {
        playback::PauseOrResume(this->transport);
        return true;
    }
    else if (hotkeys.Is(Hotkeys::VolumeUp, kn)) {
        this->transport.SetVolume(this->transport.Volume() + 0.05);
        return true;
    }
    else if (hotkeys.Is(Hotkeys::VolumeDown, kn)) {
        this->transport.SetVolume(this->transport.Volume() - 0.05);
        return true;
    }
    else if (hotkeys.Is(Hotkeys::Previous, kn)) {
        this->playback.Previous();
        return true;
    }
    else if (hotkeys.Is(Hotkeys::Next, kn)) {
        this->playback.Next();
        return true;
    }
    else if (hotkeys.Is(Hotkeys::SeekBack, kn)) {
        double time = this->transport.Position();
        this->transport.SetPosition(time - 10.0f);
        return true;
    }
    else if (hotkeys.Is(Hotkeys::SeekForward, kn)) {
        double time = this->transport.Position();
        this->transport.SetPosition(time + 10.0f);
        return true;
    }
    else if (hotkeys.Is(Hotkeys::ToggleRepeat, kn)) {
        playback::ToggleRepeatMode(this->playback);
        return true;
    }
    else if (hotkeys.Is(Hotkeys::ToggleShuffle, kn)) {
        this->playback.ToggleShuffle();
        return true;
    }
    else if (hotkeys.Is(Hotkeys::Stop, kn)) {
        this->playback.Stop();
        return true;
    }
    else if (hotkeys.Is(Hotkeys::RescanMetadata, kn)) {
        library->Indexer()->Synchronize(true);
        return true;
    }

    return false;
}
