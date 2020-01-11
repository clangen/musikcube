//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004-2019 musikcube team
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

#include <stdafx.h>

#include <cursespp/INavigationKeys.h>

namespace musik {
    namespace cube {
        class Hotkeys {
            public:
                enum Id {
                    /* selection */
                    Up = 0,
                    Down,
                    Left,
                    Right,
                    PageUp,
                    PageDown,
                    Home,
                    End,

                    /* navigation */
                    NavigateLibrary,
                    NavigateLibraryBrowse,
                    NavigateLibraryBrowseArtists,
                    NavigateLibraryBrowseAlbums,
                    NavigateLibraryBrowseGenres,
                    NavigateLibraryBrowseAlbumArtists,
                    NavigateLibraryBrowsePlaylists,
                    NavigateLibraryBrowseChooseCategory,
                    NavigateLibraryBrowseDirectories,
                    NavigateLibraryFilter,
                    NavigateLibraryTracks,
                    NavigateLibraryPlayQueue,
                    NavigateConsole,
                    NavigateLyrics,
                    NavigateHotkeys,
                    NavigateJumpToPlaying,
                    NavigateSettings,

                    /* views */
                    ViewRefresh,
                    ToggleVisualizer,
                    ShowEqualizer,

                    /* playback */
                    ToggleMute,
                    TogglePause,
                    Next,
                    Previous,
                    VolumeUp,
                    VolumeDown,
                    SeekForward,
                    SeekForwardProportional,
                    SeekBackProportional,
                    SeekBack,
                    ToggleRepeat,
                    ToggleShuffle,
                    Stop,

                    /* play queue */
                    PlayQueueMoveUp,
                    PlayQueueMoveDown,
                    PlayQueueDelete,
                    PlayQueuePlaylistLoad,
                    PlayQueuePlaylistSave,
                    PlayQueuePlaylistRename,
                    PlayQueuePlaylistDelete,
                    PlayQueueHotSwap,

                    /* browse -> playlists */
                    BrowsePlaylistsNew,
                    BrowsePlaylistsSave,
                    BrowsePlaylistsRename,
                    BrowsePlaylistsDelete,

                    /* tracklist items */
                    TrackListRateTrack,
                    TrackListChangeSortOrder,

                    /* lyrics */
                    LyricsRetry,

                    /* indexer */
                    RescanMetadata,

                    /* hotkeys */
                    HotkeysResetToDefault,
                    HotkeysBackup,

                    /* general */
                    ContextMenu,

                    /* :3 */
                    COUNT
                };

                static bool Is(Id id, const std::string& kn);
                static std::string Get(Id id);
                static void Set(Id id, const std::string& kn);
                static void Reset();
                static std::string Existing(const std::string& kn);
                static std::string Name(Id id);
                static std::string Default(Id id);
                static std::string Custom(Id id);
                static std::shared_ptr<cursespp::INavigationKeys> NavigationKeys();

            private:
                Hotkeys();
        };
    }
}
