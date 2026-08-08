//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004-2023 musikcube team
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

/**
 * @file Hotkeys.h
 * @brief Definition and management of the keyboard hotkey bindings.
 * @details Declares the identifier for every configurable hotkey and provides
 *          static helpers to query, set, reset, back up and name the
 *          bindings. Defaults, custom overrides and navigation keys are all
 *          exposed through this interface.
 */

#include <stdafx.h>

#include <cursespp/INavigationKeys.h>

namespace musik {
    namespace cube {
        /**
         * @brief Central registry for user-configurable hotkeys.
         * @details All bindings are persisted as preferences. Custom bindings
         *          take precedence over the built-in defaults. This class is
         *          only used through its static members.
         */
        class Hotkeys {
            public:
                /**
                 * @brief The identifier for each configurable hotkey.
                 */
                enum Id {
                    /* selection */
                    Up = 0,               /**< move the selection up */
                    Down,                 /**< move the selection down */
                    Left,                 /**< move the selection left */
                    Right,                /**< move the selection right */
                    PageUp,               /**< page the selection up */
                    PageDown,             /**< page the selection down */
                    Home,                 /**< jump to the first item */
                    End,                  /**< jump to the last item */

                    /* navigation */
                    NavigateLibrary,                /**< open the library */
                    NavigateLibraryBrowse,          /**< open the library browse view */
                    NavigateLibraryBrowseArtists,   /**< browse artists */
                    NavigateLibraryBrowseAlbums,    /**< browse albums */
                    NavigateLibraryBrowseGenres,    /**< browse genres */
                    NavigateLibraryBrowseAlbumArtists, /**< browse album artists */
                    NavigateLibraryBrowsePlaylists, /**< browse playlists */
                    NavigateLibraryBrowseChooseCategory, /**< choose a category */
                    NavigateLibraryBrowseDirectories,   /**< browse directories */
                    NavigateLibraryFilter,          /**< focus the category filter */
                    NavigateLibraryTracks,          /**< open the tracks view */
                    NavigateLibraryPlayQueue,       /**< open the play queue */
                    NavigateConsole,                /**< open the console */
                    NavigateLyrics,                 /**< open the lyrics view */
                    NavigateHotkeys,                /**< open the hotkeys view */
                    NavigateJumpToPlaying,          /**< jump to the playing track */
                    NavigateSettings,               /**< open the settings view */

                    /* views */
                    ViewRefresh,        /**< refresh the current view */
                    ToggleVisualizer,   /**< toggle the visualizer overlay */
                    ShowEqualizer,      /**< show the equalizer overlay */

                    /* playback */
                    ToggleMute,                 /**< toggle mute */
                    TogglePause,                /**< toggle pause */
                    Next,                       /**< play the next track */
                    Previous,                   /**< play the previous track */
                    VolumeUp,                   /**< increase the volume */
                    VolumeDown,                 /**< decrease the volume */
                    SeekForward,                /**< seek forward */
                    SeekForwardProportional,    /**< seek forward by a fixed ratio */
                    SeekBackProportional,       /**< seek backward by a fixed ratio */
                    SeekBack,                   /**< seek backward */
                    ToggleRepeat,               /**< toggle repeat mode */
                    ToggleShuffle,              /**< toggle shuffle */
                    Stop,                       /**< stop playback */

                    /* play queue */
                    PlayQueueMoveUp,        /**< move the selected track up */
                    PlayQueueMoveDown,      /**< move the selected track down */
                    PlayQueueDelete,        /**< delete the selected track */
                    PlayQueuePlaylistLoad,  /**< load a playlist */
                    PlayQueuePlaylistSave,  /**< save the queue as a playlist */
                    PlayQueuePlaylistRename,/**< rename a playlist */
                    PlayQueuePlaylistDelete,/**< delete a playlist */
                    PlayQueueHotSwap,       /**< play from the selection in place */
                    PlayQueueClear,         /**< clear the queue */

                    /* browse */
                    BrowseCategoryFilter,   /**< toggle the browse category filter */

                    /* browse -> playlists */
                    BrowsePlaylistsNew,     /**< create a new playlist */
                    BrowsePlaylistsSave,    /**< save the browse playlist */
                    BrowsePlaylistsRename,  /**< rename the browse playlist */
                    BrowsePlaylistsDelete,  /**< delete the browse playlist */

                    /* tracklist items */
                    TrackListRateTrack,         /**< rate the selected track */
                    TrackListChangeSortOrder,   /**< change the track sort order */
                    TrackListNextGroup,         /**< jump to the next group */
                    TrackListPreviousGroup,     /**< jump to the previous group */
                    TrackListPlayFromTop,       /**< play from the top of the list */

                    /* search input */
                    SearchInputToggleMatchType, /**< toggle the search match type */

                    /* lyrics */
                    LyricsRetry,        /**< retry the lyrics lookup */

                    /* indexer */
                    RescanMetadata,     /**< rescan track metadata */

                    /* hotkeys */
                    HotkeysResetToDefault,  /**< reset all hotkeys to default */
                    HotkeysBackup,          /**< back up the hotkey configuration */

                    /* general */
                    ContextMenu,        /**< open the context menu */

                    /* :3 */
                    COUNT               /**< the number of hotkey ids */
                };

                /**
                 * @brief Checks whether a key sequence maps to the given id.
                 * @param id the hotkey to test
                 * @param kn the key sequence
                 * @return true if the sequence matches the hotkey
                 */
                static bool Is(Id id, const std::string& kn);
                /**
                 * @brief Returns the effective binding for the given id.
                 * @param id the hotkey
                 * @return the key sequence, custom if set, otherwise default
                 */
                static std::string Get(Id id);
                /**
                 * @brief Overrides the binding for the given id.
                 * @param id the hotkey
                 * @param kn the new key sequence
                 */
                static void Set(Id id, const std::string& kn);
                /**
                 * @brief Clears all custom bindings.
                 */
                static void Reset();
                /**
                 * @brief Finds the id currently bound to a key sequence.
                 * @param kn the key sequence to look up
                 * @return the name of the bound hotkey, or empty
                 */
                static std::string Existing(const std::string& kn);
                /**
                 * @brief Returns the localized display name of a hotkey.
                 * @param id the hotkey
                 * @return the display name
                 */
                static std::string Name(Id id);
                /**
                 * @brief Returns the built-in default binding of a hotkey.
                 * @param id the hotkey
                 * @return the default key sequence
                 */
                static std::string Default(Id id);
                /**
                 * @brief Returns the user overridden binding of a hotkey.
                 * @param id the hotkey
                 * @return the custom key sequence, or empty if not overridden
                 */
                static std::string Custom(Id id);
                /**
                 * @brief Returns the navigation keys derived from the bindings.
                 * @return the navigation keys implementation
                 */
                static std::shared_ptr<cursespp::INavigationKeys> NavigationKeys();

            private:
                DELETE_CLASS_DEFAULTS(Hotkeys)
        };
    }
}
