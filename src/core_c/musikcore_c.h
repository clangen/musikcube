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

#ifndef __C_MUSIKCORE_SDK_C_H__
#define __C_MUSIKCORE_SDK_C_H__

#include <stdlib.h>
#include <stdint.h>
#include <string.h>

/*
 *
 * version
 *
 */

static const int mcsdk_version = 18;

/*
 *
 * constants
 *
 */

enum mcsdk_playback_state {
    mcsdk_playback_stopped = 1,
    mcsdk_playback_paused = 2,
    mcsdk_playback_prepared = 3,
    mcsdk_playback_playing = 4,
};

enum mcsdk_stream_event {
    mcsdk_stream_scheduled = 1,
    mcsdk_stream_prepared = 2,
    mcsdk_stream_playing = 3,
    mcsdk_stream_almost_done = 4,
    mcsdk_stream_finished = 5,
    mcsdk_stream_stopped = 6,
    mcsdk_stream_error = -1
};

enum mcsdk_repeat_mode {
    mcsdk_repeat_none = 0,
    mcsdk_repeat_track = 1,
    mcsdk_repeat_list = 2
};

enum mcsdk_output_code {
    mcsdk_output_error_invalid_format = -4,
    mcsdk_output_error_invalid_state = -3,
    mcsdk_output_error_buffer_full = -2,
    mcsdk_output_error_buffer_written = -1
};

enum mcsdk_time_change_mode {
    mcsdk_time_change_mode_seek = 0,
    mcsdk_time_change_mode_scrub = 1
};

enum mcsdk_path_type {
    mcsdk_path_user_home = 0,
    mcsdk_path_data = 1,
    mcsdk_path_application = 2,
    mcsdk_path_plugins = 3,
    mcsdk_path_library = 4
};

enum mcsdk_stream_capability {
    mcsdk_stream_capability_prebuffer = 0x01
};

enum mcsdk_indexer_scan_result {
    mcsdk_indexer_scan_result_commit = 1,
    mcsdk_indexer_scan_result_rollback = 2
};

enum class mcsdk_replay_gain_mode {
    mcsdk_replay_gain_mode_disabled = 0,
    mcsdk_replay_gain_mode_track = 1,
    mcsdk_replay_gain_mode_album = 2
};

enum class mcsdk_transport_type {
    mcsdk_transport_type_gapless = 0,
    mcsdk_transport_type_crossfade = 1
};

enum mcsdk_stream_open_flags {
    mcsdk_stream_open_flags_none = 0,
    mcsdk_stream_open_flags_read = 1,
    mcsdk_stream_open_flags_write = 2
};

static const size_t mcsdk_equalizer_band_count = 18;

static const size_t mcsdk_equalizer_bands[] = {
    65, 92, 131, 185, 262, 370, 523, 740, 1047, 1480,
    2093, 2960, 4186, 5920, 8372, 11840, 16744, 22000
};

static const char* mcsdk_category_type_album = "album";
static const char* mcsdk_category_type_artist = "artist";
static const char* mcsdk_category_type_album_artist = "album_artist";
static const char* mcsdk_category_type_genre = "genre";
static const char* mcsdk_category_type_playlist = "playlists";

static const char* mcsdk_track_field_id = "id";
static const char* mcsdk_track_field_track_num = "track";
static const char* mcsdk_track_field_disc_num = "disc";
static const char* mcsdk_track_field_bpm = "bpm";
static const char* mcsdk_track_field_duration = "duration";
static const char* mcsdk_track_field_filesize = "filesize";
static const char* mcsdk_track_field_year = "year";
static const char* mcsdk_track_field_title = "title";
static const char* mcsdk_track_field_filename = "filename";
static const char* mcsdk_track_field_thumbnail_id = "thumbnail_id";
static const char* mcsdk_track_field_album = "album";
static const char* mcsdk_track_field_album_artist = "album_artist";
static const char* mcsdk_track_field_genre = "genre";
static const char* mcsdk_track_field_artist = "artist";
static const char* mcsdk_track_field_filetime = "filetime";
static const char* mcsdk_track_field_genre_id = "visual_genre_id";
static const char* mcsdk_track_field_artist_id = "visual_artist_id";
static const char* mcsdk_track_field_album_artist_id = "album_artist_id";
static const char* mcsdk_track_field_album_id = "album_id";
static const char* mcsdk_track_field_source_id = "source_id";
static const char* mcsdk_track_field_external_id = "external_id";

/*
 *
 * types
 *
 */

typedef void* mcsdk_handle;
typedef mcsdk_handle mcsdk_resource;
typedef mcsdk_handle mcsdk_value;
typedef mcsdk_handle mcsdk_map;
typedef mcsdk_handle mcsdk_track;

/*
 *
 * IResource
 *
 */

extern int64_t mcsdk_resource_get_id(mcsdk_resource r);
extern int mcsdk_resource_get_class(mcsdk_resource r );
extern const char* mcsdk_resource_get_type(mcsdk_resource r);
extern const char* mcsdk_resource_release(mcsdk_resource r);

/*
 *
 * IValue
 *
 */

extern size_t mcsdk_value_get_value(mcsdk_value v, char* dst, size_t size);

/*
 *
 * IMap
 *
 */

extern int mcsdk_map_get_string(mcsdk_map m, const char* key, char* dst, int size);
extern int64_t mcsdk_map_get_int64(mcsdk_map m, const char* key, int64_t default_value = 0);
extern int32_t mcsdk_map_get_int32(mcsdk_map m, const char* key, int32_t default_value = 0);
extern double mcsdk_map_get_double(mcsdk_map m, const char* key, double default_value = 0.0f);

/*
 *
 * ITrack
 *
 */

extern void mcsdk_track_retain(mcsdk_track t);
extern int mcsdk_track_get_uri(mcsdk_track t, char* dst, int size);

#endif