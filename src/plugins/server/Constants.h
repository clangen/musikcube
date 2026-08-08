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

/// @file Constants.h
/// @brief Shared string constants and mappings for the streaming server.
/// @details Groups the default configuration values, preference keys, JSON
/// message keys, request names, broadcast names and enum-to-string mappings
/// used by the HTTP and WebSocket servers so the wire protocol stays
/// consistent across clients.

#include <string>
#include <unordered_map>
#include "Util.h"

//#define ENABLE_DEBUG 1

/** @brief Default server configuration values. */
namespace defaults {
    /** @brief Default WebSocket server port. */
    static const int websocket_server_port = 7905;
    /** @brief Default HTTP server port. */
    static const int http_server_port = 7906;
    /** @brief Default server password (empty = no authentication). */
    static const std::string password = "";
    /** @brief Default number of transcoded files kept in the cache. */
    static const int transcoder_cache_count = 50;
    /** @brief Default maximum concurrent active transcodes. */
    static const int transcoder_max_active_count = 4;
    /** @brief Whether the server listens on IPv6 by default. */
    static const bool use_ipv6 = false;
    /** @brief Whether transcodes are synchronous by default. */
    static const bool transcoder_synchronous = false;
    /** @brief Whether synchronous transcoding falls back on-demand. */
    static const bool transcoder_synchronous_fallback = false;
}

/** @brief Preference key names persisted by the plugin. */
namespace prefs {
    /** @brief Preference: enable the WebSocket server. */
    static const std::string websocket_server_enabled = "websocket_server_enabled";
    /** @brief Preference: WebSocket server port. */
    static const std::string websocket_server_port = "websocket_server_port";
    /** @brief Preference: enable the HTTP server. */
    static const std::string http_server_enabled = "http_server_enabled";
    /** @brief Preference: HTTP server port. */
    static const std::string http_server_port = "http_server_port";
    /** @brief Preference: listen on IPv6. */
    static const std::string use_ipv6 = "use_ipv6";
    /** @brief Preference: transcoder cache size. */
    static const std::string transcoder_cache_count = "transcoder_cache_count";
    /** @brief Preference: max active transcodes. */
    static const std::string transcoder_max_active_count = "transcoder_max_active_count";
    /** @brief Preference: synchronous transcoding. */
    static const std::string transcoder_synchronous = "transcoder_synchronous";
    /** @brief Preference: synchronous transcoding fallback. */
    static const std::string transcoder_synchronous_fallback = "transcoder_synchronous_fallback";
}

/** @brief Common JSON message field names. */
namespace message {
    /** @brief Message field: name. */
    static const std::string name = "name";
    /** @brief Message field: id. */
    static const std::string id = "id";
    /** @brief Message field: device id. */
    static const std::string device_id = "device_id";
    /** @brief Message field: type. */
    static const std::string type = "type";
    /** @brief Message field: options. */
    static const std::string options = "options";
}

/** @brief JSON key names used in request and response payloads. */
namespace key {
    /** @brief Key: error. */
    static const std::string error = "error";
    /** @brief Key: state. */
    static const std::string state = "state";
    /** @brief Key: volume. */
    static const std::string volume = "volume";
    /** @brief Key: position. */
    static const std::string position = "position";
    /** @brief Key: repeat mode. */
    static const std::string repeat_mode = "repeat_mode";
    /** @brief Key: shuffled. */
    static const std::string shuffled = "shuffled";
    /** @brief Key: muted. */
    static const std::string muted = "muted";
    /** @brief Key: play queue track count. */
    static const std::string play_queue_count = "track_count";
    /** @brief Key: play queue position. */
    static const std::string play_queue_position = "play_queue_position";
    /** @brief Key: playing duration. */
    static const std::string playing_duration = "playing_duration";
    /** @brief Key: playing current time. */
    static const std::string playing_current_time = "playing_current_time";
    /** @brief Key: playing track. */
    static const std::string playing_track = "playing_track";
    /** @brief Key: title. */
    static const std::string title = "title";
    /** @brief Key: external id. */
    static const std::string external_id = "external_id";
    /** @brief Key: external ids. */
    static const std::string external_ids = "external_ids";
    /** @brief Key: filename. */
    static const std::string filename = "filename";
    /** @brief Key: duration. */
    static const std::string duration = "duration";
    /** @brief Key: artist. */
    static const std::string artist = "artist";
    /** @brief Key: album. */
    static const std::string album = "album";
    /** @brief Key: album artist. */
    static const std::string album_artist = "album_artist";
    /** @brief Key: genre. */
    static const std::string genre = "genre";
    /** @brief Key: thumbnail id. */
    static const std::string thumbnail_id = "thumbnail_id";
    /** @brief Key: visual genre id. */
    static const std::string visual_genre_id = "visual_genre_id";
    /** @brief Key: genre id. */
    static const std::string genre_id = "genre_id";
    /** @brief Key: visual artist id. */
    static const std::string visual_artist_id = "visual_artist_id";
    /** @brief Key: artist id. */
    static const std::string artist_id = "artist_id";
    /** @brief Key: track number. */
    static const std::string track_num = "track";
    /** @brief Key: album artist id. */
    static const std::string album_artist_id = "album_artist_id";
    /** @brief Key: album id. */
    static const std::string album_id = "album_id";
    /** @brief Key: category. */
    static const std::string category = "category";
    /** @brief Key: category id. */
    static const std::string category_id = "category_id";
    /** @brief Key: filter. */
    static const std::string filter = "filter";
    /** @brief Key: id. */
    static const std::string id = "id";
    /** @brief Key: ids. */
    static const std::string ids = "ids";
    /** @brief Key: value. */
    static const std::string value = "value";
    /** @brief Key: data. */
    static const std::string data = "data";
    /** @brief Key: limit. */
    static const std::string limit = "limit";
    /** @brief Key: offset. */
    static const std::string offset = "offset";
    /** @brief Key: count only. */
    static const std::string count_only = "count_only";
    /** @brief Key: ids only. */
    static const std::string ids_only = "ids_only";
    /** @brief Key: count. */
    static const std::string count = "count";
    /** @brief Key: success. */
    static const std::string success = "success";
    /** @brief Key: index. */
    static const std::string index = "index";
    /** @brief Key: delta. */
    static const std::string delta = "delta";
    /** @brief Key: relative. */
    static const std::string relative = "relative";
    /** @brief Key: password. */
    static const std::string password = "password";
    /** @brief Key: raw query data. */
    static const std::string raw_query_data = "raw_query_data";
    /** @brief Key: authenticated. */
    static const std::string authenticated = "authenticated";
    /** @brief Key: environment. */
    static const std::string environment = "environment";
    /** @brief Key: playlist id. */
    static const std::string playlist_id = "playlist_id";
    /** @brief Key: playlist name. */
    static const std::string playlist_name = "playlist_name";
    /** @brief Key: subquery. */
    static const std::string subquery = "subquery";
    /** @brief Key: type. */
    static const std::string type = "type";
    /** @brief Key: sort order. */
    static const std::string sort_order = "sort_order";
    /** @brief Key: sort orders. */
    static const std::string sort_orders = "sort_orders";
    /** @brief Key: predicate category. */
    static const std::string predicate_category = "predicate_category";
    /** @brief Key: predicate id. */
    static const std::string predicate_id = "predicate_id";
    /** @brief Key: predicates. */
    static const std::string predicates = "predicates";
    /** @brief Key: sdk version. */
    static const std::string sdk_version = "sdk_version";
    /** @brief Key: api version. */
    static const std::string api_version = "api_version";
    /** @brief Key: app version. */
    static const std::string app_version = "app_version";
    /** @brief Key: driver name. */
    static const std::string driver_name = "driver_name";
    /** @brief Key: all. */
    static const std::string all = "all";
    /** @brief Key: selected. */
    static const std::string selected = "selected";
    /** @brief Key: devices. */
    static const std::string devices = "devices";
    /** @brief Key: device name. */
    static const std::string device_name = "device_name";
    /** @brief Key: device id. */
    static const std::string device_id = "device_id";
    /** @brief Key: replaygain mode. */
    static const std::string replaygain_mode = "replaygain_mode";
    /** @brief Key: preamp gain. */
    static const std::string preamp_gain = "preamp_gain";
    /** @brief Key: enabled. */
    static const std::string enabled = "enabled";
    /** @brief Key: bands. */
    static const std::string bands = "bands";
    /** @brief Key: time. */
    static const std::string time = "time";
}

/** @brief JSON value strings used by the protocol. */
namespace value {
    /** @brief Value: invalid. */
    static const std::string invalid = "invalid";
    /** @brief Value: unauthenticated. */
    static const std::string unauthenticated = "unauthenticated";
    /** @brief Value: up. */
    static const std::string up = "up";
    /** @brief Value: down. */
    static const std::string down = "down";
    /** @brief Value: delta. */
    static const std::string delta = "delta";
    /** @brief Value: reindex. */
    static const std::string reindex = "reindex";
    /** @brief Value: rebuild. */
    static const std::string rebuild = "rebuild";
    /** @brief Value: live. */
    static const std::string live = "live";
    /** @brief Value: snapshot. */
    static const std::string snapshot = "snapshot";
}

/** @brief JSON message type strings. */
namespace type {
    /** @brief Type: request. */
    static const std::string request = "request";
    /** @brief Type: response. */
    static const std::string response = "response";
    /** @brief Type: broadcast. */
    static const std::string broadcast = "broadcast";
}

/** @brief Names of the requests the WebSocket server accepts. */
namespace request {
    /** @brief Request: authenticate. */
    static const std::string authenticate = "authenticate";
    /** @brief Request: ping. */
    static const std::string ping = "ping";
    /** @brief Request: send_raw_query. */
    static const std::string send_raw_query = "send_raw_query";
    /** @brief Request: pause_or_resume. */
    static const std::string pause_or_resume = "pause_or_resume";
    /** @brief Request: stop. */
    static const std::string stop = "stop";
    /** @brief Request: previous. */
    static const std::string previous = "previous";
    /** @brief Request: next. */
    static const std::string next = "next";
    /** @brief Request: play_at_index. */
    static const std::string play_at_index = "play_at_index";
    /** @brief Request: toggle_shuffle. */
    static const std::string toggle_shuffle = "toggle_shuffle";
    /** @brief Request: toggle_repeat. */
    static const std::string toggle_repeat = "toggle_repeat";
    /** @brief Request: set_volume. */
    static const std::string set_volume = "set_volume";
    /** @brief Request: seek_to. */
    static const std::string seek_to = "seek_to";
    /** @brief Request: seek_relative. */
    static const std::string seek_relative = "seek_relative";
    /** @brief Request: toggle_mute. */
    static const std::string toggle_mute = "toggle_mute";
    /** @brief Request: get_playback_overview. */
    static const std::string get_playback_overview = "get_playback_overview";
    /** @brief Request: get_current_time. */
    static const std::string get_current_time = "get_current_time";
    /** @brief Request: list_categories. */
    static const std::string list_categories = "list_categories";
    /** @brief Request: query_category. */
    static const std::string query_category = "query_category";
    /** @brief Request: query_tracks. */
    static const std::string query_tracks = "query_tracks";
    /** @brief Request: query_tracks_by_external_ids. */
    static const std::string query_tracks_by_external_ids = "query_tracks_by_external_ids";
    /** @brief Request: query_albums. */
    static const std::string query_albums = "query_albums";
    /** @brief Request: query_tracks_by_category. */
    static const std::string query_tracks_by_category = "query_tracks_by_category";
    /** @brief Request: play_all_tracks. */
    static const std::string play_all_tracks = "play_all_tracks";
    /** @brief Request: play_snapshot_tracks. */
    static const std::string play_snapshot_tracks = "play_snapshot_tracks";
    /** @brief Request: play_tracks. */
    static const std::string play_tracks = "play_tracks";
    /** @brief Request: play_tracks_by_category. */
    static const std::string play_tracks_by_category = "play_tracks_by_category";
    /** @brief Request: query_play_queue_tracks. */
    static const std::string query_play_queue_tracks = "query_play_queue_tracks";
    /** @brief Request: get_environment. */
    static const std::string get_environment = "get_environment";
    /** @brief Request: save_playlist. */
    static const std::string save_playlist = "save_playlist";
    /** @brief Request: rename_playlist. */
    static const std::string rename_playlist = "rename_playlist";
    /** @brief Request: delete_playlist. */
    static const std::string delete_playlist = "delete_playlist";
    /** @brief Request: append_to_playlist. */
    static const std::string append_to_playlist = "append_to_playlist";
    /** @brief Request: remove_tracks_from_playlist. */
    static const std::string remove_tracks_from_playlist = "remove_tracks_from_playlist";
    /** @brief Request: run_indexer. */
    static const std::string run_indexer = "run_indexer";
    /** @brief Request: list_output_drivers. */
    static const std::string list_output_drivers = "list_output_drivers";
    /** @brief Request: set_default_output_driver. */
    static const std::string set_default_output_driver = "set_default_output_driver";
    /** @brief Request: get_gain_settings. */
    static const std::string get_gain_settings = "get_gain_settings";
    /** @brief Request: set_gain_settings. */
    static const std::string set_gain_settings = "set_gain_settings";
    /** @brief Request: get_equalizer_settings. */
    static const std::string get_equalizer_settings = "get_equalizer_settings";
    /** @brief Request: set_equalizer_settings. */
    static const std::string set_equalizer_settings = "set_equalizer_settings";
    /** @brief Request: get_transport_type. */
    static const std::string get_transport_type = "get_transport_type";
    /** @brief Request: set_transport_type. */
    static const std::string set_transport_type = "set_transport_type";
    /** @brief Request: snapshot_play_queue. */
    static const std::string snapshot_play_queue = "snapshot_play_queue";
    /** @brief Request: invalidate_play_queue_snapshot. */
    static const std::string invalidate_play_queue_snapshot = "invalidate_play_queue_snapshot";
}

/** @brief URI fragment names used by the HTTP server. */
namespace fragment {
    /** @brief Fragment: audio. */
    static const std::string audio = "audio";
    /** @brief Fragment: id. */
    static const std::string id = "id";
    /** @brief Fragment: external id. */
    static const std::string external_id = "external_id";
    /** @brief Fragment: thumbnail. */
    static const std::string thumbnail = "thumbnail";
}

/** @brief Broadcast event names sent to connected clients. */
namespace broadcast {
    /** @brief Broadcast: playback overview changed. */
    static const std::string playback_overview_changed = "playback_overview_changed";
    /** @brief Broadcast: play queue changed. */
    static const std::string play_queue_changed = "play_queue_changed";
}

/** @brief Maps playback states to their JSON string representation. */
static auto PLAYBACK_STATE_TO_STRING = std::unordered_map<musik::core::sdk::PlaybackState, std::string>({
    { musik::core::sdk::PlaybackState::Stopped, "stopped" },
    { musik::core::sdk::PlaybackState::Playing, "playing" },
    { musik::core::sdk::PlaybackState::Prepared, "prepared" },
    { musik::core::sdk::PlaybackState::Paused, "paused" }
});

/** @brief Maps repeat modes to their JSON string representation. */
static auto REPEAT_MODE_TO_STRING = std::unordered_map<musik::core::sdk::RepeatMode, std::string>({
    { musik::core::sdk::RepeatMode::None, "none" },
    { musik::core::sdk::RepeatMode::Track, "track" },
    { musik::core::sdk::RepeatMode::List, "list" }
});

/** @brief Maps replay-gain modes to their JSON string representation. */
static auto REPLAYGAIN_MODE_TO_STRING = std::unordered_map<musik::core::sdk::ReplayGainMode, std::string>({
    { musik::core::sdk::ReplayGainMode::Disabled, "disabled" },
    { musik::core::sdk::ReplayGainMode::Album, "album" },
    { musik::core::sdk::ReplayGainMode::Track, "track" },
});

/** @brief Maps transport types to their JSON string representation. */
static auto TRANSPORT_TYPE_TO_STRING = std::unordered_map<musik::core::sdk::TransportType, std::string>({
    { musik::core::sdk::TransportType::Gapless, "gapless" },
    { musik::core::sdk::TransportType::Crossfade, "crossfade" },
});

/** @brief Version of the server remote API. */
static const int ApiVersion = 20;
