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

#ifndef __C_MUSIKCORE_SDK_C_H__
#define __C_MUSIKCORE_SDK_C_H__

/** @file musikcore_c.h
 *  @brief C language binding for the musikcube core (musikcore_c SDK).
 *  @details A pure C API (C++ compiled) exposing the core services: libraries,
 *      metadata queries, playback, preferences, audio streams/players, database
 *      access and the indexer. All exported symbols use the mcsdk_ prefix and
 *      opaque handle types. Plugin authors use this header via musikcore's
 *      plugin SDK. */

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

/** @name Version
 *  @brief Version of the C SDK interface. */
/** @{ */
/** @brief The current C SDK interface version. */
static const int mcsdk_version = 18;
/** @} */

/** @name Constants
 *  @brief Shared enumerations and constants used across the C API. */
/** @{ */

/** @brief Playback state of the transport. */
typedef enum mcsdk_playback_state {
    mcsdk_playback_stopped = 1,  /**< Playback stopped. */
    mcsdk_playback_paused = 2,   /**< Playback paused. */
    mcsdk_playback_prepared = 3, /**< Track prepared but not yet playing. */
    mcsdk_playback_playing = 4,  /**< Currently playing. */
} mcsdk_playback_state;

/** @brief Stream-level events reported by the transport. */
typedef enum mcsdk_stream_event {
    mcsdk_stream_scheduled = 1, /**< A stream was scheduled. */
    mcsdk_stream_prepared = 2,  /**< A stream was prepared. */
    mcsdk_stream_playing = 3,   /**< A stream began playing. */
    mcsdk_stream_almost_done = 4, /**< A stream is nearly finished. */
    mcsdk_stream_finished = 5,  /**< A stream finished. */
    mcsdk_stream_stopped = 6,   /**< A stream was stopped. */
    mcsdk_stream_error = -1     /**< A stream errored. */
} mcsdk_stream_event;

/** @brief Repeat mode of the playback service. */
typedef enum mcsdk_repeat_mode {
    mcsdk_repeat_none = 0,  /**< No repeat. */
    mcsdk_repeat_track = 1, /**< Repeat the current track. */
    mcsdk_repeat_list = 2   /**< Repeat the whole list. */
} mcsdk_repeat_mode;

/** @brief Result codes returned by audio output operations. */
typedef enum mcsdk_audio_output_code {
    mcsdk_audio_output_error_invalid_format = -4, /**< Output rejected the buffer format. */
    mcsdk_audio_output_error_invalid_state = -3,  /**< Output is in an invalid state. */
    mcsdk_audio_output_error_buffer_full = -2,    /**< Output buffers are full. */
    mcsdk_audio_output_error_buffer_written = -1  /**< Buffers were written (soft error). */
} mcsdk_audio_output_code;

/** @brief Playback error codes. */
typedef enum mcsdk_playback_error_code {
    mcsdk_playback_error_unknown = -1,    /**< Unknown error. */
    mcsdk_playback_error_open_failed = 0, /**< The stream could not be opened. */
} mcsdk_playback_error_code;

/** @brief How position changes are applied during playback. */
typedef enum mcsdk_time_change_mode {
    mcsdk_time_change_mode_seek = 0,  /**< Seek (pause-free reposition). */
    mcsdk_time_change_mode_scrub = 1  /**< Scrub (live reposition). */
} mcsdk_time_change_mode;

/** @brief Well-known path types resolved by the environment. */
typedef enum mcsdk_path_type {
    mcsdk_path_type_user_home = 0,     /**< User home directory. */
    mcsdk_path_type_data = 1,          /**< Application data directory. */
    mcsdk_path_type_application = 2,   /**< Application directory. */
    mcsdk_path_type_plugins = 3,       /**< Plugin directory. */
    mcsdk_path_type_library = 4        /**< Library directory. */
} mcsdk_path_type;

/** @brief Capabilities a stream may support. */
typedef enum mcsdk_stream_capability {
    mcsdk_stream_capability_prebuffer = 0x01 /**< Stream supports pre-buffering. */
} mcsdk_stream_capability;

/** @brief Result of an indexer source scan. */
typedef enum mcsdk_svc_indexer_scan_result {
    mcsdk_svc_indexer_scan_result_commit = 1,   /**< Commit the scan results. */
    mcsdk_svc_indexer_scan_result_rollback = 2  /**< Roll back the scan results. */
} mcsdk_svc_indexer_scan_result;

/** @brief Indexer lifecycle state. */
typedef enum mcsdk_svc_indexer_state {
    mcsdk_svc_indexer_state_idle = 0,     /**< Not indexing. */
    mcsdk_svc_indexer_state_indexing = 1, /**< Indexing in progress. */
    mcsdk_svc_indexer_state_stopping = 2, /**< Indexing is stopping. */
    mcsdk_svc_indexer_state_stopped = 3   /**< Indexing has stopped. */
} mcsdk_svc_indexer_state;

/** @brief Scope of an indexer synchronization. */
typedef enum mcsdk_svc_indexer_sync_type {
    mcsdk_svc_indexer_sync_type_all = 0,     /**< Sync all sources. */
    mcsdk_svc_indexer_sync_type_local = 1,   /**< Sync local sources only. */
    mcsdk_svc_indexer_sync_type_rebuild = 2, /**< Rebuild the entire index. */
    mcsdk_svc_indexer_sync_type_sources = 3  /**< Sync sources whose paths changed. */
} mcsdk_svc_indexer_sync_type;

/** @brief Flags controlling how a library query runs. */
typedef enum mcsdk_svc_library_query_flag {
    mcsdk_svc_library_query_flag_none = 0,         /**< Default (asynchronous). */
    mcsdk_svc_library_query_flag_synchronous = 1   /**< Run synchronously. */
} mcsdk_svc_library_query_flag;

/** @brief ReplayGain processing mode. */
typedef enum mcsdk_replay_gain_mode {
    mcsdk_replay_gain_mode_disabled = 0, /**< ReplayGain disabled. */
    mcsdk_replay_gain_mode_track = 1,    /**< Apply track gain. */
    mcsdk_replay_gain_mode_album = 2     /**< Apply album gain. */
} mcsdk_replay_gain_mode;

/** @brief The audio transport implementation in use. */
typedef enum mcsdk_transport_type {
    mcsdk_transport_type_gapless = 0,   /**< Gapless transport. */
    mcsdk_transport_type_crossfade = 1  /**< Crossfading transport. */
} mcsdk_transport_type;

/** @brief Access flags for opening data streams. */
typedef enum mcsdk_stream_open_flags {
    mcsdk_stream_open_flags_none = 0,  /**< No access. */
    mcsdk_stream_open_flags_read = 1,  /**< Read access. */
    mcsdk_stream_open_flags_write = 2  /**< Write access. */
} mcsdk_stream_open_flags;

/** @brief Options for creating audio streams. */
typedef enum mcsdk_audio_stream_flags {
    mcsdk_audio_stream_flags_none = 0,    /**< No special options. */
    mcsdk_audio_stream_flags_no_dsp = 1   /**< Bypass the DSP plugin chain. */
} mcsdk_audio_stream_flags;

/** @brief The kind of resource behind an mcsdk_resource handle. */
typedef enum mcsdk_resource_class {
    mcsdk_resource_type_value = 0, /**< A scalar value. */
    mcsdk_resource_type_map = 1    /**< A key/value map. */
} mcsdk_resource_class;

/** @brief The kind of encoder implementation. */
typedef enum mcsdk_encoder_type {
    mcsdk_encoder_type_none = 0,      /**< Not an encoder. */
    mcsdk_encoder_type_blocking = 1,  /**< Blocking encoder. */
    mcsdk_encoder_type_streaming = 2  /**< Streaming encoder. */
} mcsdk_encoder_type;

/** @brief How a player should behave when released. */
typedef enum mcsdk_audio_player_release_mode {
    mcsdk_audio_player_release_mode_drain = 0,     /**< Drain queued audio before stopping. */
    mcsdk_audio_player_release_mode_no_drain = 1   /**< Stop immediately. */
} mcsdk_audio_player_release_mode;

/** @brief Result codes from database statement steps. */
typedef enum mcsdk_db_result {
    mcsdk_db_result_okay = 0,  /**< Operation completed successfully. */
    mcsdk_db_result_row = 100, /**< A result row is available. */
    mcsdk_db_result_done = 101,/**< Statement finished iterating. */
} mcsdk_db_result;

/** @brief Number of equalizer bands. */
static const size_t mcsdk_equalizer_band_count = 18;

/** @brief Center frequencies of the equalizer bands, in Hz. */
static const size_t mcsdk_equalizer_bands[] = {
    65, 92, 131, 185, 262, 370, 523, 740, 1047, 1480,
    2093, 2960, 4186, 5920, 8372, 11840, 16744, 22000
};

/** @brief Sentinel value meaning "no offset". */
static const int mcsdk_no_offset = 0;
/** @brief Sentinel value meaning "no limit". */
static const int mcsdk_no_limit = -1;

/** @brief Category type names used by metadata queries. */
static const char* mcsdk_category_type_album = "album";         /**< Albums. */
static const char* mcsdk_category_type_artist = "artist";       /**< Artists. */
static const char* mcsdk_category_type_album_artist = "album_artist"; /**< Album artists. */
static const char* mcsdk_category_type_genre = "genre";         /**< Genres. */
static const char* mcsdk_category_type_playlist = "playlists";  /**< Playlists. */

/** @brief Track field names used by metadata queries. */
static const char* mcsdk_track_field_id = "id"; /**< Track id. */
static const char* mcsdk_track_field_track_num = "track"; /**< Track number. */
static const char* mcsdk_track_field_disc_num = "disc";   /**< Disc number. */
static const char* mcsdk_track_field_bpm = "bpm";         /**< Beats per minute. */
static const char* mcsdk_track_field_duration = "duration"; /**< Duration, in seconds. */
static const char* mcsdk_track_field_filesize = "filesize"; /**< File size, in bytes. */
static const char* mcsdk_track_field_year = "year";       /**< Release year. */
static const char* mcsdk_track_field_title = "title";     /**< Track title. */
static const char* mcsdk_track_field_filename = "filename"; /**< File name. */
static const char* mcsdk_track_field_thumbnail_id = "thumbnail_id"; /**< Thumbnail id. */
static const char* mcsdk_track_field_album = "album";     /**< Album name. */
static const char* mcsdk_track_field_album_artist = "album_artist"; /**< Album artist name. */
static const char* mcsdk_track_field_genre = "genre";     /**< Genre name. */
static const char* mcsdk_track_field_artist = "artist";   /**< Artist name. */
static const char* mcsdk_track_field_filetime = "filetime"; /**< File modification time. */
static const char* mcsdk_track_field_genre_id = "visual_genre_id"; /**< Genre id. */
static const char* mcsdk_track_field_artist_id = "visual_artist_id"; /**< Artist id. */
static const char* mcsdk_track_field_album_artist_id = "album_artist_id"; /**< Album artist id. */
static const char* mcsdk_track_field_album_id = "album_id"; /**< Album id. */
static const char* mcsdk_track_field_source_id = "source_id"; /**< Indexer source id. */
static const char* mcsdk_track_field_external_id = "external_id"; /**< External id. */
/** @} */

/** @name Types
 *  @brief Opaque handle and callback types. */
/** @{ */

/** @brief Defines an opaque handle type.
 *  @param x The handle type name. */
#define mcsdk_define_handle(x) \
    typedef struct x { \
        void* opaque; \
    } x;

/** @brief Tests whether a handle wraps a non-null pointer.
 *  @param x The handle to test. */
#define mcsdk_handle_ok(x) x.opaque != NULL

/** @brief Casts an opaque pointer into a handle.
 *  @param x The pointer to wrap. */
#define mcsdk_cast_handle(x) { x.opaque }

/** @brief Compares two handles for equality.
 *  @param x First handle.
 *  @param y Second handle. */
#define mcsdk_handle_equals(x, y) x.opaque == y.opaque

/** @brief Opaque handle types used throughout the C API. */
mcsdk_define_handle(mcsdk_internal);
mcsdk_define_handle(mcsdk_resource);
mcsdk_define_handle(mcsdk_value);
mcsdk_define_handle(mcsdk_value_list);
mcsdk_define_handle(mcsdk_map);
mcsdk_define_handle(mcsdk_track);
mcsdk_define_handle(mcsdk_map_list);
mcsdk_define_handle(mcsdk_track_list);
mcsdk_define_handle(mcsdk_track_list_editor);
mcsdk_define_handle(mcsdk_svc_metadata);
mcsdk_define_handle(mcsdk_svc_playback);
mcsdk_define_handle(mcsdk_svc_indexer);
mcsdk_define_handle(mcsdk_svc_library);
mcsdk_define_handle(mcsdk_prefs);
mcsdk_define_handle(mcsdk_audio_buffer);
mcsdk_define_handle(mcsdk_audio_buffer_provider);
mcsdk_define_handle(mcsdk_data_stream);
mcsdk_define_handle(mcsdk_device);
mcsdk_define_handle(mcsdk_device_list);
mcsdk_define_handle(mcsdk_audio_output);
mcsdk_define_handle(mcsdk_decoder);
mcsdk_define_handle(mcsdk_encoder);
mcsdk_define_handle(mcsdk_blocking_encoder);
mcsdk_define_handle(mcsdk_streaming_encoder);
mcsdk_define_handle(mcsdk_audio_stream);
mcsdk_define_handle(mcsdk_audio_player);
mcsdk_define_handle(mcsdk_db_connection);
mcsdk_define_handle(mcsdk_db_statement);
mcsdk_define_handle(mcsdk_db_transaction);

/** @brief Callbacks reported by an audio player.
 *  @details A subset of Player::EventListener translated into C function pointers. */
typedef struct mcsdk_audio_player_callbacks {
    void (*on_prepared)(mcsdk_audio_player p);          /**< Player prepared. */
    void (*on_started)(mcsdk_audio_player p);           /**< Playback started. */
    void (*on_stream_eof)(mcsdk_audio_player p);        /**< Stream reached EOF. */
    void (*on_finished)(mcsdk_audio_player p);          /**< Playback finished. */
    void (*on_error)(mcsdk_audio_player p, mcsdk_playback_error_code ec); /**< Playback error. */
    void (*on_destroying)(mcsdk_audio_player p);        /**< Player being destroyed. */
    void (*on_mixpoint)(mcsdk_audio_player p, int id, double time); /**< Mix point reached. */
} mcsdk_audio_player_callbacks;

/** @brief Callbacks reported by the indexer. */
typedef struct mcsdk_svc_indexer_callbacks {
    void (*on_started)(mcsdk_svc_indexer i); /**< Indexing started. */
    void (*on_finished)(mcsdk_svc_indexer i, int tracks_processed); /**< Indexing finished. */
    void (*on_progress)(mcsdk_svc_indexer i, int tracks_processed); /**< Indexing progress. */
} mcsdk_svc_indexer_callbacks;

/** @brief Gain profile applied to an audio player. */
typedef struct mcsdk_audio_player_gain {
    float preamp;     /**< Pre-amp gain multiplier. */
    float gain;       /**< Track gain multiplier. */
    float peak;       /**< Measured peak level. */
    float peakValid;  /**< Non-zero if peak contains a valid measurement. */
} mcsdk_audio_player_gain;

/** @brief Callback used to run a raw SQL query on the library database.
 *  @return true to keep stepping the result, false to stop. */
typedef bool (*mcsdk_svc_library_run_query_callback)(mcsdk_svc_library l, mcsdk_db_connection db, void* user_context);

/** @brief Callback invoked when a buffer provider hands a buffer to the output.
 *  @return true if the buffer may be reused by the provider. */
typedef bool (*mcsdk_audio_buffer_provider_processed_callback)(mcsdk_audio_buffer buffer);
/** @} */

/** @name Global setup
 *  @brief Process-wide environment initialization. */
/** @{ */
/** @brief Initializes the core environment and plugin system. */
mcsdk_export void mcsdk_env_init();
/** @brief Releases the core environment. */
mcsdk_export void mcsdk_env_release();
/** @} */

/** @name Instance context
 *  @brief Aggregates all core service handles into one context. */
/** @{ */

/** @brief All core service handles for one application context. */
typedef struct mcsdk_context {
    mcsdk_svc_metadata metadata; /**< Metadata service. */
    mcsdk_svc_playback playback; /**< Playback service. */
    mcsdk_svc_indexer indexer;   /**< Indexer service. */
    mcsdk_svc_library library;   /**< Library service. */
    mcsdk_db_connection db;      /**< Database connection. */
    mcsdk_prefs preferences;     /**< Preferences store. */
    mcsdk_internal internal;     /**< Internal implementation data. */
} mcsdk_context;

/** @brief Creates a new application context.
 *  @param context Output handle for the created context.
 *  @details Callers own the context; release with mcsdk_context_release(). */
mcsdk_export void mcsdk_context_init(mcsdk_context** context);
/** @brief Releases an application context.
 *  @param context Pointer to the context handle to release. */
mcsdk_export void mcsdk_context_release(mcsdk_context** context);
/** @brief Sets the context used by plugin API calls.
 *  @param context The context to install. */
mcsdk_export void mcsdk_set_plugin_context(mcsdk_context* context);
/** @return true if a plugin context is currently installed.
 *  @param context The context to test. */
mcsdk_export bool mcsdk_is_plugin_context(mcsdk_context* context);
/** @} */

/** @name IResource
 *  @brief Functions on the base mcsdk_resource handle. */
/** @{ */

mcsdk_export int64_t mcsdk_resource_get_id(mcsdk_resource r);
mcsdk_export mcsdk_resource_class mcsdk_resource_get_class(mcsdk_resource r);
mcsdk_export void mcsdk_resource_release(mcsdk_resource r);
/** @} */

/** @name IValue
 *  @brief Functions on the mcsdk_value handle. */
/** @{ */

mcsdk_export size_t mcsdk_value_get_value(mcsdk_value v, char* dst, size_t size);
mcsdk_export void mcsdk_value_release(mcsdk_value v);
/** @} */

/** @name IValueList
 *  @brief Functions on the mcsdk_value_list handle. */
/** @{ */

mcsdk_export size_t mcsdk_value_list_count(mcsdk_value_list vl);
mcsdk_export mcsdk_value mcsdk_value_list_get_at(mcsdk_value_list vl, size_t index);
mcsdk_export void mcsdk_value_list_release(mcsdk_value_list vl);
/** @} */

/** @name IMap
 *  @brief Functions on the mcsdk_map handle. */
/** @{ */

mcsdk_export int mcsdk_map_get_string(mcsdk_map m, const char* key, char* dst, int size);
mcsdk_export int64_t mcsdk_map_get_int64(mcsdk_map m, const char* key, int64_t default_value);
mcsdk_export int32_t mcsdk_map_get_int32(mcsdk_map m, const char* key, int32_t default_value);
mcsdk_export double mcsdk_map_get_double(mcsdk_map m, const char* key, double default_value);
mcsdk_export void mcsdk_map_release(mcsdk_map m);
/** @} */

/** @name IMapList
 *  @brief Functions on the mcsdk_map_list handle. */
/** @{ */

mcsdk_export size_t mcsdk_map_list_get_count(mcsdk_map_list ml);
mcsdk_export mcsdk_map mcsdk_map_list_get_at(mcsdk_map_list ml, size_t index);
mcsdk_export void mcsdk_map_list_release(mcsdk_map_list ml);
/** @} */

/** @name ITrack
 *  @brief Functions on the mcsdk_track handle. */
/** @{ */

mcsdk_export void mcsdk_track_retain(mcsdk_track t);
mcsdk_export int mcsdk_track_get_uri(mcsdk_track t, char* dst, int size);
mcsdk_export void mcsdk_track_release(mcsdk_track t);
/** @} */

/** @name ITrackList
 *  @brief Functions on the mcsdk_track_list handle. */
/** @{ */

mcsdk_export size_t mcsdk_track_list_get_count(mcsdk_track_list tl);
mcsdk_export int64_t mcsdk_track_list_get_id(mcsdk_track_list tl, size_t index);
mcsdk_export int64_t mcsdk_track_list_index_of(mcsdk_track_list tl, int64_t id);
mcsdk_export mcsdk_track mcsdk_track_list_get_track_at(mcsdk_track_list tl, size_t index);
mcsdk_export void mcsdk_track_list_release(mcsdk_track_list tl);
/** @} */

/** @name TrackList
 *  @brief Track list creation and editing functions. */
/** @{ */

mcsdk_export mcsdk_track_list mcsdk_track_list_create(mcsdk_context* context);
mcsdk_export bool mcsdk_track_list_can_edit(mcsdk_track_list tl);
mcsdk_export mcsdk_track_list_editor mcsdk_track_list_edit(mcsdk_track_list tl);
/** @} */

/** @name ITrackListEditor
 *  @brief Functions on the mcsdk_track_list_editor handle. */
/** @{ */

mcsdk_export bool mcsdk_track_list_editor_insert(mcsdk_track_list_editor tle, int64_t id, size_t index);
mcsdk_export bool mcsdk_track_list_editor_swap(mcsdk_track_list_editor tle, size_t index1, size_t index2);
mcsdk_export bool mcsdk_track_list_editor_move(mcsdk_track_list_editor tle, size_t from, size_t to);
mcsdk_export bool mcsdk_track_list_editor_delete(mcsdk_track_list_editor tle, size_t index);
mcsdk_export void mcsdk_track_list_editor_add(mcsdk_track_list_editor tle, const int64_t id);
mcsdk_export void mcsdk_track_list_editor_clear(mcsdk_track_list_editor tle);
mcsdk_export void mcsdk_track_list_editor_shuffle(mcsdk_track_list_editor tle);
mcsdk_export void mcsdk_track_list_editor_release(mcsdk_track_list_editor tle);
/** @} */

/** @name IMetadataProxy
 *  @brief Metadata query functions on the mcsdk_svc_metadata handle. */
/** @{ */

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
/** @} */

/** @name IPlaybackService
 *  @brief Playback control functions on the mcsdk_svc_playback handle. */
/** @{ */

mcsdk_export void mcsdk_svc_playback_play_at(mcsdk_svc_playback pb, size_t index);
mcsdk_export bool mcsdk_svc_playback_next(mcsdk_svc_playback pb);
mcsdk_export bool mcsdk_svc_playback_previous(mcsdk_svc_playback pb);
mcsdk_export void mcsdk_svc_playback_stop(mcsdk_svc_playback pb);
mcsdk_export mcsdk_repeat_mode mcsdk_svc_playback_get_repeat_mode(mcsdk_svc_playback pb);
mcsdk_export void mcsdk_svc_playback_set_repeat_mode(mcsdk_svc_playback pb, mcsdk_repeat_mode mode);
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
mcsdk_export mcsdk_time_change_mode mcsdk_svc_playback_get_time_change_mode(mcsdk_svc_playback pb);
mcsdk_export void mcsdk_svc_playback_set_time_change_mode(mcsdk_svc_playback pb, mcsdk_time_change_mode mode);
mcsdk_export void mcsdk_svc_playback_reload_output(mcsdk_svc_playback pb);
mcsdk_export mcsdk_track_list mcsdk_svc_playback_clone_track_list(mcsdk_svc_playback pb);
/** @} */

/** @name IPreferences
 *  @brief Preferences access functions on the mcsdk_prefs handle. */
/** @{ */

mcsdk_export bool mcsdk_prefs_get_bool(mcsdk_prefs p, const char* key, bool defaultValue);
mcsdk_export int mcsdk_prefs_get_int(mcsdk_prefs p, const char* key, int defaultValue);
mcsdk_export double mcsdk_prefs_get_double(mcsdk_prefs p, const char* key, double defaultValue);
mcsdk_export int mcsdk_prefs_get_string(mcsdk_prefs p, const char* key, char* dst, size_t size, const char* defaultValue);
mcsdk_export void mcsdk_prefs_set_int(mcsdk_prefs p, const char* key, int value);
mcsdk_export void mcsdk_prefs_set_double(mcsdk_prefs p, const char* key, double value);
mcsdk_export void mcsdk_prefs_set_string(mcsdk_prefs p, const char* key, const char* value);
mcsdk_export void mcsdk_prefs_save(mcsdk_prefs p);
mcsdk_export void mcsdk_prefs_release(mcsdk_prefs p);
/** @} */

/** @name IDataStream
 *  @brief Data stream functions on the mcsdk_data_stream handle. */
/** @{ */

mcsdk_export bool mcsdk_data_stream_open(mcsdk_data_stream ds, const char *uri, mcsdk_stream_open_flags flags);
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
/** @} */

/** @name IBuffer
 *  @brief Audio buffer functions on the mcsdk_audio_buffer handle. */
/** @{ */

mcsdk_export long mcsdk_audio_buffer_get_sample_rate(mcsdk_audio_buffer ab);
mcsdk_export void mcsdk_audio_buffer_set_sample_rate(mcsdk_audio_buffer ab, long sample_rate);
mcsdk_export int mcsdk_audio_buffer_get_channels(mcsdk_audio_buffer ab);
mcsdk_export void mcsdk_audio_buffer_set_channels(mcsdk_audio_buffer ab, int channels);
mcsdk_export float* mcsdk_audio_buffer_get_buffer_pointer(mcsdk_audio_buffer ab);
mcsdk_export long mcsdk_audio_buffer_get_sample_count(mcsdk_audio_buffer ab);
mcsdk_export void mcsdk_audio_buffer_set_sample_count(mcsdk_audio_buffer ab, long samples);
mcsdk_export long mcsdk_audio_buffer_get_byte_count(mcsdk_audio_buffer ab);
mcsdk_export void mcsdk_audio_buffer_release(mcsdk_audio_buffer ab);
/** @} */

/** @name IBufferProvider
 *  @brief Functions on the mcsdk_audio_buffer_provider handle. */
/** @{ */

mcsdk_export mcsdk_audio_buffer_provider mcsdk_audio_audio_buffer_provider_create(mcsdk_audio_buffer_provider_processed_callback cb);
mcsdk_export void mcsdk_audio_audio_buffer_provider_release(mcsdk_audio_buffer_provider abp);
/** @} */

/** @name IDevice
 *  @brief Functions on the mcsdk_device handle. */
/** @{ */

mcsdk_export const char* mcsdk_device_get_name(mcsdk_device d);
mcsdk_export const char* mcsdk_device_get_id(mcsdk_device d);
mcsdk_export void mcsdk_device_release(mcsdk_device d);
/** @} */

/** @name IDeviceList
 *  @brief Functions on the mcsdk_device_list handle. */
/** @{ */

mcsdk_export size_t mcsdk_device_list_get_count(mcsdk_device_list dl);
mcsdk_export const mcsdk_device mcsdk_device_list_get_at(mcsdk_device_list dl, size_t index);
mcsdk_export void mcsdk_device_list_release(mcsdk_device_list dl);
/** @} */

/** @name IOutput
 *  @brief Audio output functions on the mcsdk_audio_output handle. */
/** @{ */

mcsdk_export void mcsdk_audio_output_pause(mcsdk_audio_output o);
mcsdk_export void mcsdk_audio_output_resume(mcsdk_audio_output o);
mcsdk_export void mcsdk_audio_output_set_volume(mcsdk_audio_output o, double volume);
mcsdk_export double mcsdk_audio_output_get_volume(mcsdk_audio_output o);
mcsdk_export void mcsdk_audio_output_stop(mcsdk_audio_output o);
mcsdk_export mcsdk_audio_output_code mcsdk_audio_output_play(mcsdk_audio_output o, mcsdk_audio_buffer ab, mcsdk_audio_buffer_provider abp);
mcsdk_export void mcsdk_audio_output_drain(mcsdk_audio_output o);
mcsdk_export double mcsdk_audio_output_get_latency(mcsdk_audio_output o);
mcsdk_export const char* mcsdk_audio_output_get_name(mcsdk_audio_output o);
mcsdk_export mcsdk_device_list mcsdk_audio_output_get_device_list(mcsdk_audio_output o);
mcsdk_export bool mcsdk_audio_output_set_default_device(mcsdk_audio_output o, const char* device_id);
mcsdk_export mcsdk_device mcsdk_audio_output_get_default_device(mcsdk_audio_output o);
mcsdk_export void mcsdk_audio_output_release(mcsdk_audio_output o);
/** @} */

/** @name IDecoder
 *  @brief Decoder functions on the mcsdk_decoder handle. */
/** @{ */

mcsdk_export double mcsdk_decoder_set_position(mcsdk_decoder d, double seconds);
mcsdk_export bool mcsdk_decoder_fill_buffer(mcsdk_decoder d, mcsdk_audio_buffer ab);
mcsdk_export double mcsdk_decoder_get_duration(mcsdk_decoder d);
mcsdk_export bool mcsdk_decoder_open(mcsdk_decoder d, mcsdk_data_stream ds);
mcsdk_export bool mcsdk_decoder_is_eof(mcsdk_decoder d);
mcsdk_export void mcsdk_decoder_release(mcsdk_decoder d);
/** @} */

/** @name IEncoder
 *  @brief Encoder functions on the mcsdk_encoder handle. */
/** @{ */

mcsdk_export mcsdk_encoder_type mcsdk_encoder_get_type(mcsdk_encoder e);
mcsdk_export void mcsdk_encoder_release(mcsdk_encoder e);
/** @} */

/** @name IBlockingEncoder
 *  @brief Blocking encoder functions on the mcsdk_blocking_encoder handle. */
/** @{ */

mcsdk_export bool mcsdk_blocking_encoder_initialize(mcsdk_blocking_encoder be, mcsdk_data_stream out, size_t rate, size_t channels, size_t bitrate);
mcsdk_export bool mcsdk_blocking_encoder_encode(mcsdk_blocking_encoder be, mcsdk_audio_buffer ab);
mcsdk_export void mcsdk_blocking_encoder_finalize(mcsdk_blocking_encoder be);
mcsdk_export void mcsdk_blocking_encoder_release(mcsdk_blocking_encoder be, mcsdk_encoder e);
/** @} */

/** @name IStreamingEncoder
 *  @brief Streaming encoder functions on the mcsdk_streaming_encoder handle. */
/** @{ */

mcsdk_export bool mcsdk_streaming_encoder_initialize(mcsdk_streaming_encoder se, size_t rate, size_t channels, size_t bitrate);
mcsdk_export int mcsdk_streaming_encoder_encode(mcsdk_streaming_encoder se, mcsdk_audio_buffer ab, char** data);
mcsdk_export int mcsdk_streaming_encoder_flush(mcsdk_streaming_encoder se, char** data);
mcsdk_export void mcsdk_streaming_encoder_finalize(mcsdk_streaming_encoder se, const char* uri);
mcsdk_export void mcsdk_streaming_encoder_release(mcsdk_streaming_encoder se);
/** @} */

/** @name IDebug
 *  @brief Debug logging functions. */
/** @{ */

mcsdk_export void mcsdk_debug_verbose(const char* tag, const char* message);
mcsdk_export void mcsdk_debug_info(const char* tag, const char* message);
mcsdk_export void mcsdk_debug_warning(const char* tag, const char* message);
mcsdk_export void mcsdk_debug_error(const char* tag, const char* message);
/** @} */

/** @name IEnvironment
 *  @brief Process-wide environment functions. */
/** @{ */

mcsdk_export size_t mcsdk_env_get_path(mcsdk_path_type type, char* dst, int size);
mcsdk_export mcsdk_data_stream mcsdk_env_open_data_stream(const char* uri, mcsdk_stream_open_flags flags);
mcsdk_export mcsdk_decoder mcsdk_env_open_decoder(mcsdk_data_stream stream);
mcsdk_export mcsdk_encoder mcsdk_env_open_encoder(const char* type) ;
mcsdk_export mcsdk_audio_buffer mcsdk_env_create_audio_buffer(size_t samples, size_t rate, size_t channels);
mcsdk_export mcsdk_prefs mcsdk_env_open_preferences(const char* name);
mcsdk_export size_t mcsdk_env_get_output_count();
mcsdk_export mcsdk_audio_output mcsdk_env_get_output_at_index(size_t index);
mcsdk_export mcsdk_audio_output mcsdk_env_get_output_with_name(const char* name);
mcsdk_export mcsdk_replay_gain_mode mcsdk_env_get_replay_gain_mode();
mcsdk_export void mcsdk_env_set_replay_gain_mode(mcsdk_replay_gain_mode mode);
mcsdk_export float mcsdk_env_get_preamp_gain();
mcsdk_export void mcsdk_env_set_preamp_gain(float gain);
mcsdk_export bool mcsdk_env_is_equalizer_enabled();
mcsdk_export void mcsdk_env_set_equalizer_enabled(bool enabled);
mcsdk_export bool mcsdk_env_get_equalizer_band_values(double target[], size_t count);
mcsdk_export bool mcsdk_env_set_equalizer_band_values(double values[], size_t count);
mcsdk_export void mcsdk_env_reload_playback_output();
mcsdk_export void mcsdk_env_set_default_output(mcsdk_audio_output output);
mcsdk_export mcsdk_audio_output mcsdk_env_get_default_output();
mcsdk_export mcsdk_transport_type mcsdk_env_get_transport_type();
mcsdk_export void mcsdk_env_set_transport_type(mcsdk_transport_type type);
/** @} */

/** @name IStream
 *  @brief Audio stream functions on the mcsdk_audio_stream handle. */
/** @{ */

mcsdk_export mcsdk_audio_stream mcsdk_audio_stream_create(int samples_per_channel, double buffer_length_seconds, mcsdk_audio_stream_flags options);
mcsdk_export mcsdk_audio_buffer mcsdk_audio_stream_get_next_buffer(mcsdk_audio_stream as);
mcsdk_export void mcsdk_audio_stream_recycle_buffer(mcsdk_audio_stream as, mcsdk_audio_buffer ab);
mcsdk_export double mcsdk_audio_stream_set_position(mcsdk_audio_stream as, double seconds);
mcsdk_export double mcsdk_audio_stream_get_duration(mcsdk_audio_stream as);
mcsdk_export bool mcsdk_audio_stream_open_uri(mcsdk_audio_stream as, const char* uri);
mcsdk_export void mcsdk_audio_stream_interrupt(mcsdk_audio_stream as);
mcsdk_export mcsdk_stream_capability mcsdk_audio_stream_get_capabilities(mcsdk_audio_stream as);
mcsdk_export bool mcsdk_audio_stream_is_eof(mcsdk_audio_stream as);
mcsdk_export void mcsdk_audio_stream_release(mcsdk_audio_stream as);
/** @} */

/** @name Player
 *  @brief Audio player functions on the mcsdk_audio_player handle. */
/** @{ */

mcsdk_export mcsdk_audio_player mcsdk_audio_player_create(const char* url, mcsdk_audio_output output, mcsdk_audio_player_callbacks* callbacks, mcsdk_audio_player_gain gain);
mcsdk_export int mcsdk_audio_player_get_url(mcsdk_audio_player ap, char* dst, int size);
mcsdk_export void mcsdk_audio_player_detach(mcsdk_audio_player ap, mcsdk_audio_player_callbacks* callbacks);
mcsdk_export void mcsdk_audio_player_attach(mcsdk_audio_player ap, mcsdk_audio_player_callbacks* callbacks);
mcsdk_export void mcsdk_audio_player_play(mcsdk_audio_player ap);
mcsdk_export double mcsdk_audio_player_get_position(mcsdk_audio_player ap);
mcsdk_export void mcsdk_audio_player_set_position(mcsdk_audio_player ap, double seconds);
mcsdk_export double mcsdk_audio_player_get_duration(mcsdk_audio_player ap);
mcsdk_export void mcsdk_audio_player_add_mix_point(mcsdk_audio_player ap, int id, double time);
mcsdk_export bool mcsdk_audio_player_has_capability(mcsdk_audio_player ap, mcsdk_stream_capability capability);
mcsdk_export mcsdk_audio_player_gain mcsdk_audio_player_get_default_gain();
mcsdk_export void mcsdk_audio_player_release(mcsdk_audio_player ap, mcsdk_audio_player_release_mode mode);
/** @} */

/** @name IIndexer
 *  @brief Indexer functions on the mcsdk_svc_indexer handle. */
/** @{ */

mcsdk_export void mcsdk_svc_indexer_add_path(mcsdk_svc_indexer in, const char* path);
mcsdk_export void mcsdk_svc_indexer_remove_path(mcsdk_svc_indexer in, const char* path);
mcsdk_export int mcsdk_svc_indexer_get_paths_count(mcsdk_svc_indexer in);
mcsdk_export int mcsdk_svc_indexer_get_paths_at(mcsdk_svc_indexer in, int index, char* dst, int len);
mcsdk_export void mcsdk_svc_indexer_schedule(mcsdk_svc_indexer in, mcsdk_svc_indexer_sync_type type);
mcsdk_export void mcsdk_svc_indexer_stop(mcsdk_svc_indexer in);
mcsdk_export mcsdk_svc_indexer_state mcsdk_svc_indexer_get_state(mcsdk_svc_indexer in);
mcsdk_export void mcsdk_svc_indexer_add_callbacks(mcsdk_svc_indexer in, mcsdk_svc_indexer_callbacks* cb);
mcsdk_export void mcsdk_svc_indexer_remove_callbacks(mcsdk_svc_indexer in, mcsdk_svc_indexer_callbacks* cb);
/** @} */

/** @name ILibrary
 *  @brief Library functions on the mcsdk_svc_library handle. */
/** @{ */

mcsdk_export void mcsdk_svc_library_run_query(mcsdk_svc_library l, const char* name, void* user_context, mcsdk_svc_library_run_query_callback cb, mcsdk_svc_library_query_flag flags);
mcsdk_export int mcsdk_svc_library_get_id(mcsdk_svc_library l);
mcsdk_export int mcsdk_svc_library_get_name(mcsdk_svc_library l, char* dst, int len);
/** @} */

/** @name Statement
 *  @brief SQL statement functions on the mcsdk_db_statement handle. */
/** @{ */

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
/** @} */

/** @name ScopedTransaction
 *  @brief SQL transaction functions on the mcsdk_db_transaction handle. */
/** @{ */

mcsdk_export mcsdk_db_transaction mcsdk_db_transaction_create(mcsdk_db_connection db);
mcsdk_export void mcsdk_db_transaction_cancel(mcsdk_db_transaction tx);
mcsdk_export void mcsdk_db_transaction_commit_and_restart(mcsdk_db_transaction tx);
mcsdk_export void mcsdk_db_transaction_release(mcsdk_db_transaction tx);
/** @} */

#endif