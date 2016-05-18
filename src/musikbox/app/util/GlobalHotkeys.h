#pragma once

#include "stdafx.h"

#include <core/playback/Transport.h>

using musik::core::audio::Transport;

class GlobalHotkeys {
    public:
        GlobalHotkeys(Transport& transport);
        ~GlobalHotkeys(); /* non-virtual; do not use as a base class */

        bool Handle(int64 ch);

    private:
        Transport& transport;
};