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

#include <pch.hpp>

#include <core/musikcore_c.h>
#include <core/c_context.h>

#include <core/debug.h>
#include <core/plugin/Plugins.h>
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
#include <core/library/query/local/LocalQueryBase.h>
#include <core/library/ILibrary.h>
#include <core/library/IIndexer.h>
#include <core/library/track/TrackList.h>
#include <core/audio/Stream.h>
#include <core/audio/Player.h>
#include <core/db/ScopedTransaction.h>
#include <core/db/Connection.h>
#include <core/db/Statement.h>
#include <core/support/Common.h>

#include <string>

using namespace musik;
using namespace musik::core;
using namespace musik::core::db;
using namespace musik::core::sdk;
using namespace musik::core::audio;

#define ENV musik::core::plugin::Environment()

#define RESOURCE(x) static_cast<IResource*>(x.opaque)
#define VALUE(x) static_cast<IValue*>(x.opaque)
#define MAP(x) static_cast<IMap*>(x.opaque)
#define VALUELIST(x) static_cast<IValueList*>(x.opaque)
#define MAPLIST(x) static_cast<IMapList*>(x.opaque)
#define TRACK(x) static_cast<ITrack*>(x.opaque)
#define TRACKLIST(x) static_cast<ITrackList*>(x.opaque)
#define TRACKLISTEDITOR(x) static_cast<ITrackListEditor*>(x.opaque)
#define METADATA(x) static_cast<IMetadataProxy*>(x.opaque)
#define PLAYBACK(x) static_cast<IPlaybackService*>(x.opaque)
#define PREFS(x) static_cast<IPreferences*>(x.opaque)
#define DATASTREAM(x) static_cast<IDataStream*>(x.opaque)
#define BUFFER(x) static_cast<IBuffer*>(x.opaque)
#define BUFFERPROVIDER(x) static_cast<IBufferProvider*>(x.opaque)
#define DECODER(x) static_cast<IDecoder*>(x.opaque)
#define ENCODER(x) static_cast<IEncoder*>(x.opaque)
#define STREAMINGENCODER(x) static_cast<IStreamingEncoder*>(x.opaque)
#define BLOCKINGENCODER(x) static_cast<IBlockingEncoder*>(x.opaque)
#define DEVICE(x) static_cast<IDevice*>(x.opaque)
#define DEVICELIST(x) static_cast<IDeviceList*>(x.opaque)
#define OUTPUT(x) static_cast<IOutput*>(x.opaque)
#define AUDIOSTREAM(x) static_cast<IStream*>(x.opaque)
#define PLAYER(x) static_cast<mcsdk_player_context_internal*>(x.opaque)
#define INDEXER(x) static_cast<mcsdk_svc_indexer_context_internal*>(x.opaque)->indexer
#define LIBRARY(x) static_cast<ILibrary*>(x.opaque)
#define DBCONNECTION(x) static_cast<Connection*>(x.opaque)
#define DBSTATEMENT(x) static_cast<Statement*>(x.opaque)
#define DBTRANSACTION(x) static_cast<ScopedTransaction*>(x.opaque)

#define RELEASE(x, type) if (mcsdk_handle_ok(x)) { type(x)->Release(); x.opaque = nullptr; }

/*
 * IResource
 */

mcsdk_export int64_t mcsdk_resource_get_id(mcsdk_resource r) {
    return RESOURCE(r)->GetId();
}

mcsdk_export mcsdk_resource_class mcsdk_resource_get_class(mcsdk_resource r) {
    return (mcsdk_resource_class) RESOURCE(r)->GetClass();
}

mcsdk_export void mcsdk_resource_release(mcsdk_resource r) {
    RELEASE(r, RESOURCE);
}

/*
 * IValue
 */

mcsdk_export size_t mcsdk_value_get_value(mcsdk_value v, char* dst, size_t size) {
    return VALUE(v)->GetValue(dst, size);
}

mcsdk_export void mcsdk_value_release(mcsdk_value v) {
    RELEASE(v, VALUE);
}

/*
 * IValueList
 */

mcsdk_export size_t mcsdk_value_list_count(mcsdk_value_list vl) {
    return VALUELIST(vl)->Count();
}

mcsdk_export mcsdk_value mcsdk_value_list_get_at(mcsdk_value_list vl, size_t index) {
    return mcsdk_value { VALUELIST(vl)->GetAt(index) };
}

mcsdk_export void mcsdk_value_list_release(mcsdk_value_list vl) {
    RELEASE(vl, VALUELIST);
}

/*
 * IMap
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
    RELEASE(m, MAP);
}

/*
 * IMapList
 */

mcsdk_export size_t mcsdk_map_list_get_count(mcsdk_map_list ml) {
    return MAPLIST(ml)->Count();
}

mcsdk_export mcsdk_map mcsdk_map_list_get_at(mcsdk_map_list ml, size_t index) {
    return mcsdk_map { MAPLIST(ml)->GetAt(index) };
}

mcsdk_export void mcsdk_map_list_release(mcsdk_map_list ml) {
    RELEASE(ml, MAPLIST);
}

/*
 * ITrack
 */

mcsdk_export void mcsdk_track_retain(mcsdk_track t) {
    TRACK(t)->Retain();
}

mcsdk_export int mcsdk_track_get_uri(mcsdk_track t, char* dst, int size) {
    return TRACK(t)->Uri(dst, size);
}

mcsdk_export void mcsdk_track_release(mcsdk_track t) {
    RELEASE(t, TRACK);
}

/*
 * TrackList
 */

mcsdk_export mcsdk_track_list mcsdk_track_list_create(mcsdk_context* context) {
    auto internal = (mcsdk_context_internal*) context->internal.opaque;
    return mcsdk_track_list { new TrackList(internal->library) };
}

mcsdk_export bool mcsdk_track_list_can_edit(mcsdk_track_list tl) {
    return dynamic_cast<TrackList*>(TRACKLIST(tl)) != nullptr;
}

mcsdk_export mcsdk_track_list_editor mcsdk_track_list_edit(mcsdk_track_list tl) {
    auto trackList = static_cast<TrackList*>(tl.opaque);
    auto trackListPtr = std::shared_ptr<TrackList>(trackList, [](TrackList*){});
    return mcsdk_track_list_editor { new TrackListEditor(trackListPtr) };
}

/*
 * ITrackList
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
    return mcsdk_track { TRACKLIST(tl)->GetTrack(index) };
}

mcsdk_export void mcsdk_track_list_release(mcsdk_track_list tl) {
    RELEASE(tl, TRACKLIST);
}

/*
 * ITrackListEditor
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
    RELEASE(tle, TRACKLISTEDITOR);
}

/*
 * IMetadataProxy
 */

mcsdk_export mcsdk_track_list mcsdk_svc_metadata_query_tracks(mcsdk_svc_metadata mp, const char* keyword, int limit, int offset) {
    return mcsdk_track_list { METADATA(mp)->QueryTracks(keyword, limit, offset) };
}

mcsdk_export mcsdk_track mcsdk_svc_metadata_query_track_by_id(mcsdk_svc_metadata mp, int64_t track_id) {
    return mcsdk_track { METADATA(mp)->QueryTrackById(track_id) };
}

mcsdk_export mcsdk_track mcsdk_svc_metadata_query_track_by_external_id(mcsdk_svc_metadata mp, const char* external_id) {
    return mcsdk_track { METADATA(mp)->QueryTrackByExternalId(external_id) };
}

mcsdk_export mcsdk_track_list mcsdk_svc_metadata_query_tracks_by_category(mcsdk_svc_metadata mp, const char* category_type, int64_t selected_id, const char* filter, int limit, int offset) {
    return mcsdk_track_list { METADATA(mp)->QueryTracksByCategory(category_type, selected_id, filter, limit, offset) };
}

mcsdk_export mcsdk_track_list mcsdk_svc_metadata_query_tracks_by_categories(mcsdk_svc_metadata mp, mcsdk_value* categories, size_t category_count, const char* filter, int limit, int offset) {
    return mcsdk_track_list { METADATA(mp)->QueryTracksByCategories(reinterpret_cast<IValue**>(categories), category_count, filter, limit, offset) };
}

mcsdk_export mcsdk_track_list mcsdk_svc_metadata_query_tracks_by_external_id(mcsdk_svc_metadata mp, const char** external_ids, size_t external_id_count) {
    return mcsdk_track_list { METADATA(mp)->QueryTracksByExternalId(external_ids, external_id_count) };
}

mcsdk_export mcsdk_value_list mcsdk_svc_metadata_list_categories(mcsdk_svc_metadata mp) {
    return mcsdk_value_list { METADATA(mp)->ListCategories() };
}

mcsdk_export mcsdk_value_list mcsdk_svc_metadata_query_category(mcsdk_svc_metadata mp, const char* type, const char* filter) {
    return mcsdk_value_list { METADATA(mp)->QueryCategory(type, filter) };
}

mcsdk_export mcsdk_value_list mcsdk_svc_metadata_query_category_with_predicate(mcsdk_svc_metadata mp, const char* type, const char* predicate_type, int64_t predicate_id, const char* filter) {
    return mcsdk_value_list { METADATA(mp)->QueryCategoryWithPredicate(type, predicate_type, predicate_id, filter) };
}

mcsdk_export mcsdk_value_list mcsdk_svc_metadata_query_category_with_predicates(mcsdk_svc_metadata mp, const char* type, mcsdk_value* predicates, size_t predicate_count, const char* filter) {
    return mcsdk_value_list { METADATA(mp)->QueryCategoryWithPredicates(type, reinterpret_cast<IValue**>(predicates), predicate_count, filter) };
}

mcsdk_export mcsdk_map_list mcsdk_svc_metadata_query_albums(mcsdk_svc_metadata mp, const char* filter) {
    return mcsdk_map_list { METADATA(mp)->QueryAlbums(filter) };
}

mcsdk_export mcsdk_map_list mcsdk_svc_metadata_query_albums_by_category(mcsdk_svc_metadata mp, const char* category_id_name, int64_t category_id_value, const char* filter) {
    return mcsdk_map_list { METADATA(mp)->QueryAlbums(category_id_name, category_id_value, filter) };
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
    RELEASE(mp, METADATA);
}

/*
 * IPlaybackService
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
    return mcsdk_track { PLAYBACK(pb)->GetTrack(index) };
}

mcsdk_export mcsdk_track mcsdk_svc_playback_get_playing_track(mcsdk_svc_playback pb) {
    return mcsdk_track { PLAYBACK(pb)->GetPlayingTrack() };
}

mcsdk_export void mcsdk_svc_playback_copy_from(mcsdk_svc_playback pb, const mcsdk_track_list track_list) {
    PLAYBACK(pb)->CopyFrom(TRACKLIST(track_list));
}

mcsdk_export void mcsdk_svc_playback_play(mcsdk_svc_playback pb, const mcsdk_track_list source, size_t index) {
    PLAYBACK(pb)->Play(TRACKLIST(source), index);
}

mcsdk_export mcsdk_track_list_editor mcsdk_svc_playback_edit_playlist(mcsdk_svc_playback pb) {
    return mcsdk_track_list_editor { PLAYBACK(pb)->EditPlaylist() };
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
    return mcsdk_track_list { PLAYBACK(pb)->Clone() };
}

/*
 * IPreferences
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
    RELEASE(p, PREFS);
}

/*
 * IDataStream
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
    RELEASE(ds, DATASTREAM);
}

/*
 * IBuffer
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
    RELEASE(ab, BUFFER);
}

/*
 * IBufferProvider
 */

class mcsdk_audio_buffer_provider_callback_proxy: public IBufferProvider {
    public:
        mcsdk_audio_buffer_provider_callback_proxy(mcsdk_audio_buffer_provider_processed_callback cb) {
            this->cb = cb;
        }

        virtual void OnBufferProcessed(IBuffer *buffer) {
            cb({ buffer });
        }

    private:
        mcsdk_audio_buffer_provider_processed_callback cb;
};

mcsdk_export mcsdk_audio_buffer_provider mcsdk_audio_audio_buffer_provider_create(mcsdk_audio_buffer_provider_processed_callback cb) {
    return { new mcsdk_audio_buffer_provider_callback_proxy(cb) };
}

mcsdk_export void mcsdk_audio_audio_buffer_provider_release(mcsdk_audio_buffer_provider abp) {
    delete static_cast<mcsdk_audio_buffer_provider_callback_proxy*>(abp.opaque);
    abp.opaque = nullptr;
}

/*
 * IDevice
 */

mcsdk_export const char* mcsdk_device_get_name(mcsdk_device d) {
    return DEVICE(d)->Name();
}

mcsdk_export const char* mcsdk_device_get_id(mcsdk_device d) {
    return DEVICE(d)->Id();
}

mcsdk_export void mcsdk_device_release(mcsdk_device d) {
    RELEASE(d, DEVICE);
}

/*
 * IDeviceList
 */

mcsdk_export size_t mcsdk_device_list_get_count(mcsdk_device_list dl) {
    return DEVICELIST(dl)->Count();
}

mcsdk_export const mcsdk_device mcsdk_device_list_get_at(mcsdk_device_list dl, size_t index) {
    return mcsdk_device { (void*) DEVICELIST(dl)->At(index) };
}

mcsdk_export void mcsdk_device_list_release(mcsdk_device_list dl) {
    RELEASE(dl, DEVICELIST);
}

/*
 * IOutput
 */

mcsdk_export void mcsdk_audio_output_pause(mcsdk_audio_output o) {
    OUTPUT(o)->Pause();
}

mcsdk_export void mcsdk_audio_output_resume(mcsdk_audio_output o) {
    OUTPUT(o)->Resume();
}

mcsdk_export void mcsdk_audio_output_set_volume(mcsdk_audio_output o, double volume) {
    OUTPUT(o)->SetVolume(volume);
}

mcsdk_export double mcsdk_audio_output_get_volume(mcsdk_audio_output o) {
    return OUTPUT(o)->GetVolume();
}

mcsdk_export void mcsdk_audio_output_stop(mcsdk_audio_output o) {
    OUTPUT(o)->Stop();
}

mcsdk_export int mcsdk_audio_output_play(mcsdk_audio_output o, mcsdk_audio_buffer ab, mcsdk_audio_buffer_provider abp) {
    return OUTPUT(o)->Play(BUFFER(ab), BUFFERPROVIDER(abp));
}

mcsdk_export void mcsdk_audio_output_drain(mcsdk_audio_output o) {
    OUTPUT(o)->Drain();
}

mcsdk_export double mcsdk_audio_output_get_latency(mcsdk_audio_output o) {
    return OUTPUT(o)->Latency();
}

mcsdk_export const char* mcsdk_audio_output_get_name(mcsdk_audio_output o) {
    return OUTPUT(o)->Name();
}

mcsdk_export mcsdk_device_list mcsdk_audio_output_get_device_list(mcsdk_audio_output o) {
    return mcsdk_device_list { OUTPUT(o)->GetDeviceList() };
}

mcsdk_export bool mcsdk_audio_output_set_default_device(mcsdk_audio_output o, const char* device_id) {
    return OUTPUT(o)->SetDefaultDevice(device_id);
}

mcsdk_export mcsdk_device mcsdk_audio_output_get_default_device(mcsdk_audio_output o) {
    return mcsdk_device { OUTPUT(o)->GetDefaultDevice() };
}

mcsdk_export void mcsdk_audio_output_release(mcsdk_audio_output o) {
    RELEASE(o, OUTPUT);
}

/*
 * IDecoder
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
    RELEASE(d, DECODER);
}

/*
 * IEncoder
 */

mcsdk_export void mcsdk_encoder_release(mcsdk_encoder e) {
    RELEASE(e, ENCODER);
}

mcsdk_export mcsdk_encoder_type mcsdk_encoder_get_type(mcsdk_encoder e) {
    IEncoder* encoder = static_cast<IEncoder*>(e.opaque);
    if (dynamic_cast<IBlockingEncoder*>(encoder) != nullptr) {
        return mcsdk_encoder_type_blocking;
    }
    if (dynamic_cast<IStreamingEncoder*>(encoder) != nullptr) {
        return mcsdk_encoder_type_streaming;
    }
    return mcsdk_encoder_type_none;
}

/*
 * IBlockingEncoder
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
    RELEASE(be, BLOCKINGENCODER);
}

/*
 * IStreamingEncoder
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

mcsdk_export void mcsdk_streaming_encoder_release(mcsdk_streaming_encoder se) {
    RELEASE(se, STREAMINGENCODER);
}

/*
 * IDebug
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

/*
 * IEnvironment
 */

mcsdk_export size_t mcsdk_env_get_path(mcsdk_path_type type, char* dst, int size) {
    return ENV.GetPath((PathType) type, dst, size);
}

mcsdk_export mcsdk_data_stream mcsdk_env_open_data_stream(const char* uri, mcsdk_stream_open_flags flags) {
    return mcsdk_data_stream { ENV.GetDataStream(uri, (OpenFlags) flags) };
}

mcsdk_export mcsdk_decoder mcsdk_env_open_decoder(mcsdk_data_stream stream) {
    return mcsdk_decoder { ENV.GetDecoder(DATASTREAM(stream)) };
}

mcsdk_export mcsdk_encoder mcsdk_env_open_encoder(const char* type) {
    return mcsdk_encoder { ENV.GetEncoder(type) };
}

mcsdk_export mcsdk_audio_buffer mcsdk_env_create_audio_buffer(size_t samples, size_t rate, size_t channels) {
    return mcsdk_audio_buffer { ENV.GetBuffer(samples, rate, channels) };
}

mcsdk_export mcsdk_prefs mcsdk_env_open_preferences(const char* name) {
    return mcsdk_prefs { ENV.GetPreferences(name) };
}

mcsdk_export size_t mcsdk_env_get_output_count() {
    return ENV.GetOutputCount();
}

mcsdk_export mcsdk_audio_output mcsdk_env_get_output_at_index(size_t index) {
    return mcsdk_audio_output { ENV.GetOutputAtIndex(index) };
}

mcsdk_export mcsdk_audio_output mcsdk_env_get_output_with_name(const char* name) {
    return mcsdk_audio_output { ENV.GetOutputWithName(name) };
}

mcsdk_export mcsdk_replay_gain_mode mcsdk_env_get_replay_gain_mode() {
    return (mcsdk_replay_gain_mode) ENV.GetReplayGainMode();
}

mcsdk_export void mcsdk_env_set_replay_gain_mode(mcsdk_replay_gain_mode mode) {
    ENV.SetReplayGainMode((ReplayGainMode) mode);
}

mcsdk_export float mcsdk_env_get_preamp_gain() {
    return ENV.GetPreampGain();
}

mcsdk_export void mcsdk_env_set_preamp_gain(float gain) {
    ENV.SetPreampGain(gain);
}

mcsdk_export bool mcsdk_env_is_equalizer_enabled() {
    return ENV.GetEqualizerEnabled();
}

mcsdk_export void mcsdk_env_set_equalizer_enabled(bool enabled) {
    return ENV.SetEqualizerEnabled(enabled);
}

mcsdk_export bool mcsdk_env_get_equalizer_band_values(double target[], size_t count) {
    return ENV.GetEqualizerBandValues(target, count);
}

mcsdk_export bool mcsdk_env_set_equalizer_band_values(double values[], size_t count) {
    return ENV.SetEqualizerBandValues(values, count);
}

mcsdk_export void mcsdk_env_reload_playback_output() {
    ENV.ReloadPlaybackOutput();
}

mcsdk_export void mcsdk_env_set_default_output(mcsdk_audio_output output) {
    ENV.SetDefaultOutput(OUTPUT(output));
}

mcsdk_export mcsdk_audio_output mcsdk_env_get_default_output() {
    return mcsdk_audio_output { ENV.GetDefaultOutput() };
}

mcsdk_export mcsdk_transport_type mcsdk_env_get_transport_type() {
    return (mcsdk_transport_type) ENV.GetTransportType();
}

mcsdk_export void mcsdk_env_set_transport_type(mcsdk_transport_type type) {
    ENV.SetTransportType((TransportType) type);
}

/*
 * IStream
 */

mcsdk_export mcsdk_audio_stream mcsdk_audio_stream_create(int samples_per_channel, double buffer_length_seconds, mcsdk_audio_stream_flags options) {
    return mcsdk_audio_stream { Stream::CreateUnmanaged(samples_per_channel, buffer_length_seconds, (StreamFlags) options) };
}

mcsdk_export mcsdk_audio_buffer mcsdk_audio_stream_get_next_buffer(mcsdk_audio_stream as) {
    return mcsdk_audio_buffer { AUDIOSTREAM(as)->GetNextProcessedOutputBuffer() };
}

mcsdk_export void mcsdk_audio_stream_recycle_buffer(mcsdk_audio_stream as, mcsdk_audio_buffer ab) {
    AUDIOSTREAM(as)->OnBufferProcessedByPlayer(BUFFER(ab));
}

mcsdk_export double mcsdk_audio_stream_set_position(mcsdk_audio_stream as, double seconds) {
    return AUDIOSTREAM(as)->SetPosition(seconds);
}

mcsdk_export double mcsdk_audio_stream_get_duration(mcsdk_audio_stream as) {
    return AUDIOSTREAM(as)->GetDuration();
}

mcsdk_export bool mcsdk_audio_stream_open_uri(mcsdk_audio_stream as, const char* uri) {
    return AUDIOSTREAM(as)->OpenStream(uri);
}

mcsdk_export void mcsdk_audio_stream_interrupt(mcsdk_audio_stream as) {
    AUDIOSTREAM(as)->Interrupt();
}
mcsdk_export mcsdk_stream_capability mcsdk_audio_stream_get_capabilities(mcsdk_audio_stream as) {
    return (mcsdk_stream_capability) AUDIOSTREAM(as)->GetCapabilities();
}

mcsdk_export bool mcsdk_audio_stream_is_eof(mcsdk_audio_stream as) {
    return AUDIOSTREAM(as)->Eof();
}

mcsdk_export void mcsdk_audio_stream_release(mcsdk_audio_stream as) {
    RELEASE(as, AUDIOSTREAM);
}

/*
 * Player
 */

class mcsdk_audio_player_callback_proxy: public Player::EventListener {
    public:
        std::set<mcsdk_audio_player_callbacks*> callbacks;
        mcsdk_player_context_internal* context;
        virtual void OnPlayerPrepared(Player *player) {
            std::unique_lock<std::mutex> lock(this->context->event_mutex);
            for (auto c : callbacks) {
                if (c->on_prepared) {
                    c->on_prepared(mcsdk_audio_player { context });
                }
            }
        }
        virtual void OnPlayerStarted(Player *player) {
            std::unique_lock<std::mutex> lock(this->context->event_mutex);
            for (auto c : callbacks) {
                if (c->on_started) {
                    c->on_started(mcsdk_audio_player { context });
                }
            }
        }
        virtual void OnPlayerAlmostEnded(Player *player) {
            std::unique_lock<std::mutex> lock(this->context->event_mutex);
            for (auto c : callbacks) {
                if (c->on_almost_ended) {
                    c->on_almost_ended(mcsdk_audio_player { context });
                }
            }
        }
        virtual void OnPlayerFinished(Player *player) {
            std::unique_lock<std::mutex> lock(this->context->event_mutex);
            for (auto c : callbacks) {
                if (c->on_finished) {
                    c->on_finished(mcsdk_audio_player { context });
                }
            }
        }
        virtual void OnPlayerError(Player *player) {
            std::unique_lock<std::mutex> lock(this->context->event_mutex);
            for (auto c : callbacks) {
                if (c->on_error) {
                    c->on_error(mcsdk_audio_player { context });
                }
            }
        }
        virtual void OnPlayerDestroying(Player *player) {
            std::unique_lock<std::mutex> lock(this->context->event_mutex);
            for (auto c : callbacks) {
                if (c->on_destroying) {
                    c->on_destroying(mcsdk_audio_player { context });
                }
            }
            this->context->player_finished = true;
            this->context->finished_condition.notify_all();
        }
        virtual void OnPlayerMixPoint(Player *player, int id, double time) {
            std::unique_lock<std::mutex> lock(this->context->event_mutex);
            for (auto c : callbacks) {
                if (c->on_mixpoint) {
                    c->on_mixpoint(mcsdk_audio_player { context }, id, time);
                }
            }
        }
};

mcsdk_export mcsdk_audio_player mcsdk_audio_player_create(const char* url, mcsdk_audio_output output, mcsdk_audio_player_callbacks* callbacks, mcsdk_audio_player_gain gain) {
    Player::Gain playerGain;
    playerGain.gain = gain.gain;
    playerGain.preamp = gain.preamp;
    playerGain.peak = gain.peak;
    playerGain.peakValid = gain.peakValid;

    auto callbackProxy = new mcsdk_audio_player_callback_proxy();

    mcsdk_player_context_internal* playerContext = new mcsdk_player_context_internal();
    playerContext->event_listener = callbackProxy;
    playerContext->player_finished = false;

    playerContext->output = std::shared_ptr<IOutput>(OUTPUT(output), [](IOutput*){});

    playerContext->player = Player::Create(
        url,
        playerContext->output,
        Player::DestroyMode::Drain,
        playerContext->event_listener,
        playerGain);

    callbackProxy->context = playerContext;

    if (callbacks) {
        callbackProxy->callbacks.insert(callbacks);
    }

    return mcsdk_audio_player { playerContext };;
}

mcsdk_export int mcsdk_audio_player_get_url(mcsdk_audio_player ap, char* dst, int size) {
    mcsdk_player_context_internal* context = PLAYER(ap);
    std::unique_lock<std::mutex> lock(context->event_mutex);
    if (!context->player_finished) {
        return (int) CopyString(context->player->GetUrl(), dst, size);
    }
    return (int) CopyString("", dst, size);
}

mcsdk_export void mcsdk_audio_player_detach(mcsdk_audio_player ap, mcsdk_audio_player_callbacks* callbacks) {
    mcsdk_player_context_internal* context = PLAYER(ap);
    std::unique_lock<std::mutex> lock(context->event_mutex);
    if (!context->player_finished) {
        auto proxy = static_cast<mcsdk_audio_player_callback_proxy*>(context->event_listener);
        auto it = proxy->callbacks.find(callbacks);
        if (it != proxy->callbacks.end()) {
            proxy->callbacks.erase(it);
        }
    }
}

mcsdk_export void mcsdk_audio_player_attach(mcsdk_audio_player ap, mcsdk_audio_player_callbacks* callbacks) {
    mcsdk_player_context_internal* context = PLAYER(ap);
    std::unique_lock<std::mutex> lock(context->event_mutex);
    if (!context->player_finished) {
        static_cast<mcsdk_audio_player_callback_proxy*>(context->event_listener)->callbacks.insert(callbacks);
    }
}

mcsdk_export void mcsdk_audio_player_play(mcsdk_audio_player ap) {
    mcsdk_player_context_internal* context = PLAYER(ap);
    std::unique_lock<std::mutex> lock(context->event_mutex);
    if (!context->player_finished) {
        context->player->Play();
    }
}

mcsdk_export double mcsdk_audio_player_get_position(mcsdk_audio_player ap) {
    mcsdk_player_context_internal* context = PLAYER(ap);
    std::unique_lock<std::mutex> lock(context->event_mutex);
    if (!context->player_finished) {
        return context->player->GetPosition();
    }
    return 0.0;
}

mcsdk_export void mcsdk_audio_player_set_position(mcsdk_audio_player ap, double seconds) {
    mcsdk_player_context_internal* context = PLAYER(ap);
    std::unique_lock<std::mutex> lock(context->event_mutex);
    if (!context->player_finished) {
        context->player->SetPosition(seconds);
    }
}

mcsdk_export double mcsdk_audio_player_get_duration(mcsdk_audio_player ap) {
    mcsdk_player_context_internal* context = PLAYER(ap);
    std::unique_lock<std::mutex> lock(context->event_mutex);
    if (!context->player_finished) {
        return context->player->GetDuration();
    }
    return 0.0;
}

mcsdk_export void mcsdk_audio_player_add_mix_point(mcsdk_audio_player ap, int id, double time) {
    mcsdk_player_context_internal* context = PLAYER(ap);
    std::unique_lock<std::mutex> lock(context->event_mutex);
    if (!context->player_finished) {
        context->player->AddMixPoint(id, time);
    }
}

mcsdk_export bool mcsdk_audio_player_has_capability(mcsdk_audio_player ap, mcsdk_stream_capability capability) {
    mcsdk_player_context_internal* context = PLAYER(ap);
    std::unique_lock<std::mutex> lock(context->event_mutex);
    if (!context->player_finished) {
        return PLAYER(ap)->player->HasCapability((Capability) capability);
    }
    return false;
}

mcsdk_export void mcsdk_audio_player_release(mcsdk_audio_player ap, mcsdk_audio_player_release_mode mode) {
    mcsdk_player_context_internal* context = PLAYER(ap);

    {
        std::unique_lock<std::mutex> lock(context->event_mutex);
        if (!context->player_finished) {
            context->player->Destroy((Player::DestroyMode) mode);
            while (!context->player_finished) {
                context->finished_condition.wait(lock);
            }
        }
    }

    delete context->event_listener;
    delete context;

    ap.opaque = nullptr;
}

mcsdk_export mcsdk_audio_player_gain mcsdk_audio_player_get_default_gain() {
    return mcsdk_audio_player_gain { 1.0, 1.0, 0.0, false };
}
/*
 * IIndexer
 */

#define INDEXER_INTERNAL(x) static_cast<mcsdk_svc_indexer_context_internal*>(x.opaque)

mcsdk_export void mcsdk_svc_indexer_add_path(mcsdk_svc_indexer in, const char* path) {
    INDEXER(in)->AddPath(path);
}

mcsdk_export void mcsdk_svc_indexer_remove_path(mcsdk_svc_indexer in, const char* path) {
    INDEXER(in)->RemovePath(path);
}

mcsdk_export int mcsdk_svc_indexer_get_paths_count(mcsdk_svc_indexer in) {
    std::vector<std::string> paths;
    INDEXER(in)->GetPaths(paths);
    return (int) paths.size();
}

mcsdk_export int mcsdk_svc_indexer_get_paths_at(mcsdk_svc_indexer in, int index, char* dst, int len) {
    std::vector<std::string> paths;
    INDEXER(in)->GetPaths(paths);
    return CopyString(paths[index], dst, (int) len);
}

mcsdk_export void mcsdk_svc_indexer_schedule(mcsdk_svc_indexer in, mcsdk_svc_indexer_sync_type type) {
    INDEXER(in)->Schedule((IIndexer::SyncType) type);
}

mcsdk_export void mcsdk_svc_indexer_stop(mcsdk_svc_indexer in) {
    INDEXER(in)->Stop();
}

mcsdk_export mcsdk_svc_indexer_state mcsdk_svc_indexer_get_state(mcsdk_svc_indexer in) {
    return (mcsdk_svc_indexer_state) INDEXER(in)->GetState();
}

mcsdk_export void mcsdk_svc_indexer_add_callbacks(mcsdk_svc_indexer in, mcsdk_svc_indexer_callbacks* cb) {
    INDEXER_INTERNAL(in)->callbacks.insert(cb);
}

mcsdk_export void mcsdk_svc_indexer_remove_callbacks(mcsdk_svc_indexer in, mcsdk_svc_indexer_callbacks* cb) {
    auto internal = INDEXER_INTERNAL(in);
    auto it = internal->callbacks.find(cb);
    if (it != internal->callbacks.end()) {
        internal->callbacks.erase(it);
    }
}

/*
 * ILibrary
 */

class mcsdk_db_wrapped_query: public LocalQueryBase {
    public:
        mcsdk_db_wrapped_query(
            mcsdk_svc_library library,
            const std::string& name,
            void* user_context,
            mcsdk_svc_library_run_query_callback cb)
        {
            this->library = library;
            this->name = name;
            this->user_context = user_context;
            this->cb = cb;
        }

    protected:
        virtual bool OnRun(musik::core::db::Connection& db) {
            return cb(this->library, { &db }, this->user_context);
        }

        virtual std::string Name() {
            return "";
        }

    private:
        mcsdk_svc_library library;
        std::string name;
        void* user_context;
        mcsdk_svc_library_run_query_callback cb;
};

mcsdk_export void mcsdk_svc_library_run_query(mcsdk_svc_library l, const char* name, void* user_context, mcsdk_svc_library_run_query_callback cb, mcsdk_svc_library_query_flag flags) {
    LIBRARY(l)->Enqueue(std::make_shared<mcsdk_db_wrapped_query>(l, name, user_context, cb));
}

mcsdk_export int mcsdk_svc_library_get_id(mcsdk_svc_library l) {
    return LIBRARY(l)->Id();
}

mcsdk_export int mcsdk_svc_library_get_name(mcsdk_svc_library l, char* dst, int len) {
    return CopyString(LIBRARY(l)->Name(), dst, len);
}

/*
 * Statement
 */

mcsdk_export mcsdk_db_statement mcsdk_db_statement_create(mcsdk_db_connection db, const char* sql) {
    return { new Statement(sql, *DBCONNECTION(db)) };
}

mcsdk_export void mcsdk_db_statement_bind_int32(mcsdk_db_statement stmt, int position, int value) {
    DBSTATEMENT(stmt)->BindInt32(position, value);
}

mcsdk_export void mcsdk_db_statement_bind_int64(mcsdk_db_statement stmt, int position, int64_t value) {
    DBSTATEMENT(stmt)->BindInt64(position, value);
}

mcsdk_export void mcsdk_db_statement_bind_float(mcsdk_db_statement stmt, int position, float value) {
    DBSTATEMENT(stmt)->BindFloat(position, value);
}

mcsdk_export void mcsdk_db_statement_bind_text(mcsdk_db_statement stmt, int position, const char* value) {
    DBSTATEMENT(stmt)->BindText(position, value);
}

mcsdk_export void mcsdk_db_statement_bind_null(mcsdk_db_statement stmt, int position) {
    DBSTATEMENT(stmt)->BindNull(position);
}

mcsdk_export int mcsdk_db_statement_column_int32(mcsdk_db_statement stmt, int column) {
    return DBSTATEMENT(stmt)->ColumnInt32(column);
}

mcsdk_export int64_t mcsdk_db_statement_column_int64(mcsdk_db_statement stmt, int column) {
    return DBSTATEMENT(stmt)->ColumnInt64(column);
}

mcsdk_export float mcsdk_db_statement_column_float(mcsdk_db_statement stmt, int column) {
    return DBSTATEMENT(stmt)->ColumnFloat(column);
}

mcsdk_export int mcsdk_db_statement_column_text(mcsdk_db_statement stmt, int column, char* dst, int len) {
    return CopyString(DBSTATEMENT(stmt)->ColumnText(column), dst, len);
}

mcsdk_export mcsdk_db_result mcsdk_db_statement_step(mcsdk_db_statement stmt) {
    return (mcsdk_db_result) DBSTATEMENT(stmt)->Step();
}

mcsdk_export void mcsdk_db_statement_reset(mcsdk_db_statement stmt) {
    DBSTATEMENT(stmt)->Reset();
}

mcsdk_export void mcsdk_db_statement_unbind(mcsdk_db_statement stmt) {
    DBSTATEMENT(stmt)->Unbind();
}

mcsdk_export void mcsdk_db_statement_reset_and_unbind(mcsdk_db_statement stmt) {
    DBSTATEMENT(stmt)->ResetAndUnbind();
}

mcsdk_export void mcsdk_db_statement_release(mcsdk_db_statement stmt) {
    delete DBSTATEMENT(stmt);
    stmt.opaque = nullptr;
}

/*
 * ScopedTransaction
 */

mcsdk_export mcsdk_db_transaction mcsdk_db_transaction_create(mcsdk_db_connection db) {
    return { new ScopedTransaction(*DBCONNECTION(db)) };
}

mcsdk_export void mcsdk_db_transaction_cancel(mcsdk_db_transaction tx) {
    DBTRANSACTION(tx)->Cancel();
}

mcsdk_export void mcsdk_db_transaction_commit_and_restart(mcsdk_db_transaction tx) {
    DBTRANSACTION(tx)->CommitAndRestart();
}

mcsdk_export void mcsdk_db_transaction_release(mcsdk_db_transaction tx) {
    delete DBCONNECTION(tx);
    tx.opaque = nullptr;
}
