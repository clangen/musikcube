#pragma once

#include "stdafx.h"

#include <core/playback/Transport.h>
#include <core/library/ILibrary.h>

using musik::core::audio::Transport;
using musik::core::LibraryPtr;

class GlobalHotkeys {
    public:
        GlobalHotkeys(Transport& transport, LibraryPtr library);
        ~GlobalHotkeys(); /* non-virtual; do not use as a base class */

        bool Handle(int64 ch);

    private:
        Transport& transport;
        LibraryPtr library;
};