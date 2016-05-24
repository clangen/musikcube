#pragma once

#include "stdafx.h"

#include <app/service/PlaybackService.h>
#include <core/library/ILibrary.h>

using musik::core::audio::Transport;
using musik::core::LibraryPtr;

class GlobalHotkeys {
    public:
        GlobalHotkeys(PlaybackService& playback, LibraryPtr library);
        ~GlobalHotkeys(); /* non-virtual; do not use as a base class */

        bool Handle(int64 ch);

    private:
        PlaybackService& playback;
        Transport& transport;
        LibraryPtr library;
};