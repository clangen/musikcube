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

#include <app/overlay/VisualizerOverlay.h>
#include <app/util/Playback.h>

#include <core/audio/Visualizer.h>

#include <glue/util/Playback.h>

using namespace musik::core;
using namespace musik::core::audio;
using namespace musik::core::sdk;
using namespace musik::cube;

GlobalHotkeys::GlobalHotkeys(musik::core::audio::PlaybackService& playback, ILibraryPtr library)
: playback(playback)
, transport(playback.GetTransport()) {
    this->library = library;
}

GlobalHotkeys::~GlobalHotkeys() {

}

bool GlobalHotkeys::Handle(const std::string& kn) {
    if (Hotkeys::Is(Hotkeys::ToggleMute, kn)) {
        this->playback.ToggleMute();
        return true;
    }
    else if (Hotkeys::Is(Hotkeys::TogglePause, kn)) {
        musik::glue::playback::PauseOrResume(this->transport);
        return true;
    }
    else if (Hotkeys::Is(Hotkeys::VolumeUp, kn)) {
        musik::glue::playback::VolumeUp(this->transport);
        return true;
    }
    else if (Hotkeys::Is(Hotkeys::VolumeDown, kn)) {
        musik::glue::playback::VolumeDown(this->transport);
        return true;
    }
    else if (Hotkeys::Is(Hotkeys::Previous, kn)) {
        this->playback.Previous();
        return true;
    }
    else if (Hotkeys::Is(Hotkeys::Next, kn)) {
        this->playback.Next();
        return true;
    }
    else if (Hotkeys::Is(Hotkeys::SeekBack, kn)) {
        musik::glue::playback::SeekBack(this->playback);
        return true;
    }
    else if (Hotkeys::Is(Hotkeys::SeekForward, kn)) {
        musik::glue::playback::SeekForward(this->playback);
        return true;
    }
    else if (Hotkeys::Is(Hotkeys::ToggleRepeat, kn)) {
        this->playback.ToggleRepeatMode();
        return true;
    }
    else if (Hotkeys::Is(Hotkeys::ToggleShuffle, kn)) {
        this->playback.ToggleShuffle();
        return true;
    }
    else if (Hotkeys::Is(Hotkeys::Stop, kn)) {
        this->playback.Stop();
        return true;
    }
    else if (Hotkeys::Is(Hotkeys::RescanMetadata, kn)) {
        library->Indexer()->Schedule(IIndexer::SyncType::All);
        return true;
    }
    else if (Hotkeys::Is(Hotkeys::ToggleVisualizer, kn)) {
        std::shared_ptr<IVisualizer> selected = vis::SelectedVisualizer();
        if (selected && selected->Visible()) {
            selected->Hide();
        }
        else {
            VisualizerOverlay::Show();
        }
        return true;
    }

    return false;
}
