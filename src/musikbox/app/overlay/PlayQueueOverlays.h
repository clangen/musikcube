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

#include <core/audio/PlaybackService.h>
#include <core/library/ILibrary.h>
#include <core/library/query/local/TrackListQueryBase.h>
#include <app/window/TrackListView.h>

namespace musik {
    namespace box {
        class PlayQueueOverlays {
            public:
                using PlaylistSelectedCallback = std::function<void(DBID)>;

                static void ShowAddTrackOverlay(
                    musik::core::audio::PlaybackService& playback,
                    unsigned long long trackId);

                static void ShowAddCategoryOverlay(
                    musik::core::audio::PlaybackService& playback,
                    musik::core::ILibraryPtr library,
                    const std::string& fieldColumn,
                    DBID fieldId);

                static void ShowLoadPlaylistOverlay(
                    musik::core::audio::PlaybackService& playback,
                    musik::core::ILibraryPtr library,
                    PlaylistSelectedCallback callback);

                static void ShowSavePlaylistOverlay(
                    musik::core::audio::PlaybackService& playback,
                    musik::core::ILibraryPtr library,
                    DBID selectedPlaylistId = -1);

                static void ShowRenamePlaylistOverlay(
                    musik::core::ILibraryPtr library);

                static void ShowDeletePlaylistOverlay(
                    musik::core::ILibraryPtr library);

            private:
                PlayQueueOverlays();
        };
    }
}
