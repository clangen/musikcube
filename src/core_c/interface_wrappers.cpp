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

#include "musikcore_c.h"

#include <core/debug.h>

#include <core/sdk/IResource.h>
#include <core/sdk/IValue.h>
#include <core/sdk/IValueList.h>
#include <core/sdk/IMap.h>
#include <core/sdk/IMapList.h>
#include <core/sdk/ITrack.h>
#include <core/sdk/ITrackList.h>
#include <core/sdk/ITrackListEditor.h>
#include <core/sdk/IMetadataProxy.h>
#include <core/sdk/IPlaybackService.h>
#include <core/sdk/IPreferences.h>
#include <core/sdk/IDataStream.h>
#include <core/sdk/IBuffer.h>
#include <core/sdk/IBufferProvider.h>
#include <core/sdk/IDecoder.h>
#include <core/sdk/IEncoder.h>
#include <core/sdk/IBlockingEncoder.h>
#include <core/sdk/IStreamingEncoder.h>
#include <core/sdk/IDevice.h>
#include <core/sdk/IOutput.h>

using namespace musik;
using namespace musik::core::sdk;

#define RESOURCE(x) reinterpret_cast<IResource*>(x)
#define VALUE(x) reinterpret_cast<IValue*>(x)
#define MAP(x) reinterpret_cast<IMap*>(x)
#define VALUELIST(x) reinterpret_cast<IValueList*>(x)
#define MAPLIST(x) reinterpret_cast<IMapList*>(x)
#define TRACK(x) reinterpret_cast<ITrack*>(x)
#define TRACKLIST(x) reinterpret_cast<ITrackList*>(x)
#define TRACKLISTEDITOR(x) reinterpret_cast<ITrackListEditor*>(x)
#define METADATA(x) reinterpret_cast<IMetadataProxy*>(x)
#define PLAYBACK(x) reinterpret_cast<IPlaybackService*>(x)
#define PREFS(x) reinterpret_cast<IPreferences*>(x)
#define DATASTREAM(x) reinterpret_cast<IDataStream*>(x)
#define BUFFER(x) reinterpret_cast<IBuffer*>(x)
#define BUFFERPROVIDER(x) reinterpret_cast<IBufferProvider*>(x)
#define DECODER(x) reinterpret_cast<IDecoder*>(x)
#define ENCODER(x) reinterpret_cast<IEncoder*>(x)
#define STREAMINGENCODER(x) reinterpret_cast<IStreamingEncoder*>(x)
#define BLOCKINGENCODER(x) reinterpret_cast<IBlockingEncoder*>(x)
#define DEVICE(x) reinterpret_cast<IDevice*>(x)
#define DEVICELIST(x) reinterpret_cast<IDeviceList*>(x)
#define OUTPUT(x) reinterpret_cast<IOutput*>(x)

/*
 *
 * IResource
 *
 */

mcsdk_export int64_t mcsdk_resource_get_id(mcsdk_resource r) {
    return RESOURCE(r)->GetId();
}

mcsdk_export mcsdk_resource_class mcsdk_resource_get_class(mcsdk_resource r) {
    return (mcsdk_resource_class) RESOURCE(r)->GetClass();
}

mcsdk_export void mcsdk_resource_release(mcsdk_resource r) {
    RESOURCE(r)->Release();
}

/*
 *
 * IValue
 *
 */

mcsdk_export size_t mcsdk_value_get_value(mcsdk_value v, char* dst, size_t size) {
    return VALUE(v)->GetValue(dst, size);
}

mcsdk_export void mcsdk_value_release(mcsdk_value v) {
    return VALUE(v)->Release();
}

/*
 *
 * IValueList
 *
 */

mcsdk_export size_t mcsdk_value_list_count(mcsdk_value_list vl) {
    return VALUELIST(vl)->Count();
}

mcsdk_export mcsdk_value mcsdk_value_list_get_at(mcsdk_value_list vl, size_t index) {
    return (mcsdk_value) VALUELIST(vl)->GetAt(index);
}

mcsdk_export void mcsdk_value_list_release(mcsdk_value_list vl) {
    VALUELIST(vl)->Release();
}

/*
 *
 * IMap
 *
 */

mcsdk_export int mcsdk_map_get_string(mcsdk_map m, const char* key, char* dst, int size) {
    return MAP(m)->GetString(key, dst, size);
}

mcsdk_export int64_t mcsdk_map_get_int64(mcsdk_map m, const char* key, int64_t default_value) {
    return MAP(m)->GetInt64(key, default_value);
}

mcsdk_export int32_t mcsdk_map_get_int32(mcsdk_map m, const char* key, int32_t default_value) {
    return MAP(m)->GetInt32(key, default_value);
}

mcsdk_export double mcsdk_map_get_double(mcsdk_map m, const char* key, double default_value) {
    return MAP(m)->GetDouble(key, default_value);
}

mcsdk_export void mcsdk_map_release(mcsdk_map m) {
    MAP(m)->Release();
}

/*
 *
 * IMapList
 *
 */

mcsdk_export size_t mcsdk_map_list_get_count(mcsdk_map_list ml) {
    return MAPLIST(ml)->Count();
}

mcsdk_export mcsdk_map_list mcsdk_map_list_get_at(mcsdk_map_list ml, size_t index) {
    return (mcsdk_map_list) MAPLIST(ml)->GetAt(index);
}

mcsdk_export void mcsdk_map_list_release(mcsdk_map_list ml) {
    MAPLIST(ml)->Release();
}

/*
 *
 * ITrack
 *
 */

mcsdk_export void mcsdk_track_retain(mcsdk_track t) {
    TRACK(t)->Retain();
}

mcsdk_export int mcsdk_track_get_uri(mcsdk_track t, char* dst, int size) {
    return TRACK(t)->Uri(dst, size);
}

mcsdk_export void mcsdk_track_release(mcsdk_track t) {
    TRACK(t)->Release();
}

/*
 *
 * ITrackList
 *
 */

mcsdk_export size_t mcsdk_track_list_get_count(mcsdk_track_list tl) {
    return TRACKLIST(tl)->Count();
}

mcsdk_export int64_t mcsdk_track_list_get_id(mcsdk_track_list tl, size_t index) {
    return TRACKLIST(tl)->GetId(index);
}

mcsdk_export int64_t mcsdk_track_list_index_of(mcsdk_track_list tl, int64_t id) {
    return TRACKLIST(tl)->IndexOf(id);
}

mcsdk_export mcsdk_track mcsdk_track_list_get_track_at(mcsdk_track_list tl, size_t index) {
    return (mcsdk_track) TRACKLIST(tl)->GetTrack(index);
}

mcsdk_export void mcsdk_track_list_release(mcsdk_track_list tl) {
    TRACKLIST(tl)->Release();
}

/*
 *
 * ITrackListEditor
 *
 */

mcsdk_export bool mcsdk_track_list_editor_insert(mcsdk_track_list_editor tle, int64_t id, size_t index) {
    return TRACKLISTEDITOR(tle)->Insert(id, index);
}

mcsdk_export bool mcsdk_track_list_editor_swap(mcsdk_track_list_editor tle, size_t index1, size_t index2) {
    return TRACKLISTEDITOR(tle)-> Swap(index1, index2);
}

mcsdk_export bool mcsdk_track_list_editor_move(mcsdk_track_list_editor tle, size_t from, size_t to) {
    return TRACKLISTEDITOR(tle)->Move(from, to);
}

mcsdk_export bool mcsdk_track_list_editor_delete(mcsdk_track_list_editor tle, size_t index) {
    return TRACKLISTEDITOR(tle)->Delete(index);
}

mcsdk_export void mcsdk_track_list_editor_add(mcsdk_track_list_editor tle, const int64_t id) {
    TRACKLISTEDITOR(tle)->Add(id);
}

mcsdk_export void mcsdk_track_list_editor_clear(mcsdk_track_list_editor tle) {
    TRACKLISTEDITOR(tle)->Clear();
}

mcsdk_export void mcsdk_track_list_editor_shuffle(mcsdk_track_list_editor tle) {
    TRACKLISTEDITOR(tle)->Shuffle();
}

mcsdk_export void mcsdk_track_list_editor_release(mcsdk_track_list_editor tle) {
    TRACKLISTEDITOR(tle)->Release();
}

/*
 *
 * IMetadataProxy
 *
 */

mcsdk_export mcsdk_track_list mcsdk_svc_metadata_query_tracks(mcsdk_svc_metadata mp, const char* keyword, int limit, int offset) {
    return (mcsdk_track_list) METADATA(mp)->QueryTracks(keyword, limit, offset);
}

mcsdk_export mcsdk_track_list mcsdk_svc_metadata_query_track_by_id(mcsdk_svc_metadata mp, int64_t track_id) {
    return (mcsdk_track) METADATA(mp)->QueryTrackById(track_id);
}

mcsdk_export mcsdk_track_list mcsdk_svc_metadata_query_track_by_external_id(mcsdk_svc_metadata mp, const char* external_id) {
    return (mcsdk_track) METADATA(mp)->QueryTrackByExternalId(external_id);
}

mcsdk_export mcsdk_track_list mcsdk_svc_metadata_query_tracks_by_category(mcsdk_svc_metadata mp, const char* category_type, int64_t selected_id, const char* filter, int limit, int offset) {
    return (mcsdk_track_list) METADATA(mp)->QueryTracksByCategory(category_type, selected_id, filter, limit, offset);
}

mcsdk_export mcsdk_track_list mcsdk_svc_metadata_query_tracks_by_categories(mcsdk_svc_metadata mp, mcsdk_value* categories, size_t category_count, const char* filter, int limit, int offset) {
    return (mcsdk_track_list) METADATA(mp)->QueryTracksByCategories(reinterpret_cast<IValue**>(categories), category_count, filter, limit, offset);
}

mcsdk_export mcsdk_track_list mcsdk_svc_metadata_query_tracks_by_external_id(mcsdk_svc_metadata mp, const char** external_ids, size_t external_id_count) {
    return (mcsdk_track_list) METADATA(mp)->QueryTracksByExternalId(external_ids, external_id_count);
}

mcsdk_export mcsdk_value_list mcsdk_svc_metadata_list_categories(mcsdk_svc_metadata mp) {
    return (mcsdk_value_list) METADATA(mp)->ListCategories();
}

mcsdk_export mcsdk_value_list mcsdk_svc_metadata_query_category(mcsdk_svc_metadata mp, const char* type, const char* filter) {
    return (mcsdk_value_list) METADATA(mp)->QueryCategory(type, filter);
}

mcsdk_export mcsdk_value_list mcsdk_svc_metadata_query_category_with_predicate(mcsdk_svc_metadata mp, const char* type, const char* predicate_type, int64_t predicate_id, const char* filter) {
    return (mcsdk_value_list) METADATA(mp)->QueryCategoryWithPredicate(type, predicate_type, predicate_id, filter);
}

mcsdk_export mcsdk_value_list mcsdk_svc_metadata_query_category_with_predicates(mcsdk_svc_metadata mp, const char* type, mcsdk_value* predicates, size_t predicate_count, const char* filter) {
    return (mcsdk_value_list) METADATA(mp)->QueryCategoryWithPredicates(type, reinterpret_cast<IValue**>(predicates), predicate_count, filter);
}

mcsdk_export mcsdk_map_list mcsdk_svc_metadata_query_albums(mcsdk_svc_metadata mp, const char* filter) {
    return (mcsdk_map_list) METADATA(mp)->QueryAlbums(filter);
}

mcsdk_export mcsdk_map_list mcsdk_svc_metadata_query_albums_by_category(mcsdk_svc_metadata mp, const char* category_id_name, int64_t category_id_value, const char* filter) {
    return (mcsdk_map_list) METADATA(mp)->QueryAlbums(category_id_name, category_id_value, filter);
}

mcsdk_export int64_t mcsdk_svc_metadata_save_playlist_with_ids(mcsdk_svc_metadata mp, int64_t* track_ids, size_t track_id_count, const char* playlist_name, const int64_t playlist_id) {
    return METADATA(mp)->SavePlaylistWithIds(track_ids, track_id_count, playlist_name, playlist_id);
}

mcsdk_export int64_t mcsdk_svc_metadata_save_playlist_with_external_ids(mcsdk_svc_metadata mp, const char** external_ids, size_t external_id_count, const char* playlist_name, const int64_t playlist_id) {
    return METADATA(mp)->SavePlaylistWithExternalIds(external_ids, external_id_count, playlist_name, playlist_id);
}

mcsdk_export int64_t mcsdk_svc_metadata_save_playlist_with_track_list(mcsdk_svc_metadata mp, mcsdk_track_list track_list, const char* playlist_name, const int64_t playlist_id) {
    return METADATA(mp)->SavePlaylistWithTrackList(TRACKLIST(track_list), playlist_name, playlist_id);
}

mcsdk_export bool mcsdk_svc_metadata_rename_playlist(mcsdk_svc_metadata mp, const int64_t playlist_id, const char* playlist_name) {
    return METADATA(mp)->RenamePlaylist(playlist_id, playlist_name);
}

mcsdk_export bool mcsdk_svc_metadata_delete_playlist(mcsdk_svc_metadata mp, const int64_t playlist_id) {
    return METADATA(mp)->DeletePlaylist(playlist_id);
}

mcsdk_export bool mcsdk_svc_metadata_append_to_playlist_with_ids(mcsdk_svc_metadata mp, const int64_t playlist_id, const int64_t* track_ids, size_t track_id_count, int offset) {
    return METADATA(mp)->AppendToPlaylistWithIds(playlist_id, track_ids, track_id_count, offset);
}

mcsdk_export bool mcsdk_svc_metadata_append_to_playlist_with_external_ids(mcsdk_svc_metadata mp, const int64_t playlist_id, const char** external_track_ids, size_t external_track_id_count, int offset) {
    return METADATA(mp)->AppendToPlaylistWithExternalIds(playlist_id, external_track_ids, external_track_id_count, offset);
}

mcsdk_export bool mcsdk_svc_metadata_append_to_playlist_with_track_list(mcsdk_svc_metadata mp, const int64_t playlist_id, mcsdk_track_list track_list, int offset) {
    return METADATA(mp)->AppendToPlaylistWithTrackList(playlist_id, TRACKLIST(track_list), offset);
}

mcsdk_export size_t mcsdk_svc_metadata_remove_tracks_from_playlist(mcsdk_svc_metadata mp, const int64_t playlist_id, const char** external_ids, const int* sort_orders, int count) {
    return METADATA(mp)->RemoveTracksFromPlaylist(playlist_id, external_ids, sort_orders, count);
}

mcsdk_export void mcsdk_svc_metadata_release(mcsdk_svc_metadata mp) {
    METADATA(mp)->Release();
}

/*
 *
 * IPlaybackService
 *
 */

mcsdk_export void mcsdk_svc_playback_play_at(mcsdk_svc_playback pb, size_t index) {
    PLAYBACK(pb)->Play(index);
}

mcsdk_export bool mcsdk_svc_playback_next(mcsdk_svc_playback pb) {
    return PLAYBACK(pb)->Next();
}

mcsdk_export bool mcsdk_svc_playback_previous(mcsdk_svc_playback pb) {
    return PLAYBACK(pb)->Previous();
}

mcsdk_export void mcsdk_svc_playback_stop(mcsdk_svc_playback pb) {
    PLAYBACK(pb)->Stop();
}

mcsdk_export mcsdk_repeat_mode mcsdk_svc_playback_get_repeat_mode(mcsdk_svc_playback pb) {
    return (mcsdk_repeat_mode) PLAYBACK(pb)->GetRepeatMode();
}

mcsdk_export void mcsdk_svc_playback_set_repeat_mode(mcsdk_svc_playback pb, mcsdk_repeat_mode mode) {
    PLAYBACK(pb)->SetRepeatMode((RepeatMode) mode);
}

mcsdk_export void mcsdk_svc_playback_toggle_repeat_mode(mcsdk_svc_playback pb) {
    PLAYBACK(pb)->ToggleRepeatMode();
}

mcsdk_export mcsdk_playback_state mcsdk_svc_playback_get_playback_state(mcsdk_svc_playback pb) {
    return (mcsdk_playback_state) PLAYBACK(pb)->GetPlaybackState();
}

mcsdk_export bool mcsdk_svc_playback_is_shuffled(mcsdk_svc_playback pb) {
    return PLAYBACK(pb)->IsShuffled();
}

mcsdk_export void mcsdk_svc_playback_toggle_shuffle(mcsdk_svc_playback pb) {
    PLAYBACK(pb)->ToggleShuffle();
}

mcsdk_export void mcsdk_svc_playback_pause_or_resume(mcsdk_svc_playback pb) {
    PLAYBACK(pb)->PauseOrResume();
}

mcsdk_export double mcsdk_svc_playback_get_volume(mcsdk_svc_playback pb) {
    return PLAYBACK(pb)->GetVolume();
}

mcsdk_export void mcsdk_svc_playback_set_volume(mcsdk_svc_playback pb, double volume) {
    PLAYBACK(pb)->SetVolume(volume);
}

mcsdk_export double mcsdk_svc_playback_get_position(mcsdk_svc_playback pb) {
    return PLAYBACK(pb)->GetPosition();
}

mcsdk_export void mcsdk_svc_playback_set_position(mcsdk_svc_playback pb, double seconds) {
    PLAYBACK(pb)->SetPosition(seconds);
}

mcsdk_export double mcsdk_svc_playback_get_duration(mcsdk_svc_playback pb) {
    return PLAYBACK(pb)->GetDuration();
}

mcsdk_export bool mcsdk_svc_playback_is_muted(mcsdk_svc_playback pb) {
    return PLAYBACK(pb)->IsMuted();
}

mcsdk_export void mcsdk_svc_playback_toggle_mute(mcsdk_svc_playback pb) {
    PLAYBACK(pb)->ToggleMute();
}

mcsdk_export size_t mcsdk_svc_playback_get_index(mcsdk_svc_playback pb) {
    return PLAYBACK(pb)->GetIndex();
}

mcsdk_export size_t mcsdk_svc_playback_count(mcsdk_svc_playback pb) {
    return PLAYBACK(pb)->Count();
}

mcsdk_export mcsdk_track mcsdk_svc_playback_get_track(mcsdk_svc_playback pb, size_t index) {
    return (mcsdk_track) PLAYBACK(pb)->GetTrack(index);
}

mcsdk_export mcsdk_track mcsdk_svc_playback_get_playing_track(mcsdk_svc_playback pb) {
    return (mcsdk_track) PLAYBACK(pb)->GetPlayingTrack();
}

mcsdk_export void mcsdk_svc_playback_copy_from(mcsdk_svc_playback pb, const mcsdk_track_list track_list) {
    PLAYBACK(pb)->CopyFrom(TRACKLIST(track_list));
}

mcsdk_export void mcsdk_svc_playback_play(mcsdk_svc_playback pb, const mcsdk_track_list source, size_t index) {
    PLAYBACK(pb)->Play(TRACKLIST(source), index);
}

mcsdk_export mcsdk_track_list_editor mcsdk_svc_playback_edit_playlist(mcsdk_svc_playback pb) {
    return (mcsdk_track_list_editor) PLAYBACK(pb)->EditPlaylist();
}

mcsdk_export mcsdk_time_change_mode mcsdk_svc_playback_get_time_change_mode(mcsdk_svc_playback pb) {
    return (mcsdk_time_change_mode) PLAYBACK(pb)->GetTimeChangeMode();
}

mcsdk_export void mcsdk_svc_playback_set_time_change_mode(mcsdk_svc_playback pb, mcsdk_time_change_mode mode) {
    PLAYBACK(pb)->SetTimeChangeMode((TimeChangeMode) mode);
}

mcsdk_export void mcsdk_svc_playback_reload_output(mcsdk_svc_playback pb) {
    PLAYBACK(pb)->ReloadOutput();
}

mcsdk_export mcsdk_track_list mcsdk_svc_playback_clone(mcsdk_svc_playback pb) {
    return (mcsdk_track_list) PLAYBACK(pb)->Clone();
}

/*
 *
 * IPreferences
 *
 */

mcsdk_export bool mcsdk_prefs_get_bool(mcsdk_prefs p, const char* key, bool defaultValue) {
    return PREFS(p)->GetBool(key, defaultValue);
}

mcsdk_export int mcsdk_prefs_get_int(mcsdk_prefs p, const char* key, int defaultValue) {
    return PREFS(p)->GetInt(key, defaultValue);
}

mcsdk_export double mcsdk_prefs_get_double(mcsdk_prefs p, const char* key, double defaultValue) {
    return PREFS(p)->GetDouble(key, defaultValue);
}

mcsdk_export int mcsdk_prefs_get_string(mcsdk_prefs p, const char* key, char* dst, size_t size, const char* defaultValue) {
    return PREFS(p)->GetString(key, dst, size, defaultValue);
}

mcsdk_export void mcsdk_prefs_set_int(mcsdk_prefs p, const char* key, int value) {
    PREFS(p)->SetInt(key, value);
}

mcsdk_export void mcsdk_prefs_set_double(mcsdk_prefs p, const char* key, double value) {
    PREFS(p)->SetDouble(key, value);
}

mcsdk_export void mcsdk_prefs_set_string(mcsdk_prefs p, const char* key, const char* value) {
    PREFS(p)->SetString(key, value);
}

mcsdk_export void mcsdk_prefs_save(mcsdk_prefs p) {
    PREFS(p)->Save();
}

mcsdk_export void mcsdk_prefs_release(mcsdk_prefs p) {
    PREFS(p)->Release();
}

/*
 *
 * IDataStream
 *
 */

mcsdk_export bool mcsdk_data_stream_open(mcsdk_data_stream ds, const char *uri, mcsdk_stream_open_flags flags) {
    return DATASTREAM(ds)->Open(uri, (OpenFlags) flags);
}

mcsdk_export bool mcsdk_data_stream_close(mcsdk_data_stream ds) {
    return DATASTREAM(ds)->Close();
}

mcsdk_export void mcsdk_data_stream_interrupt(mcsdk_data_stream ds) {
    DATASTREAM(ds)->Interrupt();
}

mcsdk_export bool mcsdk_data_stream_is_readable(mcsdk_data_stream ds) {
    return DATASTREAM(ds)->Readable();
}

mcsdk_export bool mcsdk_data_stream_is_writable(mcsdk_data_stream ds) {
    return DATASTREAM(ds)->Writable();
}

mcsdk_export long mcsdk_data_stream_read(mcsdk_data_stream ds, void* dst, long count) {
    return DATASTREAM(ds)->Read(dst, count);
}

mcsdk_export long mcsdk_data_stream_write(mcsdk_data_stream ds, void* src, long count) {
    return DATASTREAM(ds)->Write(src, count);
}

mcsdk_export bool mcsdk_data_stream_set_position(mcsdk_data_stream ds, long position) {
    return DATASTREAM(ds)->SetPosition(position);
}

mcsdk_export long mcsdk_data_stream_get_position(mcsdk_data_stream ds) {
    return DATASTREAM(ds)->Position();
}

mcsdk_export bool mcsdk_data_stream_is_seekable(mcsdk_data_stream ds) {
    return DATASTREAM(ds)->Seekable();
}

mcsdk_export bool mcsdk_data_stream_is_eof(mcsdk_data_stream ds) {
    return DATASTREAM(ds)->Eof();
}

mcsdk_export long mcsdk_data_stream_get_length(mcsdk_data_stream ds) {
    return DATASTREAM(ds)->Length();
}

mcsdk_export const char* mcsdk_data_stream_get_type(mcsdk_data_stream ds) {
    return DATASTREAM(ds)->Type();
}

mcsdk_export const char* mcsdk_data_stream_get_uri(mcsdk_data_stream ds) {
    return DATASTREAM(ds)->Uri();
}

mcsdk_export bool mcsdk_data_stream_can_prefetch(mcsdk_data_stream ds) {
    return DATASTREAM(ds)->CanPrefetch();
}

mcsdk_export void mcsdk_data_stream_release(mcsdk_data_stream ds) {
    DATASTREAM(ds)->Release();
}

/*
 *
 * IBuffer
 *
 */

mcsdk_export long mcsdk_audio_buffer_get_sample_rate(mcsdk_audio_buffer ab) {
    return BUFFER(ab)->SampleRate();
}

mcsdk_export void mcsdk_audio_buffer_set_sample_rate(mcsdk_audio_buffer ab, long sample_rate) {
    BUFFER(ab)->SetSampleRate(sample_rate);
}

mcsdk_export int mcsdk_audio_buffer_get_channels(mcsdk_audio_buffer ab) {
    return BUFFER(ab)->Channels();
}

mcsdk_export void mcsdk_audio_buffer_set_channels(mcsdk_audio_buffer ab, int channels) {
    BUFFER(ab)->SetChannels(channels);
}

mcsdk_export float* mcsdk_audio_buffer_get_buffer_pointer(mcsdk_audio_buffer ab) {
    return BUFFER(ab)->BufferPointer();
}

mcsdk_export long mcsdk_audio_buffer_get_sample_count(mcsdk_audio_buffer ab) {
    return BUFFER(ab)->Samples();
}

mcsdk_export void mcsdk_audio_buffer_set_sample_count(mcsdk_audio_buffer ab, long samples) {
    BUFFER(ab)->SetSamples(samples);
}

mcsdk_export long mcsdk_audio_buffer_get_byte_count(mcsdk_audio_buffer ab) {
    return BUFFER(ab)->Bytes();
}

mcsdk_export void mcsdk_audio_buffer_release(mcsdk_audio_buffer ab) {
    BUFFER(ab)->Release();
}

/*
 *
 * IBufferProvider
 *
 */

mcsdk_export void mcsdk_audio_buffer_provider_notify_processed(mcsdk_audio_buffer_provider abp, mcsdk_audio_buffer ab) {
    BUFFERPROVIDER(abp)->OnBufferProcessed(BUFFER(ab));
}

/*
 *
 * IDevice
 *
 */

mcsdk_export const char* mcsdk_device_get_name(mcsdk_device d) {
    return DEVICE(d)->Name();
}

mcsdk_export const char* mcsdk_device_get_id(mcsdk_device d) {
    return DEVICE(d)->Id();
}

mcsdk_export void mcsdk_device_release(mcsdk_device d) {
    DEVICE(d)->Release();
}


/*
 *
 * IDeviceList
 *
 */

mcsdk_export size_t mcsdk_device_list_get_count(mcsdk_device_list dl) {
    return DEVICELIST(dl)->Count();
}

mcsdk_export const mcsdk_device mcsdk_device_list_get_at(mcsdk_device_list dl, size_t index) {
    return (mcsdk_device) DEVICELIST(dl)->At(index);
}

mcsdk_export void mcsdk_device_list_release(mcsdk_device_list dl) {
    DEVICELIST(dl)->Release();
}


/*
 *
 * IOutput
 *
 */

mcsdk_export void mcsdk_output_pause(mcsdk_output o) {
    OUTPUT(o)->Pause();
}

mcsdk_export void mcsdk_output_resume(mcsdk_output o) {
    OUTPUT(o)->Resume();
}

mcsdk_export void mcsdk_output_set_volume(mcsdk_output o, double volume) {
    OUTPUT(o)->SetVolume(volume);
}

mcsdk_export double mcsdk_output_get_volume(mcsdk_output o) {
    return OUTPUT(o)->GetVolume();
}

mcsdk_export void mcsdk_output_stop(mcsdk_output o) {
    OUTPUT(o)->Stop();
}

mcsdk_export int mcsdk_output_play(mcsdk_output o, mcsdk_audio_buffer ab, mcsdk_audio_buffer_provider abp) {
    return OUTPUT(o)->Play(BUFFER(ab), BUFFERPROVIDER(abp));
}

mcsdk_export void mcsdk_output_drain(mcsdk_output o) {
    OUTPUT(o)->Drain();
}

mcsdk_export double mcsdk_output_get_latency(mcsdk_output o) {
    return OUTPUT(o)->Latency();
}

mcsdk_export const char* mcsdk_output_get_name(mcsdk_output o) {
    return OUTPUT(o)->Name();
}

mcsdk_export mcsdk_device_list mcsdk_output_get_device_list(mcsdk_output o) {
    return (mcsdk_device_list) OUTPUT(o)->GetDeviceList();
}

mcsdk_export bool mcsdk_output_set_default_device(mcsdk_output o, const char* device_id) {
    return OUTPUT(o)->SetDefaultDevice(device_id);
}

mcsdk_export mcsdk_device mcsdk_output_get_default_device(mcsdk_output o) {
    return (mcsdk_device) OUTPUT(o)->GetDefaultDevice();
}

mcsdk_export void mcsdk_output_release(mcsdk_output o) {
    OUTPUT(o)->Release();
}

/*
 *
 * IDecoder
 *
 */

mcsdk_export double mcsdk_decoder_set_position(mcsdk_decoder d, double seconds) {
    return DECODER(d)->SetPosition(seconds);
}

mcsdk_export bool mcsdk_decoder_fill_buffer(mcsdk_decoder d, mcsdk_audio_buffer ab) {
    return DECODER(d)->GetBuffer(BUFFER(ab));
}

mcsdk_export double mcsdk_decoder_get_duration(mcsdk_decoder d) {
    return DECODER(d)->GetDuration();
}

mcsdk_export bool mcsdk_decoder_open(mcsdk_decoder d, mcsdk_data_stream ds) {
    return DECODER(d)->Open(DATASTREAM(ds));
}

mcsdk_export bool mcsdk_decoder_is_eof(mcsdk_decoder d) {
    return DECODER(d)->Exhausted();
}

mcsdk_export void mcsdk_decoder_release(mcsdk_decoder d) {
    DECODER(d)->Release();
}

/*
 *
 * IEncoder
 *
 */

mcsdk_export void mcsdk_encoder_release(mcsdk_encoder e) {
    ENCODER(e)->Release();
}

mcsdk_export mcsdk_encoder_type mcsdk_encoder_get_type(mcsdk_encoder e) {
    IEncoder* encoder = reinterpret_cast<IEncoder*>(e);
    if (dynamic_cast<IBlockingEncoder*>(encoder) != nullptr) {
        return mcsdk_encoder_type_blocking;
    }
    return mcsdk_encoder_type_streaming;
}

/*
 *
 * IBlockingEncoder
 *
 */

mcsdk_export bool mcsdk_blocking_encoder_initialize(mcsdk_blocking_encoder be, mcsdk_data_stream out, size_t rate, size_t channels, size_t bitrate) {
    return BLOCKINGENCODER(be)->Initialize(DATASTREAM(out), rate, channels, bitrate);
}

mcsdk_export bool mcsdk_blocking_encoder_encode(mcsdk_blocking_encoder be, mcsdk_audio_buffer ab) {
    return BLOCKINGENCODER(be)->Encode(BUFFER(ab));
}

mcsdk_export void mcsdk_blocking_encoder_finalize(mcsdk_blocking_encoder be) {
    BLOCKINGENCODER(be)->Finalize();
}

mcsdk_export void mcsdk_blocking_encoder_release(mcsdk_blocking_encoder be, mcsdk_encoder e) {
    BLOCKINGENCODER(be)->Release();
}

/*
 *
 * IStreamingEncoder
 *
 */

mcsdk_export bool mcsdk_streaming_encoder_initialize(mcsdk_streaming_encoder se, size_t rate, size_t channels, size_t bitrate) {
    return STREAMINGENCODER(se)->Initialize(rate, channels, bitrate);
}

mcsdk_export int mcsdk_streaming_encoder_encode(mcsdk_streaming_encoder se, mcsdk_audio_buffer ab, char** data) {
    return STREAMINGENCODER(se)->Encode(BUFFER(ab), data);
}

mcsdk_export int mcsdk_streaming_encoder_flush(mcsdk_streaming_encoder se, char** data) {
    return STREAMINGENCODER(se)->Flush(data);
}

mcsdk_export void mcsdk_streaming_encoder_finalize(mcsdk_streaming_encoder se, const char* uri) {
    STREAMINGENCODER(se)->Finalize(uri);
}

mcsdk_export void mcsdk_streaming_encoder_release(mcsdk_streaming_encoder se, mcsdk_encoder e) {
    STREAMINGENCODER(se)->Release();
}

/*
 *
 * IDebug
 *
 */

mcsdk_export void mcsdk_debug_verbose(const char* tag, const char* message) {
    debug::verbose(tag, message);
}

mcsdk_export void mcsdk_debug_info(const char* tag, const char* message) {
    debug::info(tag, message);
}

mcsdk_export void mcsdk_debug_warning(const char* tag, const char* message) {
    debug::warning(tag, message);
}

mcsdk_export void mcsdk_debug_error(const char* tag, const char* message) {
    debug::error(tag, message);
}
