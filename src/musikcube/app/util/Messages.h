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

#pragma once

#include "stdafx.h"

#include <core/audio/PlaybackService.h>
#include <app/window/TrackListView.h>

namespace musik {
    namespace cube {
        namespace message {
            static const int JumpToCategory = 1024;

            namespace category { /* User1 */
                static const int Album = 0;
                static const int AlbumArtist = 1;
                static const int Artist = 2;
                static const int Genre = 3;
            }

            static const int IndexerStarted = 1025;
            static const int IndexerProgress = 1026;
            static const int IndexerFinished = 1027;

            static const int RequeryTrackList = 1028;

            static const int RefreshTransport = 1029;

            static const int RefreshLogs = 1030;

            static const int TracksAddedToPlaylist = 1031;
            static const int PlaylistCreated = 1032;

            static const int UpdateCheckFinished = 1033;

            static const int JumpToConsole = 1034;
            static const int JumpToLibrary = 1035;
            static const int JumpToSettings = 1036;
        }
    }
}
