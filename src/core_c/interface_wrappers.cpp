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

#include <core/sdk/IResource.h>
#include <core/sdk/IValue.h>
#include <core/sdk/IValueList.h>
#include <core/sdk/IMap.h>
#include <core/sdk/IMapList.h>
#include <core/sdk/ITrack.h>
#include <core/sdk/ITrackList.h>
#include <core/sdk/ITrackListEditor.h>
#include <core/sdk/IMetadataProxy.h>

using namespace musik::core::sdk;

#define R(x) reinterpret_cast<IResource*>(x)
#define V(x) reinterpret_cast<IValue*>(x)
#define M(x) reinterpret_cast<IMap*>(x)
#define VL(x) reinterpret_cast<IValueList*>(x)
#define ML(x) reinterpret_cast<IMapList*>(x)
#define TRACK(x) reinterpret_cast<ITrack*>(x)
#define TRACKLIST(x) reinterpret_cast<ITrackList*>(x)
#define TRACKLISTEDITOR(x) reinterpret_cast<ITrackListEditor*>(x)
#define METADATA(x) reinterpret_cast<IMetadataProxy*>(x)

/*
 *
 * IResource
 *
 */

mcsdk_export int64_t mcsdk_resource_get_id(mcsdk_resource r) {
    return R(r)->GetId();
}

mcsdk_export mcsdk_resource_class mcsdk_resource_get_class(mcsdk_resource r) {
    return (mcsdk_resource_class) R(r)->GetClass();
}

mcsdk_export void mcsdk_resource_release(mcsdk_resource r) {
    R(r)->Release();
}

/*
 *
 * IValue
 *
 */

mcsdk_export size_t mcsdk_value_get_value(mcsdk_value v, char* dst, size_t size) {
    return V(v)->GetValue(dst, size);
}

mcsdk_export void mcsdk_value_release(mcsdk_value v) {
    return V(v)->Release();
}

/*
 *
 * IValueList
 *
 */

mcsdk_export size_t mcsdk_value_list_count(mcsdk_value_list vl) {
    return VL(vl)->Count();
}

mcsdk_export mcsdk_value mcsdk_value_list_get_at(mcsdk_value_list vl, size_t index) {
    return (mcsdk_value) VL(vl)->GetAt(index);
}

mcsdk_export void mcsdk_value_list_release(mcsdk_value_list vl) {
    VL(vl)->Release();
}

/*
 *
 * IMap
 *
 */

mcsdk_export int mcsdk_map_get_string(mcsdk_map m, const char* key, char* dst, int size) {
    return M(m)->GetString(key, dst, size);
}

mcsdk_export int64_t mcsdk_map_get_int64(mcsdk_map m, const char* key, int64_t default_value) {
    return M(m)->GetInt64(key, default_value);
}

mcsdk_export int32_t mcsdk_map_get_int32(mcsdk_map m, const char* key, int32_t default_value) {
    return M(m)->GetInt32(key, default_value);
}

mcsdk_export double mcsdk_map_get_double(mcsdk_map m, const char* key, double default_value) {
    return M(m)->GetDouble(key, default_value);
}

mcsdk_export void mcsdk_map_release(mcsdk_map m) {
    M(m)->Release();
}

/*
 *
 * IMapList
 *
 */

mcsdk_export size_t mcsdk_map_list_get_count(mcsdk_map_list ml) {
    return ML(ml)->Count();
}

mcsdk_export mcsdk_map_list mcsdk_map_list_get_at(mcsdk_map_list ml, size_t index) {
    return (mcsdk_map_list) ML(ml)->GetAt(index);
}

mcsdk_export void mcsdk_map_list_release(mcsdk_map_list ml) {
    ML(ml)->Release();
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
