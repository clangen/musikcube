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
#include <stdbool.h>
#include <string.h>

#ifdef MCSDK_DEFINE_EXPORTS
    #ifdef WIN32
        #define mcsdk_export extern "C" __declspec(dllexport)
    #else
        #define mcsdk_export extern "C"
    #endif
#else
    #define mcsdk_export extern
#endif

/**
 * @brief musikcore API version.
 *
 */
static const int mcsdk_version = 18;

/**
 * @brief The current playback state. Used by the playback service (mcsdk_svc_playback).
 *
 */
typedef enum mcsdk_playback_state {
    mcsdk_playback_stopped = 1,
    mcsdk_playback_paused = 2,
    mcsdk_playback_prepared = 3,
    mcsdk_playback_playing = 4,
} mcsdk_playback_state;

/**
 * @brief The current audio stream state. Used by audio streams (mcsdk_audio_stream)
 *
 */
typedef enum mcsdk_audio_stream_event {
    mcsdk_audio_stream_event_scheduled = 1,
    mcsdk_audio_stream_event_prepared = 2,
    mcsdk_audio_stream_event_playing = 3,
    mcsdk_audio_stream_event_almost_done = 4,
    mcsdk_audio_stream_event_finished = 5,
    mcsdk_audio_stream_event_stopped = 6,
    mcsdk_audio_stream_event_error = -1
} mcsdk_stream_event;

/**
 * @brief The current playback repeat mode. Used by the playback service (mcsdk_svc_playback)
 *
 */
typedef enum mcsdk_playback_repeat_mode {
    mcsdk_playback_repeat_mode_none = 0,
    mcsdk_playback_repeat_mode_track = 1,
    mcsdk_playback_repeat_mode_list = 2
} mcsdk_playback_repeat_mode;

/**
 * @brief Returned by an output device (mcsdk_audio_output) when writing an audio buffer
 * to the driver.
 *
 */
typedef enum mcsdk_audio_output_code {
    mcsdk_audio_output_error_invalid_format = -4,
    mcsdk_audio_output_error_invalid_state = -3,
    mcsdk_audio_output_error_buffer_full = -2,
    mcsdk_audio_output_error_buffer_written = -1
} mcsdk_audio_output_code;

/**
 * @brief Used by the playback service (mcsdk_svc_playback) to configure the seek mode.
 *
 */
typedef enum mcsdk_playback_time_change_mode {
    mcsdk_playback_time_change_mode_seek = 0,
    mcsdk_playback_time_change_mode_scrub = 1
} mcsdk_playback_time_change_mode;

/**
 * @brief Used by the environment to represent well-known paths used by various
 * subsystems in the application.
 *
 */
typedef enum mcsdk_env_path_type {
    mcsdk_env_path_type_user_home = 0,
    mcsdk_env_path_type_data = 1,
    mcsdk_env_path_type_application = 2,
    mcsdk_env_path_type_plugins = 3,
    mcsdk_env_path_type_library = 4
} mcsdk_env_path_type;

/**
 * @brief A set of capabilities that can be configured on audio streams.
 *
 */
typedef enum mcsdk_audio_stream_capability {
    mcsdk_audio_stream_capability_prebuffer = 0x01
} mcsdk_audio_stream_capability;

/**
 * @brief Used by the metadata indexer service (mcsdk_svc_metadata) plugins to
 * communicate successful or unsuccessful parsing results.
 *
 */
typedef enum mcsdk_svc_indexer_scan_result {
    mcsdk_svc_indexer_scan_result_commit = 1,
    mcsdk_svc_indexer_scan_result_rollback = 2
} mcsdk_svc_indexer_scan_result;

/**
 * @brief Used by the metadata indexer service (mcsdk_svc_metadata) to communicate
 * state changes.
 *
 */
typedef enum mcsdk_svc_indexer_state {
    mcsdk_svc_indexer_state_idle = 0,
    mcsdk_svc_indexer_state_indexing = 1,
    mcsdk_svc_indexer_state_stopping = 2,
    mcsdk_svc_indexer_state_stopped = 3
} mcsdk_svc_indexer_state;

/**
 * @brief Used by callers to configure the level of granularity with which
 * the metadata indexer service (mcsdk_svc_metadata) should scan for updates.
 *
 */
typedef enum mcsdk_svc_indexer_sync_type {
    mcsdk_svc_indexer_sync_type_all = 0,
    mcsdk_svc_indexer_sync_type_local = 1,
    mcsdk_svc_indexer_sync_type_rebuild = 2,
    mcsdk_svc_indexer_sync_type_sources = 3
} mcsdk_svc_indexer_sync_type;

/**
 * @brief Used by callers to configure how a low-level metadata library (mcsdk_svc_library)
 * query should run.
 */
typedef enum mcsdk_svc_library_query_flag {
    mcsdk_svc_library_query_flag_none = 0,
    mcsdk_svc_library_query_flag_synchronous = 1
} mcsdk_svc_library_query_flag;

/**
 * @brief Used by callers to configure how the playback service (mcsdk_svc_playback) will
 * apply ReplayGain during playback.
 *
 */
typedef enum mcsdk_playback_replay_gain_mode {
    mcsdk_playback_replay_gain_mode_disabled = 0,
    mcsdk_playback_replay_gain_mode_track = 1,
    mcsdk_playback_replay_gain_mode_album = 2
} mcsdk_playback_replay_gain_mode;

/**
 * @brief Used by callers to configure which type of underlying playback transport
 * will be used by the playback service (mcsdk_svc_playback)
 *
 */
typedef enum mcsdk_playback_transport_type {
    mcsdk_playback_transport_type_gapless = 0,
    mcsdk_playback_transport_type_crossfade = 1
} mcsdk_playback_transport_type;

/**
 * @brief Used by callers when opening data streams (mcsdk_data_stream) to represent
 * in what mode the underlying resource should be used.
 *
 */
typedef enum mcsdk_data_stream_open_flags {
    mcsdk_data_stream_open_flags_none = 0,
    mcsdk_data_stream_open_flags_read = 1,
    mcsdk_data_stream_open_flags_write = 2
} mcsdk_data_stream_open_flags;

/**
 * @brief Used to configure properties on audio streams (mcsdk_audio_stream) instances.
 *
 */
typedef enum mcsdk_audio_stream_flags {
    mcsdk_audio_stream_flags_none = 0,
    mcsdk_audio_stream_flags_no_dsp = 1
} mcsdk_audio_stream_flags;

/**
 * @brief Some resources in the SDK share a set of common properties, and can be passed
 * around generically. Each resource is tagged with a type identifier that can be queried at,
 * runtime so it can casted then used as a more specific type.
 *
 * We should probably try to get rid of this because it's gross.
 *
 */
typedef enum mcsdk_resource_class {
    mcsdk_resource_type_value = 0,
    mcsdk_resource_type_map = 1
} mcsdk_resource_class;

/**
 * @brief Encoders come in two flavors: blocking, and streaming. This distinction exists
 * because we prefer streaming encoders, but not all underlying libraries support this
 * use case.
 *
 */
typedef enum mcsdk_encoder_type {
    mcsdk_encoder_type_none = 0,
    /**
     * @brief A blocking encoder writes directly to an output data stream (mcsdk_data_stream),
     * and cannot return partial information, likely because it requires the ability to
     * seek around the output stream and fill in missing information later. This is currently
     * the most common type of encoder.
     *
     */
    mcsdk_encoder_type_blocking = 1,
    /**
     * @brief A streaming encoder is one that can be used for streaming audio before it's
     * finished encoding. This is the preferred mode for the server, as it can return audio
     * data to clients immediately without waiting for the entire track to be processed.
     *
     * A streaming encoder returns byte buffers, and the caller is responsible for all file
     * output, if required.
     *
     * A streaming encoder can always be adapted to behave like a blocking encoder.
     *
     */
    mcsdk_encoder_type_streaming = 2
} mcsdk_encoder_type;

/**
 * @brief Used to specify how buffered / pending audio samples should be treated when
 * closing down an audio player (mcsdk_audio_player).
 *
 */
typedef enum mcsdk_audio_player_release_mode {
    /**
     * @brief Wait for pending samples to be written to the output device before releasing
     * resources.
     *
     */
    mcsdk_audio_player_release_mode_drain = 0,
    /**
     * @brief Stop the stream immediately and discard pending audio data.
     *
     */
    mcsdk_audio_player_release_mode_no_drain = 1
} mcsdk_audio_player_release_mode;

/**
 * @brief Represents the result of a low-level metadata library (mcsdk_svc_library) query.
 *
 */
typedef enum mcsdk_db_result {
    mcsdk_db_result_okay = 0,
    mcsdk_db_result_row = 100,
    mcsdk_db_result_done = 101,
} mcsdk_db_result;

/**
 * @brief The number of equalizer bands.
 *
 */
static const size_t mcsdk_equalizer_band_count = 18;

/**
 * @brief The frequency ranges for the equalizer bands.
 *
 */
static const size_t mcsdk_equalizer_bands[] = {
    65, 92, 131, 185, 262, 370, 523, 740, 1047, 1480,
    2093, 2960, 4186, 5920, 8372, 11840, 16744, 22000
};

/**
 * @brief Used when querying the metadata service (mcsdk_svc_metadata) to specify
 * and offset in the resulting data start to begin reading at.
 *
 */
static const int mcsdk_no_offset = 0;

/**
 * @brief Used when querying the metadata service (mcsdk_svc_metadata) to limit
 * the number of resources returned.
 *
 */
static const int mcsdk_no_limit = -1;

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
 * types
 */

#define mcsdk_define_handle(x) \
    typedef struct x { \
        void* opaque; \
    } x;

/**
 * @brief Returns `true` if the underlying resource appears to be valid, `false` otherwise.
 *
 */
#define mcsdk_handle_ok(x) x.opaque != NULL

/**
 * @brief Used to "cast" a resource from one type to another.
 *
 */
#define mcsdk_cast_handle(x) { x.opaque }

/**
 * @brief Returns `true` if the two input handles reference the same underlying resource.
 *
 */
#define mcsdk_handle_equals(x, y) x.opaque == y.opaque

/**
 * @brief An opaque resource identifier. If you have one of these you can use the
 * `mcsdk_resource_get_class` function to resolve the type.
 *
 */
mcsdk_define_handle(mcsdk_resource);

/**
 * @brief A `value` resource. A value simply a byte buffer; it's generally a string,
 * but may be any type of data.
 *
 */
mcsdk_define_handle(mcsdk_value);

/**
 * @brief A `map` resource. Maps are just key/value stores. The key is always a string,
 * and the value for a key may be an int32, int64, double or string.
 *
 */
mcsdk_define_handle(mcsdk_map);

/**
 * @brief A list of `value` resources.
 *
 */
mcsdk_define_handle(mcsdk_value_list);

/**
 * @brief A list of `map` resources.
 *
 */
mcsdk_define_handle(mcsdk_map_list);

/**
 * @brief A track is a special type of map resource (mcsdk_map) that is represented by
 * a URI and is reference counted.
 *
 */
mcsdk_define_handle(mcsdk_track);

/**
 * @brief A list of track resources (mcsdk_track). Note this is *not* a specialization of
 * the more generic list resource.
 *
 */
mcsdk_define_handle(mcsdk_track_list);

/**
 * @brief A resource type that can be used to edit track lists (mcsdk_track_list). In the
 * future it will likely be merged with mcsdk_track_list, but currently exists as a
 * separate type for legacy reasons.
 *
 */
mcsdk_define_handle(mcsdk_track_list_editor);

/**
 * @brief A high-level service used for querying metadata. It generally vends tracks,
 * lists of tracks, or maps of different resource types.
 *
 */
mcsdk_define_handle(mcsdk_svc_metadata);

/**
 * @brief A high-level service used for audio playback. Manages an internal queue and
 * takes care of all low-level playback details including output management, gapless
 * and crossfading playback, replay gain, and more.
 *
 */
mcsdk_define_handle(mcsdk_svc_playback);

/**
 * @brief The indexer service is used to populate musikcore's internal database using
 * a list of paths and plugins to retrieve audio metadata.
 *
 */
mcsdk_define_handle(mcsdk_svc_indexer);

/**
 * @brief A low-level service that can be used to query the internal database using
 * SQL. It can be used to run queries and retrieve data asynchronously.
 *
 */
mcsdk_define_handle(mcsdk_svc_library);

/**
 * @brief A type that looks similar to a mutable map (mcsdk_map), but is also able
 * to serialize and deserialize itself.
 *
 */
mcsdk_define_handle(mcsdk_prefs);

/**
 * @brief A raw audio buffer; they are always interleaved, 32-bit floating point samples.
 *
 */
mcsdk_define_handle(mcsdk_audio_buffer);

/**
 * @brief Audio buffers (mcsdk_audio_buffer) are provided to the output (mcsdk_audio_output),
 * and may be cached for further use. When the output has finished with the buffer, it calls
 * the buffer provider (mcsdk_audio_buffer_provider) to let it know the buffer can be freed
 * or recycled for later use.
 *
 */
mcsdk_define_handle(mcsdk_audio_buffer_provider);

/**
 * @brief An abstract stream of data, generally backed by a file. It may be other things like
 * a memory buffer, an HTTP request, or anything that can read and/or write bytes.
 *
 */
mcsdk_define_handle(mcsdk_data_stream);

/**
 * @brief Represents an audio device, usually a physical sound card, but may be a virtual device.
 *
 */
mcsdk_define_handle(mcsdk_audio_device);

/**
 * @brief A list of audio devices (mcsdk_audio_device). Every audio output (mcsdk_output) is
 * able to provide the caller with a list of audio devices it supports.
 *
 */
mcsdk_define_handle(mcsdk_audio_device_list);

/**
 * @brief An audio output is a type that is able to take raw audio buffers (mcsdk_audio_buffer)
 * and send them to an audio device (mcsdk_audio_device).
 *
 */
mcsdk_define_handle(mcsdk_audio_output);

/**
 * @brief A Decoder reads raw data from a data stream (mcsdk_data_stream) and produces uncompressed
 * audio buffers (mcsdk_audio_buffer) that can be used for playback by an output (mcsdk_audio_output)
 *
 */
mcsdk_define_handle(mcsdk_decoder);

/**
 * @brief An encoder accepts uncompressed audio buffers (mcsdk_audio_buffer) and encodes and/or
 * compresses them to the specified target format.
 *
 */
mcsdk_define_handle(mcsdk_encoder);

/**
 * @brief A blocking encoder writes directly to an output data stream (mcsdk_data_stream). The
 * caller must feed all data into the encoder, one audio buffer (mcsdk_audio_buffer) at a time
 * until it reaches end of file. The contents of the data stream may not be used until the
 * encoding process has finished.
 *
 */
mcsdk_define_handle(mcsdk_blocking_encoder);

/**
 * @brief A streaming encoder accepts raw audio buffers (mcsdk_audio_buffer), and returns byte
 * arrays directly to the caller. As such, streaming encoders can be used to stream audio data
 * to clients in real time, without needing to wait for completion. Streaming encoders are
 * generally preferred, but not always possible to implement for some formats.
 *
 */
mcsdk_define_handle(mcsdk_streaming_encoder);

/**
 * @brief An audio stream is a type that orchestrates reading data from a stream, decoding it
 * via a decoder (mcsdk_audio_decoder) and applying any required DSP transformations. It's used
 * internally by audio player instances (mcsdk_audio_player).
 *
 */
mcsdk_define_handle(mcsdk_audio_stream);

/**
 * @brief An audio player accepts a data URI and an output (mcsdk_audio_output), and constructs
 * the relevant audio stream (mcsdk_audio_stream). Internally, it manages a thread nad feeds
 * the output device until the audio stream has reached end of file. It deals with buffering,
 * retries and error handling.
 *
 */
mcsdk_define_handle(mcsdk_audio_player);

/**
 * @brief A low-level database connection. This can be queried directly for audio data using
 * SQL. It's generally backed by a `sqlite` database. In general using the metadata service
 * (mcsdk_svc_metadata) is preferred. If that's not possible, it's probably a good idea to
 * run queries through the library service (mcsdk_svc_library) instead, as it will ensure
 * they are run on a background thread. Users should only use mcsdk_db_connection as a last
 * resort.
 *
 */
mcsdk_define_handle(mcsdk_db_connection);

/**
 * @brief Used to construct a statement that can be used to query a database (mcsdk_db_connection).
 * More or less a wrapper around prepared statements.
 *
 */
mcsdk_define_handle(mcsdk_db_statement);

/**
 * @brief A database transaction to be used with a database connection (mcsdk_db_connection).
 *
 */
mcsdk_define_handle(mcsdk_db_transaction);

/**
 * @brief A callback set used by mcsdk_audio_player to communicate state changes.
 *
 */
typedef struct mcsdk_audio_player_callbacks {
    void (*on_prepared)(mcsdk_audio_player p);
    void (*on_started)(mcsdk_audio_player p);
    void (*on_almost_ended)(mcsdk_audio_player p);
    void (*on_finished)(mcsdk_audio_player p);
    void (*on_error)(mcsdk_audio_player p);
    void (*on_destroying)(mcsdk_audio_player p);
    void (*on_mixpoint)(mcsdk_audio_player p, int id, double time);
} mcsdk_audio_player_callbacks;

/**
 * @brief A callback set used by mcsdk_svc_indexer to communicate state changes and metadata
 * scan progress.
 *
 */
typedef struct mcsdk_svc_indexer_callbacks {
    void (*on_started)(mcsdk_svc_indexer i);
    void (*on_finished)(mcsdk_svc_indexer i, int tracks_processed);
    void (*on_progress)(mcsdk_svc_indexer i, int tracks_processed);
} mcsdk_svc_indexer_callbacks;


/**
 * @brief Represents the replay gain settings that will be applied by an audio player
 * (mcsdk_audio_player) to a track.
 *
 */
typedef struct mcsdk_audio_player_gain {
    float preamp;
    float gain;
    float peak;
    float peakValid;
} mcsdk_audio_player_gain;

/**
 * @brief Callback used to run a query from the metadata library (mcsdk_svc_library).
 *
 */
typedef bool (*mcsdk_svc_library_run_query_callback)(mcsdk_svc_library l, mcsdk_db_connection db, void* user_context);

/*
 * global setup
 */

/**
 * @brief Initialize the environment. This ensures the small set of required globals
 * are configured properly.
 *
 * @return
 */
mcsdk_export void mcsdk_env_init();

/**
 * @brief Reset the global environment to its default state.
 *
 * @return
 */
mcsdk_export void mcsdk_env_release();

/*
 * instance context
 */

/**
 * @brief A structure that contains everything necessary to use musikcore, from metadata
 * indexing to play queue management and everything in between.
 *
 */
typedef struct mcsdk_context {
    mcsdk_svc_metadata metadata;
    mcsdk_svc_playback playback;
    mcsdk_svc_indexer indexer;
    mcsdk_svc_library library;
    mcsdk_db_connection db;
    mcsdk_prefs preferences;
    mcsdk_internal internal;
} mcsdk_context;

/**
 * @brief Allocate and initialize a new musikcore context (mcsdk_context). While it's
 * possible for multiple contexts to exist at the same time, external plugins will
 * only be aware of the first one created, or the last one set via
 * mcsdk_set_plugin_context().
 *
 * @param context
 * @return void
 */
mcsdk_export void mcsdk_context_init(mcsdk_context** context);

/**
 * @brief Deinitializes all resources associated with the specified context, then
 * deallocates the structure itself.
 *
 * @param context
 * @return void
 */
mcsdk_export void mcsdk_context_release(mcsdk_context** context);

/**
 * @brief Reinitializes all external plugins with the specified plugin context.
 * Passing a NULL context is allowed.
 *
 * @param context
 * @return
 */
mcsdk_export void mcsdk_set_plugin_context(mcsdk_context* context);

/**
 * @brief Check to see if the supplied context is the active plugin context.
 *
 * @param context
 * @return true if it is, false otherwise.
 */
mcsdk_export bool mcsdk_is_plugin_context(mcsdk_context* context);

/*
 * IResource
 */

mcsdk_export int64_t mcsdk_resource_get_id(mcsdk_resource r);
mcsdk_export mcsdk_resource_class mcsdk_resource_get_class(mcsdk_resource r);
mcsdk_export void mcsdk_resource_release(mcsdk_resource r);

/*
 * IValue
 */

mcsdk_export size_t mcsdk_value_get_value(mcsdk_value v, char* dst, size_t size);
mcsdk_export void mcsdk_value_release(mcsdk_value v);

/*
 * IValueList
 */

mcsdk_export size_t mcsdk_value_list_count(mcsdk_value_list vl);
mcsdk_export mcsdk_value mcsdk_value_list_get_at(mcsdk_value_list vl, size_t index);
mcsdk_export void mcsdk_value_list_release(mcsdk_value_list vl);

/*
 * IMap
 */

mcsdk_export int mcsdk_map_get_string(mcsdk_map m, const char* key, char* dst, int size);
mcsdk_export int64_t mcsdk_map_get_int64(mcsdk_map m, const char* key, int64_t default_value);
mcsdk_export int32_t mcsdk_map_get_int32(mcsdk_map m, const char* key, int32_t default_value);
mcsdk_export double mcsdk_map_get_double(mcsdk_map m, const char* key, double default_value);
mcsdk_export void mcsdk_map_release(mcsdk_map m);

/*
 * IMapList
 */

mcsdk_export size_t mcsdk_map_list_get_count(mcsdk_map_list ml);
mcsdk_export mcsdk_map mcsdk_map_list_get_at(mcsdk_map_list ml, size_t index);
mcsdk_export void mcsdk_map_list_release(mcsdk_map_list ml);

/*
 * ITrack
 */

mcsdk_export void mcsdk_track_retain(mcsdk_track t);
mcsdk_export int mcsdk_track_get_uri(mcsdk_track t, char* dst, int size);
mcsdk_export void mcsdk_track_release(mcsdk_track t);

/*
 * ITrackList
 */

mcsdk_export size_t mcsdk_track_list_get_count(mcsdk_track_list tl);
mcsdk_export int64_t mcsdk_track_list_get_id(mcsdk_track_list tl, size_t index);
mcsdk_export int64_t mcsdk_track_list_index_of(mcsdk_track_list tl, int64_t id);
mcsdk_export mcsdk_track mcsdk_track_list_get_track_at(mcsdk_track_list tl, size_t index);
mcsdk_export void mcsdk_track_list_release(mcsdk_track_list tl);

/*
 * TrackList
 */

mcsdk_export mcsdk_track_list mcsdk_track_list_create(mcsdk_context* context);
mcsdk_export bool mcsdk_track_list_can_edit(mcsdk_track_list tl);
mcsdk_export mcsdk_track_list_editor mcsdk_track_list_edit(mcsdk_track_list tl);

/*
 * ITrackListEditor
 */

mcsdk_export bool mcsdk_track_list_editor_insert(mcsdk_track_list_editor tle, int64_t id, size_t index);
mcsdk_export bool mcsdk_track_list_editor_swap(mcsdk_track_list_editor tle, size_t index1, size_t index2);
mcsdk_export bool mcsdk_track_list_editor_move(mcsdk_track_list_editor tle, size_t from, size_t to);
mcsdk_export bool mcsdk_track_list_editor_delete(mcsdk_track_list_editor tle, size_t index);
mcsdk_export void mcsdk_track_list_editor_add(mcsdk_track_list_editor tle, const int64_t id);
mcsdk_export void mcsdk_track_list_editor_clear(mcsdk_track_list_editor tle);
mcsdk_export void mcsdk_track_list_editor_shuffle(mcsdk_track_list_editor tle);
mcsdk_export void mcsdk_track_list_editor_release(mcsdk_track_list_editor tle);

/*
 * IMetadataProxy
 */

mcsdk_export mcsdk_track_list mcsdk_svc_metadata_query_tracks(mcsdk_svc_metadata mp, const char* keyword, int limit, int offset);
mcsdk_export mcsdk_track mcsdk_svc_metadata_query_track_by_id(mcsdk_svc_metadata mp, int64_t track_id);
mcsdk_export mcsdk_track mcsdk_svc_metadata_query_track_by_external_id(mcsdk_svc_metadata mp, const char* external_id);
mcsdk_export mcsdk_track_list mcsdk_svc_metadata_query_tracks_by_category(mcsdk_svc_metadata mp, const char* category_type, int64_t selected_id, const char* filter, int limit, int offset);
mcsdk_export mcsdk_track_list mcsdk_svc_metadata_query_tracks_by_categories(mcsdk_svc_metadata mp, mcsdk_value* categories, size_t category_count, const char* filter, int limit, int offset);
mcsdk_export mcsdk_track_list mcsdk_svc_metadata_query_tracks_by_external_id(mcsdk_svc_metadata mp, const char** external_ids, size_t external_id_count);
mcsdk_export mcsdk_value_list mcsdk_svc_metadata_list_categories(mcsdk_svc_metadata mp);
mcsdk_export mcsdk_value_list mcsdk_svc_metadata_query_category(mcsdk_svc_metadata mp, const char* type, const char* filter);
mcsdk_export mcsdk_value_list mcsdk_svc_metadata_query_category_with_predicate(mcsdk_svc_metadata mp, const char* type, const char* predicate_type, int64_t predicate_id, const char* filter);
mcsdk_export mcsdk_value_list mcsdk_svc_metadata_query_category_with_predicates(mcsdk_svc_metadata mp, const char* type, mcsdk_value* predicates, size_t predicate_count, const char* filter);
mcsdk_export mcsdk_map_list mcsdk_svc_metadata_query_albums(mcsdk_svc_metadata mp, const char* filter);
mcsdk_export mcsdk_map_list mcsdk_svc_metadata_query_albums_by_category(mcsdk_svc_metadata mp, const char* category_id_name, int64_t category_id_value, const char* filter);
mcsdk_export int64_t mcsdk_svc_metadata_save_playlist_with_ids(mcsdk_svc_metadata mp, int64_t* track_ids, size_t track_id_count, const char* playlist_name, const int64_t playlist_id);
mcsdk_export int64_t mcsdk_svc_metadata_save_playlist_with_external_ids(mcsdk_svc_metadata mp, const char** external_ids, size_t external_id_count, const char* playlist_name, const int64_t playlist_id);
mcsdk_export int64_t mcsdk_svc_metadata_save_playlist_with_track_list(mcsdk_svc_metadata mp, mcsdk_track_list track_list, const char* playlist_name, const int64_t playlist_id);
mcsdk_export bool mcsdk_svc_metadata_rename_playlist(mcsdk_svc_metadata mp, const int64_t playlist_id, const char* playlist_name);
mcsdk_export bool mcsdk_svc_metadata_delete_playlist(mcsdk_svc_metadata mp, const int64_t playlist_id);
mcsdk_export bool mcsdk_svc_metadata_append_to_playlist_with_ids(mcsdk_svc_metadata mp, const int64_t playlist_id, const int64_t* track_ids, size_t track_id_count, int offset);
mcsdk_export bool mcsdk_svc_metadata_append_to_playlist_with_external_ids(mcsdk_svc_metadata mp, const int64_t playlist_id, const char** external_track_ids, size_t external_track_id_count, int offset);
mcsdk_export bool mcsdk_svc_metadata_append_to_playlist_with_track_list(mcsdk_svc_metadata mp, const int64_t playlist_id, mcsdk_track_list track_list, int offset);
mcsdk_export size_t mcsdk_svc_metadata_remove_tracks_from_playlist(mcsdk_svc_metadata mp, const int64_t playlist_id, const char** external_ids, const int* sort_orders, int count);
mcsdk_export void mcsdk_svc_metadata_release(mcsdk_svc_metadata mp);

/*
 * IPlaybackService
 */

mcsdk_export void mcsdk_svc_playback_play_at(mcsdk_svc_playback pb, size_t index);
mcsdk_export bool mcsdk_svc_playback_next(mcsdk_svc_playback pb);
mcsdk_export bool mcsdk_svc_playback_previous(mcsdk_svc_playback pb);
mcsdk_export void mcsdk_svc_playback_stop(mcsdk_svc_playback pb);
mcsdk_export mcsdk_playback_repeat_mode mcsdk_svc_playback_get_repeat_mode(mcsdk_svc_playback pb);
mcsdk_export void mcsdk_svc_playback_set_repeat_mode(mcsdk_svc_playback pb, mcsdk_playback_repeat_mode mode);
mcsdk_export void mcsdk_svc_playback_toggle_repeat_mode(mcsdk_svc_playback pb);
mcsdk_export mcsdk_playback_state mcsdk_svc_playback_get_playback_state(mcsdk_svc_playback pb);
mcsdk_export bool mcsdk_svc_playback_is_shuffled(mcsdk_svc_playback pb);
mcsdk_export void mcsdk_svc_playback_toggle_shuffle(mcsdk_svc_playback pb);
mcsdk_export void mcsdk_svc_playback_pause_or_resume(mcsdk_svc_playback pb);
mcsdk_export double mcsdk_svc_playback_get_volume(mcsdk_svc_playback pb);
mcsdk_export void mcsdk_svc_playback_set_volume(mcsdk_svc_playback pb, double volume);
mcsdk_export double mcsdk_svc_playback_get_position(mcsdk_svc_playback pb);
mcsdk_export void mcsdk_svc_playback_set_position(mcsdk_svc_playback pb, double seconds);
mcsdk_export double mcsdk_svc_playback_get_duration(mcsdk_svc_playback pb);
mcsdk_export bool mcsdk_svc_playback_is_muted(mcsdk_svc_playback pb);
mcsdk_export void mcsdk_svc_playback_toggle_mute(mcsdk_svc_playback pb);
mcsdk_export size_t mcsdk_svc_playback_get_index(mcsdk_svc_playback pb);
mcsdk_export size_t mcsdk_svc_playback_count(mcsdk_svc_playback pb);
mcsdk_export mcsdk_track mcsdk_svc_playback_get_track(mcsdk_svc_playback pb, size_t index);
mcsdk_export mcsdk_track mcsdk_svc_playback_get_playing_track(mcsdk_svc_playback pb);
mcsdk_export void mcsdk_svc_playback_copy_from(mcsdk_svc_playback pb, const mcsdk_track_list track_list);
mcsdk_export void mcsdk_svc_playback_play(mcsdk_svc_playback pb, const mcsdk_track_list source, size_t index);
mcsdk_export mcsdk_track_list_editor mcsdk_svc_playback_edit_playlist(mcsdk_svc_playback pb);
mcsdk_export mcsdk_playback_time_change_mode mcsdk_svc_playback_get_time_change_mode(mcsdk_svc_playback pb);
mcsdk_export void mcsdk_svc_playback_set_time_change_mode(mcsdk_svc_playback pb, mcsdk_playback_time_change_mode mode);
mcsdk_export void mcsdk_svc_playback_reload_output(mcsdk_svc_playback pb);
mcsdk_export mcsdk_track_list mcsdk_svc_playback_clone(mcsdk_svc_playback pb);

/*
 * IPreferences
 */

mcsdk_export bool mcsdk_prefs_get_bool(mcsdk_prefs p, const char* key, bool defaultValue);
mcsdk_export int mcsdk_prefs_get_int(mcsdk_prefs p, const char* key, int defaultValue);
mcsdk_export double mcsdk_prefs_get_double(mcsdk_prefs p, const char* key, double defaultValue);
mcsdk_export int mcsdk_prefs_get_string(mcsdk_prefs p, const char* key, char* dst, size_t size, const char* defaultValue);
mcsdk_export void mcsdk_prefs_set_int(mcsdk_prefs p, const char* key, int value);
mcsdk_export void mcsdk_prefs_set_double(mcsdk_prefs p, const char* key, double value);
mcsdk_export void mcsdk_prefs_set_string(mcsdk_prefs p, const char* key, const char* value);
mcsdk_export void mcsdk_prefs_save(mcsdk_prefs p);
mcsdk_export void mcsdk_prefs_release(mcsdk_prefs p);

/*
 * IDataStream
 */

mcsdk_export bool mcsdk_data_stream_open(mcsdk_data_stream ds, const char *uri, mcsdk_data_stream_open_flags flags);
mcsdk_export bool mcsdk_data_stream_close(mcsdk_data_stream ds);
mcsdk_export void mcsdk_data_stream_interrupt(mcsdk_data_stream ds);
mcsdk_export bool mcsdk_data_stream_is_readable(mcsdk_data_stream ds);
mcsdk_export bool mcsdk_data_stream_is_writable(mcsdk_data_stream ds);
mcsdk_export long mcsdk_data_stream_read(mcsdk_data_stream ds, void* dst, long count);
mcsdk_export long mcsdk_data_stream_write(mcsdk_data_stream ds, void* src, long count);
mcsdk_export bool mcsdk_data_stream_set_position(mcsdk_data_stream ds, long position);
mcsdk_export long mcsdk_data_stream_get_position(mcsdk_data_stream ds);
mcsdk_export bool mcsdk_data_stream_is_seekable(mcsdk_data_stream ds);
mcsdk_export bool mcsdk_data_stream_is_eof(mcsdk_data_stream ds);
mcsdk_export long mcsdk_data_stream_get_length(mcsdk_data_stream ds);
mcsdk_export const char* mcsdk_data_stream_get_type(mcsdk_data_stream ds);
mcsdk_export const char* mcsdk_data_stream_get_uri(mcsdk_data_stream ds);
mcsdk_export bool mcsdk_data_stream_can_prefetch(mcsdk_data_stream ds);
mcsdk_export void mcsdk_data_stream_release(mcsdk_data_stream ds);

/*
 * IBuffer
 */

mcsdk_export long mcsdk_audio_buffer_get_sample_rate(mcsdk_audio_buffer ab);
mcsdk_export void mcsdk_audio_buffer_set_sample_rate(mcsdk_audio_buffer ab, long sample_rate);
mcsdk_export int mcsdk_audio_buffer_get_channels(mcsdk_audio_buffer ab);
mcsdk_export void mcsdk_audio_buffer_set_channels(mcsdk_audio_buffer ab, int channels);
mcsdk_export float* mcsdk_audio_buffer_get_buffer_pointer(mcsdk_audio_buffer ab);
mcsdk_export long mcsdk_audio_buffer_get_sample_count(mcsdk_audio_buffer ab);
mcsdk_export void mcsdk_audio_buffer_set_sample_count(mcsdk_audio_buffer ab, long samples);
mcsdk_export long mcsdk_audio_buffer_get_byte_count(mcsdk_audio_buffer ab);
mcsdk_export void mcsdk_audio_buffer_release(mcsdk_audio_buffer ab);

/*
 * IBufferProvider
 */

mcsdk_export void mcsdk_audio_buffer_provider_notify_processed(mcsdk_audio_buffer_provider abp, mcsdk_audio_buffer ab);

/*
 * IDevice
 */

mcsdk_export const char* mcsdk_audio_device_get_name(mcsdk_audio_device d);
mcsdk_export const char* mcsdk_audio_device_get_id(mcsdk_audio_device d);
mcsdk_export void mcsdk_audio_device_release(mcsdk_audio_device d);

/*
 * IDeviceList
 */

mcsdk_export size_t mcsdk_audio_device_list_get_count(mcsdk_audio_device_list dl);
mcsdk_export const mcsdk_audio_device mcsdk_audio_device_list_get_at(mcsdk_audio_device_list dl, size_t index);
mcsdk_export void mcsdk_audio_device_list_release(mcsdk_audio_device_list dl);

/*
 * IOutput
 */

mcsdk_export void mcsdk_audio_output_pause(mcsdk_audio_output o);
mcsdk_export void mcsdk_audio_output_resume(mcsdk_audio_output o);
mcsdk_export void mcsdk_audio_output_set_volume(mcsdk_audio_output o, double volume);
mcsdk_export double mcsdk_audio_output_get_volume(mcsdk_audio_output o);
mcsdk_export void mcsdk_audio_output_stop(mcsdk_audio_output o);
mcsdk_export int mcsdk_audio_output_play(mcsdk_audio_output o, mcsdk_audio_buffer ab, mcsdk_audio_buffer_provider abp);
mcsdk_export void mcsdk_audio_output_drain(mcsdk_audio_output o);
mcsdk_export double mcsdk_audio_output_get_latency(mcsdk_audio_output o);
mcsdk_export const char* mcsdk_audio_output_get_name(mcsdk_audio_output o);
mcsdk_export mcsdk_audio_device_list mcsdk_audio_output_get_device_list(mcsdk_audio_output o);
mcsdk_export bool mcsdk_audio_output_set_default_device(mcsdk_audio_output o, const char* device_id);
mcsdk_export mcsdk_audio_device mcsdk_audio_output_get_default_device(mcsdk_audio_output o);
mcsdk_export void mcsdk_audio_output_release(mcsdk_audio_output o);

/*
 * IDecoder
 */

mcsdk_export double mcsdk_decoder_set_position(mcsdk_decoder d, double seconds);
mcsdk_export bool mcsdk_decoder_fill_buffer(mcsdk_decoder d, mcsdk_audio_buffer ab);
mcsdk_export double mcsdk_decoder_get_duration(mcsdk_decoder d);
mcsdk_export bool mcsdk_decoder_open(mcsdk_decoder d, mcsdk_data_stream ds);
mcsdk_export bool mcsdk_decoder_is_eof(mcsdk_decoder d);
mcsdk_export void mcsdk_decoder_release(mcsdk_decoder d);

/*
 * IEncoder
 */

mcsdk_export mcsdk_encoder_type mcsdk_encoder_get_type(mcsdk_encoder e);
mcsdk_export void mcsdk_encoder_release(mcsdk_encoder e);

/*
 * IBlockingEncoder
 */

mcsdk_export bool mcsdk_blocking_encoder_initialize(mcsdk_blocking_encoder be, mcsdk_data_stream out, size_t rate, size_t channels, size_t bitrate);
mcsdk_export bool mcsdk_blocking_encoder_encode(mcsdk_blocking_encoder be, mcsdk_audio_buffer ab);
mcsdk_export void mcsdk_blocking_encoder_finalize(mcsdk_blocking_encoder be);
mcsdk_export void mcsdk_blocking_encoder_release(mcsdk_blocking_encoder be, mcsdk_encoder e);

/*
 * IStreamingEncoder
 */

mcsdk_export bool mcsdk_streaming_encoder_initialize(mcsdk_streaming_encoder se, size_t rate, size_t channels, size_t bitrate);
mcsdk_export int mcsdk_streaming_encoder_encode(mcsdk_streaming_encoder se, mcsdk_audio_buffer ab, char** data);
mcsdk_export int mcsdk_streaming_encoder_flush(mcsdk_streaming_encoder se, char** data);
mcsdk_export void mcsdk_streaming_encoder_finalize(mcsdk_streaming_encoder se, const char* uri);
mcsdk_export void mcsdk_streaming_encoder_release(mcsdk_streaming_encoder se);

/*
 * IDebug
 */

mcsdk_export void mcsdk_debug_verbose(const char* tag, const char* message);
mcsdk_export void mcsdk_debug_info(const char* tag, const char* message);
mcsdk_export void mcsdk_debug_warning(const char* tag, const char* message);
mcsdk_export void mcsdk_debug_error(const char* tag, const char* message);

/*
 * IEnvironment
 */

mcsdk_export size_t mcsdk_env_get_path(mcsdk_env_path_type type, char* dst, int size);
mcsdk_export mcsdk_data_stream mcsdk_env_open_data_stream(const char* uri, mcsdk_data_stream_open_flags flags);
mcsdk_export mcsdk_decoder mcsdk_env_open_decoder(mcsdk_data_stream stream);
mcsdk_export mcsdk_encoder mcsdk_env_open_encoder(const char* type) ;
mcsdk_export mcsdk_audio_buffer mcsdk_env_create_audio_buffer(size_t samples, size_t rate, size_t channels);
mcsdk_export mcsdk_prefs mcsdk_env_open_preferences(const char* name);
mcsdk_export size_t mcsdk_env_get_output_count();
mcsdk_export mcsdk_audio_output mcsdk_env_get_output_at_index(size_t index);
mcsdk_export mcsdk_audio_output mcsdk_env_get_output_with_name(const char* name);
mcsdk_export mcsdk_playback_replay_gain_mode mcsdk_env_get_replay_gain_mode();
mcsdk_export void mcsdk_env_set_replay_gain_mode(mcsdk_playback_replay_gain_mode mode);
mcsdk_export float mcsdk_env_get_preamp_gain();
mcsdk_export void mcsdk_env_set_preamp_gain(float gain);
mcsdk_export bool mcsdk_env_is_equalizer_enabled();
mcsdk_export void mcsdk_env_set_equalizer_enabled(bool enabled);
mcsdk_export bool mcsdk_env_get_equalizer_band_values(double target[], size_t count);
mcsdk_export bool mcsdk_env_set_equalizer_band_values(double values[], size_t count);
mcsdk_export void mcsdk_env_reload_playback_output();
mcsdk_export void mcsdk_env_set_default_output(mcsdk_audio_output output);
mcsdk_export mcsdk_audio_output mcsdk_env_get_default_output();
mcsdk_export mcsdk_playback_transport_type mcsdk_env_get_transport_type();
mcsdk_export void mcsdk_env_set_transport_type(mcsdk_playback_transport_type type);

/*
 * IStream
 */

mcsdk_export mcsdk_audio_stream mcsdk_audio_stream_create(int samples_per_channel, double buffer_length_seconds, mcsdk_audio_stream_flags options);
mcsdk_export mcsdk_audio_buffer mcsdk_audio_stream_get_next_buffer(mcsdk_audio_stream as);
mcsdk_export void mcsdk_audio_stream_recycle_buffer(mcsdk_audio_stream as, mcsdk_audio_buffer ab);
mcsdk_export double mcsdk_audio_stream_set_position(mcsdk_audio_stream as, double seconds);
mcsdk_export double mcsdk_audio_stream_get_duration(mcsdk_audio_stream as);
mcsdk_export bool mcsdk_audio_stream_open_uri(mcsdk_audio_stream as, const char* uri);
mcsdk_export void mcsdk_audio_stream_interrupt(mcsdk_audio_stream as);
mcsdk_export mcsdk_audio_stream_capability mcsdk_audio_stream_get_capabilities(mcsdk_audio_stream as);
mcsdk_export bool mcsdk_audio_stream_is_eof(mcsdk_audio_stream as);
mcsdk_export void mcsdk_audio_stream_release(mcsdk_audio_stream as);

/*
 * Player
 */

mcsdk_export mcsdk_audio_player mcsdk_audio_player_create(const char* url, mcsdk_audio_output output, mcsdk_audio_player_callbacks* callbacks, mcsdk_audio_player_gain gain);
mcsdk_export int mcsdk_audio_player_get_url(mcsdk_audio_player ap, char* dst, int size);
mcsdk_export void mcsdk_audio_player_detach(mcsdk_audio_player ap, mcsdk_audio_player_callbacks* callbacks);
mcsdk_export void mcsdk_audio_player_attach(mcsdk_audio_player ap, mcsdk_audio_player_callbacks* callbacks);
mcsdk_export void mcsdk_audio_player_play(mcsdk_audio_player ap);
mcsdk_export double mcsdk_audio_player_get_position(mcsdk_audio_player ap);
mcsdk_export void mcsdk_audio_player_set_position(mcsdk_audio_player ap, double seconds);
mcsdk_export double mcsdk_audio_player_get_duration(mcsdk_audio_player ap);
mcsdk_export void mcsdk_audio_player_add_mix_point(mcsdk_audio_player ap, int id, double time);
mcsdk_export bool mcsdk_audio_player_has_capability(mcsdk_audio_player ap, mcsdk_audio_stream_capability capability);
mcsdk_export mcsdk_audio_player_gain mcsdk_audio_player_get_default_gain();
mcsdk_export void mcsdk_audio_player_release(mcsdk_audio_player ap, mcsdk_audio_player_release_mode mode);

/*
 * IIndexer
 */

mcsdk_export void mcsdk_svc_indexer_add_path(mcsdk_svc_indexer in, const char* path);
mcsdk_export void mcsdk_svc_indexer_remove_path(mcsdk_svc_indexer in, const char* path);
mcsdk_export int mcsdk_svc_indexer_get_paths_count(mcsdk_svc_indexer in);
mcsdk_export int mcsdk_svc_indexer_get_paths_at(mcsdk_svc_indexer in, int index, char* dst, int len);
mcsdk_export void mcsdk_svc_indexer_schedule(mcsdk_svc_indexer in, mcsdk_svc_indexer_sync_type type);
mcsdk_export void mcsdk_svc_indexer_stop(mcsdk_svc_indexer in);
mcsdk_export mcsdk_svc_indexer_state mcsdk_svc_indexer_get_state(mcsdk_svc_indexer in);
mcsdk_export void mcsdk_svc_indexer_add_callbacks(mcsdk_svc_indexer in, mcsdk_svc_indexer_callbacks* cb);
mcsdk_export void mcsdk_svc_indexer_remove_callbacks(mcsdk_svc_indexer in, mcsdk_svc_indexer_callbacks* cb);

/*
 * ILibrary
 */

mcsdk_export void mcsdk_svc_library_run_query(mcsdk_svc_library l, const char* name, void* user_context, mcsdk_svc_library_run_query_callback cb, mcsdk_svc_library_query_flag flags);
mcsdk_export int mcsdk_svc_library_get_id(mcsdk_svc_library l);
mcsdk_export int mcsdk_svc_library_get_name(mcsdk_svc_library l, char* dst, int len);

/*
 * Statement
 */

mcsdk_export mcsdk_db_statement mcsdk_db_statement_create(mcsdk_db_connection db, const char* sql);
mcsdk_export void mcsdk_db_statement_bind_int32(mcsdk_db_statement stmt, int position, int value);
mcsdk_export void mcsdk_db_statement_bind_int64(mcsdk_db_statement stmt, int position, int64_t value);
mcsdk_export void mcsdk_db_statement_bind_float(mcsdk_db_statement stmt, int position, float value);
mcsdk_export void mcsdk_db_statement_bind_text(mcsdk_db_statement stmt, int position, const char* value);
mcsdk_export void mcsdk_db_statement_bind_null(mcsdk_db_statement stmt, int position);
mcsdk_export int mcsdk_db_statement_column_int32(mcsdk_db_statement stmt, int column);
mcsdk_export int64_t mcsdk_db_statement_column_int64(mcsdk_db_statement stmt, int column);
mcsdk_export float mcsdk_db_statement_column_float(mcsdk_db_statement stmt, int column);
mcsdk_export int mcsdk_db_statement_column_text(mcsdk_db_statement stmt, int column, char* dst, int len);
mcsdk_export mcsdk_db_result mcsdk_db_statement_step(mcsdk_db_statement stmt);
mcsdk_export void mcsdk_db_statement_reset(mcsdk_db_statement stmt);
mcsdk_export void mcsdk_db_statement_unbind(mcsdk_db_statement stmt);
mcsdk_export void mcsdk_db_statement_reset_and_unbind(mcsdk_db_statement stmt);
mcsdk_export void mcsdk_db_statement_release(mcsdk_db_statement stmt);

/*
 * ScopedTransaction
 */

mcsdk_export mcsdk_db_transaction mcsdk_db_transaction_create(mcsdk_db_connection db);
mcsdk_export void mcsdk_db_transaction_cancel(mcsdk_db_transaction tx);
mcsdk_export void mcsdk_db_transaction_commit_and_restart(mcsdk_db_transaction tx);
mcsdk_export void mcsdk_db_transaction_release(mcsdk_db_transaction tx);

#endif