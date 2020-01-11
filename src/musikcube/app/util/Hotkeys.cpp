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

#include <stdafx.h>
#include "Hotkeys.h"
#include <core/support/Preferences.h>
#include <app/util/Playback.h>
#include <unordered_map>
#include <unordered_set>
#include <iostream>

using namespace musik::cube;
using namespace musik::core;

#define ENSURE_LOADED() { if (!prefs) { loadPreferences(); } }

using Id = Hotkeys::Id;

/* sigh: http://stackoverflow.com/a/24847480 */
struct EnumHasher {
    template <typename T>
    std::size_t operator()(T t) const {
        return static_cast<std::size_t>(t);
    }
};

/* map from internal ID to user-friendly JSON key name */
static std::unordered_map<std::string, Id> NAME_TO_ID = {
    { "key_up", Id::Up },
    { "key_down", Id::Down },
    { "key_left", Id::Left },
    { "key_right", Id::Right },
    { "key_page_up", Id::PageUp },
    { "key_page_down", Id::PageDown },
    { "key_home", Id::Home },
    { "key_end", Id::End },

    { "navigate_library", Id::NavigateLibrary },
    { "navigate_library_browse", Id::NavigateLibraryBrowse },
    { "navigate_library_browse_artists", Id::NavigateLibraryBrowseArtists },
    { "navigate_library_browse_albums", Id::NavigateLibraryBrowseAlbums },
    { "navigate_library_browse_genres", Id::NavigateLibraryBrowseGenres },
    { "navigate_library_album_artists", Id::NavigateLibraryBrowseAlbumArtists },
    { "navigate_library_playlists", Id::NavigateLibraryBrowsePlaylists },
    { "navigate_library_choose_category", Id::NavigateLibraryBrowseChooseCategory },
    { "navigate_library_browse_directories", Id::NavigateLibraryBrowseDirectories },
    { "navigate_library_filter", Id::NavigateLibraryFilter },
    { "navigate_library_tracks", Id::NavigateLibraryTracks },
    { "navigate_library_play_queue", Id::NavigateLibraryPlayQueue },
    { "navigate_settings", Id::NavigateSettings },
    { "navigate_console", Id::NavigateConsole },
    { "navigate_lyrics", Id::NavigateLyrics },
    { "navigate_hotkeys", Id::NavigateHotkeys},
    { "navigate_jump_to_playing", Id::NavigateJumpToPlaying },

    { "play_queue_move_up", Id::PlayQueueMoveUp },
    { "play_queue_move_down", Id::PlayQueueMoveDown },
    { "play_queue_delete", Id::PlayQueueDelete },
    { "play_queue_playlist_load", Id::PlayQueuePlaylistLoad },
    { "play_queue_playlist_save", Id::PlayQueuePlaylistSave },
    { "play_queue_playlist_rename", Id::PlayQueuePlaylistRename },
    { "play_queue_playlist_delete", Id::PlayQueuePlaylistDelete },
    { "play_queue_hot_swap", Id::PlayQueueHotSwap },

    { "browse_playlists_new", Id::BrowsePlaylistsNew },
    { "browse_playlists_save", Id::BrowsePlaylistsSave },
    { "browse_playlists_rename", Id::BrowsePlaylistsRename },
    { "browse_playlists_delete", Id::BrowsePlaylistsDelete },

    { "track_list_change_sort_order", Id::TrackListChangeSortOrder },

    { "track_list_rate_track", Id::TrackListRateTrack },

    { "lyrics_retry", Id::LyricsRetry },

    { "playback_toggle_mute", Id::ToggleMute },
    { "playback_toggle_pause", Id::TogglePause },
    { "playback_next", Id::Next },
    { "playback_previous", Id::Previous },
    { "playback_volume_up", Id::VolumeUp },
    { "playback_volume_down", Id::VolumeDown },
    { "playback_seek_forward", Id::SeekForward },
    { "playback_seek_forward_proportional", Id::SeekForwardProportional },
    { "playback_seek_back_proportional", Id::SeekBackProportional },
    { "playback_seek_back", Id::SeekBack },
    { "playback_toggle_repeat", Id::ToggleRepeat },
    { "playback_toggle_shuffle", Id::ToggleShuffle },
    { "playback_stop", Id::Stop },

    { "view_refresh", Id::ViewRefresh },

    { "toggle_visualizer", Id::ToggleVisualizer },

    { "show_equalizer", Id::ShowEqualizer },

    { "metadata_rescan", Id::RescanMetadata },

    { "hotkeys_reset_to_default", Id::HotkeysResetToDefault },
    { "hotkeys_backup", Id::HotkeysBackup },

    { "context_menu", Id::ContextMenu }
};

/* default hotkeys */
static std::unordered_map<Id, std::string, EnumHasher> ID_TO_DEFAULT = {
    { Id::Up, "KEY_UP" },
    { Id::Down, "KEY_DOWN" },
    { Id::Left, "KEY_LEFT" },
    { Id::Right, "KEY_RIGHT" },
    { Id::PageUp, "KEY_PPAGE" },
    { Id::PageDown, "KEY_NPAGE" },
    { Id::Home, "KEY_HOME" },
    { Id::End, "KEY_END" },

    { Id::NavigateLibrary, "a" },
    { Id::NavigateLibraryBrowse, "b" },
    { Id::NavigateLibraryBrowseArtists, "1" },
    { Id::NavigateLibraryBrowseAlbums, "2" },
    { Id::NavigateLibraryBrowseGenres, "3" },
    { Id::NavigateLibraryBrowseAlbumArtists, "4" },
    { Id::NavigateLibraryBrowsePlaylists, "5" },
    { Id::NavigateLibraryBrowseChooseCategory, "6" },
    { Id::NavigateLibraryBrowseDirectories, "d" },

    { Id::NavigateLibraryFilter, "f" },
    { Id::NavigateLibraryTracks, "t" },
    { Id::NavigateLibraryPlayQueue, "n" },
    { Id::NavigateSettings, "s" },
    { Id::NavigateConsole, "`" },
    { Id::NavigateLyrics, "^L" },
    { Id::NavigateHotkeys, "?" },
    { Id::NavigateJumpToPlaying, "x" },

#ifdef __APPLE__
    /* M-up, M-down don't seem to work on iTerm2, and the delete
    key doesn't exist on most macbooks. ugly special mappings here. */
    { Id::PlayQueueMoveUp, "CTL_UP" },
    { Id::PlayQueueMoveDown, "CTL_DOWN" },
    { Id::PlayQueueDelete, "KEY_BACKSPACE" },
#else
    { Id::PlayQueueMoveUp, "M-up" },
    { Id::PlayQueueMoveDown, "M-down" },
    { Id::PlayQueueDelete, "KEY_DC" },
#endif
    { Id::PlayQueuePlaylistLoad, "M-l" },
    { Id::PlayQueuePlaylistSave, "M-s" },
    { Id::PlayQueuePlaylistRename, "M-r" },
    { Id::PlayQueuePlaylistDelete, "M-x" },
    { Id::PlayQueueHotSwap, "M-a" },

    { Id::BrowsePlaylistsSave, "M-s" },
    { Id::BrowsePlaylistsNew, "M-n" },
    { Id::BrowsePlaylistsRename, "M-r" },
#ifdef __APPLE__
    { Id::BrowsePlaylistsDelete, "KEY_BACKSPACE" },
#else
    { Id::BrowsePlaylistsDelete, "KEY_DC" },
#endif

    { Id::TrackListChangeSortOrder, "^S" },

    { Id::TrackListRateTrack, "r" },

    { Id::LyricsRetry, "r" },

    { Id::ToggleMute, "m" },
    { Id::TogglePause, "^P" },
    { Id::Next, "l" },
    { Id::Previous, "j" },
    { Id::VolumeUp, "i" },
    { Id::VolumeDown, "k" },
    { Id::SeekForward, "o" },
    { Id::SeekBack, "u" },
    { Id::SeekForwardProportional, "p" },
    { Id::SeekBackProportional, "y" },
    { Id::ToggleRepeat, "." },
    { Id::ToggleShuffle, "," },
    { Id::Stop, "^X" },

    { Id::ViewRefresh, "KEY_F(5)" },

    { Id::ToggleVisualizer, "v" },

    { Id::ShowEqualizer, "^E" },

    { Id::RescanMetadata, "^R"},

    { Id::HotkeysResetToDefault, "M-r" },
    { Id::HotkeysBackup, "M-b" },

    { Id::ContextMenu, "M-enter" }
};

/* custom keys */
static std::unordered_map<Id, std::string, EnumHasher> customIdToKey;

/* preferences file */
static std::shared_ptr<Preferences> prefs;

static void savePreferences() {
    for (const auto& pair : NAME_TO_ID) {
        prefs->SetString(
            pair.first.c_str(),
            Hotkeys::Get(pair.second).c_str());
    }

    prefs->Save();
}

static void loadPreferences() {
    prefs = Preferences::ForComponent("hotkeys", Preferences::ModeReadWrite);

    try {
        if (prefs) {
            std::vector<std::string> names;
            prefs->GetKeys(names);
            for (auto n : names) {
                auto it = NAME_TO_ID.find(n);
                if (it != NAME_TO_ID.end()) {
                    customIdToKey[it->second] = prefs->GetString(n);
                }
            }
        }

        /* write back to disk; this way any new hotkey defaults are picked
        up and saved so the user can edit them easily. */
        savePreferences();
    }
    catch (...) {
        std::cerr << "failed to load hotkeys.json! default hotkeys selected.";
        customIdToKey.clear();
    }
}

Hotkeys::Hotkeys() {
}

bool Hotkeys::Is(Id id, const std::string& kn) {
    if (!prefs) {
        loadPreferences();
    }

    /* see if the user has specified a custom value for this hotkey. if
    they have, compare it to the custom value. */
    auto custom = customIdToKey.find(id);
    if (custom != customIdToKey.end()) {
        return (custom->second == kn);
    }

    /* otherwise, let's compare against the default key */
    auto it = ID_TO_DEFAULT.find(id);
    if (it != ID_TO_DEFAULT.end() && it->second == kn) {
        return true;
    }

    return false;
}

template <typename T>
std::string find(Id id, T& map) {
    auto it = map.find(id);
    if (it != map.end()) {
        return it->second;
    }
    return "";
}

template <typename T>
Hotkeys::Id find(const std::string& kn, T& map) {
    for (auto it : map) {
        if (it.second == kn) {
            return it.first;
        }
    }
    return Hotkeys::COUNT;
}

std::string Hotkeys::Default(Id id) {
    ENSURE_LOADED()
    return find(id, ID_TO_DEFAULT);
}

std::string Hotkeys::Custom(Id id) {
    ENSURE_LOADED()
    return find(id, customIdToKey);
}

std::string Hotkeys::Get(Id id) {
    auto kn = Custom(id);
    return kn.size() ? kn : Default(id);
}

void Hotkeys::Set(Id id, const std::string& kn) {
    customIdToKey[id] = kn;
    savePreferences();
}

void Hotkeys::Reset() {
    customIdToKey.clear();
    savePreferences();
    loadPreferences();
}

std::string Hotkeys::Existing(const std::string& kn) {
    auto id = find(kn, customIdToKey);
    if (id == Hotkeys::COUNT) {
        id = find(kn, ID_TO_DEFAULT);
        if (customIdToKey.find(id) != customIdToKey.end()) {
            /* we found a default key for this one, but that default
            binding has already been overridden! ensure we return
            that it's available. */
            id = Hotkeys::COUNT;
        }
    }
    return id != Hotkeys::COUNT ? Name(id) : "";
}

std::string Hotkeys::Name(Id id) {
    for (auto entry : NAME_TO_ID) {
        if (entry.second == id) {
            return entry.first;
        }
    }
    return "<error>";
}

class NavigationKeysImpl : public cursespp::INavigationKeys {
    public:
        virtual bool Up(const std::string& key) override { return Up() == key; }
        virtual bool Down(const std::string& key) override { return Down() == key; }
        virtual bool Left(const std::string& key) override { return Left() == key; }
        virtual bool Right(const std::string& key) override { return Right() == key; }
        virtual bool PageUp(const std::string& key) override { return PageUp() == key; }
        virtual bool PageDown(const std::string& key) override { return PageDown() == key; }
        virtual bool Home(const std::string& key) override { return Home() == key; }
        virtual bool End(const std::string& key) override { return End() == key; }
        virtual bool Next(const std::string& key) override { return Next() == key; }
        virtual bool Prev(const std::string& key) override { return Prev() == key; }
        virtual bool Mode(const std::string& key) override { return Mode() == key; }

        virtual std::string Up() override { return Hotkeys::Get(Hotkeys::Up); }
        virtual std::string Down() override { return Hotkeys::Get(Hotkeys::Down); }
        virtual std::string Left() override { return Hotkeys::Get(Hotkeys::Left); }
        virtual std::string Right() override { return Hotkeys::Get(Hotkeys::Right); }
        virtual std::string PageUp() override { return Hotkeys::Get(Hotkeys::PageUp); }
        virtual std::string PageDown() override { return Hotkeys::Get(Hotkeys::PageDown); }
        virtual std::string Home() override { return Hotkeys::Get(Hotkeys::Home); }
        virtual std::string End() override { return Hotkeys::Get(Hotkeys::End); }
        virtual std::string Next() override { return "KEY_TAB"; }
        virtual std::string Prev() override { return "KEY_BTAB"; }
        virtual std::string Mode() override { return "^["; }
};

std::shared_ptr<cursespp::INavigationKeys> Hotkeys::NavigationKeys() {
    return std::shared_ptr<cursespp::INavigationKeys>(new NavigationKeysImpl());
}
