//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2007-2017 musikcube team
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

#include <string>
#include "Util.h"

//#define ENABLE_DEBUG 1

namespace defaults {
    static const int websocket_server_port = 7905;
    static const int http_server_port = 7906;
    static const std::string password = "";
    static const int transcoder_cache_count = 50;
    static const bool transcoder_synchronous = false;
    static const bool transcoder_synchronous_fallback = false;
}

namespace prefs {
    static const std::string websocket_server_enabled = "websocket_server_enabled";
    static const std::string websocket_server_port = "websocket_server_port";
    static const std::string http_server_enabled = "http_server_enabled";
    static const std::string http_server_port = "http_server_port";
    static const std::string transcoder_cache_count = "transcoder_cache_count";
    static const std::string transcoder_synchronous = "transcoder_synchronous";
    static const std::string transcoder_synchronous_fallback = "transcoder_synchronous_fallback";
}

namespace message {
    static const std::string name = "name";
    static const std::string id = "id";
    static const std::string type = "type";
    static const std::string options = "options";
}

namespace key {
    static const std::string error = "error";
    static const std::string state = "state";
    static const std::string volume = "volume";
    static const std::string position = "position";
    static const std::string repeat_mode = "repeat_mode";
    static const std::string shuffled = "shuffled";
    static const std::string muted = "muted";
    static const std::string play_queue_count = "track_count";
    static const std::string play_queue_position = "play_queue_position";
    static const std::string playing_duration = "playing_duration";
    static const std::string playing_current_time = "playing_current_time";
    static const std::string playing_track = "playing_track";
    static const std::string title = "title";
    static const std::string external_id = "external_id";
    static const std::string external_ids = "external_ids";
    static const std::string filename = "filename";
    static const std::string duration = "duration";
    static const std::string artist = "artist";
    static const std::string album = "album";
    static const std::string album_artist = "album_artist";
    static const std::string genre = "genre";
    static const std::string thumbnail_id = "thumbnail_id";
    static const std::string genre_id = "visual_genre_id";
    static const std::string artist_id = "visual_artist_id";
    static const std::string track_num = "track";
    static const std::string album_artist_id = "album_artist_id";
    static const std::string album_id = "album_id";
    static const std::string category = "category";
    static const std::string category_id = "category_id";
    static const std::string filter = "filter";
    static const std::string id = "id";
    static const std::string ids = "ids";
    static const std::string value = "value";
    static const std::string data = "data";
    static const std::string limit = "limit";
    static const std::string offset = "offset";
    static const std::string count_only = "count_only";
    static const std::string ids_only = "ids_only";
    static const std::string count = "count";
    static const std::string success = "success";
    static const std::string index = "index";
    static const std::string delta = "delta";
    static const std::string relative = "relative";
    static const std::string password = "password";
    static const std::string authenticated = "authenticated";
    static const std::string playlist_id = "playlist_id";
    static const std::string playlist_name = "playlist_name";
}

namespace value {
    static const std::string invalid = "invalid";
    static const std::string unauthenticated = "unauthenticated";
    static const std::string up = "up";
    static const std::string down = "down";
    static const std::string delta = "delta";
}

namespace type {
    static const std::string request = "request";
    static const std::string response = "response";
    static const std::string broadcast = "broadcast";
}

namespace request {
    static const std::string authenticate = "authenticate";
    static const std::string ping = "ping";
    static const std::string pause_or_resume = "pause_or_resume";
    static const std::string stop = "stop";
    static const std::string previous = "previous";
    static const std::string next = "next";
    static const std::string play_at_index = "play_at_index";
    static const std::string toggle_shuffle = "toggle_shuffle";
    static const std::string toggle_repeat = "toggle_repeat";
    static const std::string set_volume = "set_volume";
    static const std::string seek_to = "seek_to";
    static const std::string seek_relative = "seek_relative";
    static const std::string toggle_mute = "toggle_mute";
    static const std::string get_playback_overview = "get_playback_overview";
    static const std::string get_current_time = "get_current_time";
    static const std::string query_category = "query_category";
    static const std::string query_tracks = "query_tracks";
    static const std::string query_albums = "query_albums";
    static const std::string query_tracks_by_category = "query_tracks_by_category";
    static const std::string play_all_tracks = "play_all_tracks";
    static const std::string play_tracks = "play_tracks";
    static const std::string play_tracks_by_category = "play_tracks_by_category";
    static const std::string query_play_queue_tracks = "query_play_queue_tracks";
    static const std::string get_environment = "get_environment";
    static const std::string save_playlist = "save_playlist";
    static const std::string rename_playlist = "rename_playlist";
    static const std::string delete_playlist = "delete_playlist";
}

namespace fragment {
    static const std::string audio = "audio";
    static const std::string id = "id";
    static const std::string external_id = "external_id";
}

namespace broadcast {
    static const std::string playback_overview_changed = "playback_overview_changed";
    static const std::string play_queue_changed = "play_queue_changed";
}

static auto PLAYBACK_STATE_TO_STRING = makeBimap<musik::core::sdk::PlaybackState, std::string>({
    { musik::core::sdk::PlaybackStopped, "stopped" },
    { musik::core::sdk::PlaybackPlaying, "playing" },
    { musik::core::sdk::PlaybackPaused, "paused" }
});

static auto REPEAT_MODE_TO_STRING = makeBimap<musik::core::sdk::RepeatMode, std::string>({
    { musik::core::sdk::RepeatNone, "none" },
    { musik::core::sdk::RepeatTrack, "track" },
    { musik::core::sdk::RepeatList, "list" }
});