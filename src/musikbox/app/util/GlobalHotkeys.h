#pragma once

#include "stdafx.h"

#include <app/service/PlaybackService.h>
#include <core/library/ILibrary.h>

namespace musik {
    namespace box {
        class GlobalHotkeys {
            public:
                GlobalHotkeys(
                    PlaybackService& playback, 
                    musik::core::LibraryPtr library);

                ~GlobalHotkeys(); /* non-virtual; do not use as a base class */

                bool Handle(int64 ch);

            private:
                PlaybackService& playback;
                musik::core::audio::Transport& transport;
                musik::core::LibraryPtr library;
        };
    }
}
