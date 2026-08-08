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

/** @file IMetadataProxy.h @brief Defines the IMetadataProxy interface for querying the music library. */
#pragma once

#include "ITrackList.h"
#include "IValueList.h"
#include "IMapList.h"
#include "ITrack.h"
#include "IAllocator.h"

/** @namespace musik::core::sdk @brief Core SDK interfaces shared between the musikcube application and its plugins. */
namespace musik { namespace core { namespace sdk {

    /** @brief A query interface into the application's metadata library,
     *  allowing plugins to search tracks, browse categories, and manage playlists. */
    class IMetadataProxy {
        public:
            /** @brief Queries tracks using a free-form search query.
             *  @param query The search expression, or null to return all tracks.
             *  @param limit The maximum number of results, or -1 for no limit.
             *  @param offset The result offset for pagination.
             *  @return A list of matching tracks. */
            virtual ITrackList* QueryTracks(
                const char* query = nullptr,
                int limit = -1,
                int offset = 0) = 0;

            /** @brief Queries a single track by its internal id.
             *  @param trackId The internal track id.
             *  @return The track, or null if not found. */
            virtual ITrack* QueryTrackById(int64_t trackId) = 0;

            /** @brief Queries a single track by its external id.
             *  @param externalId The external id of the track.
             *  @return The track, or null if not found. */
            virtual ITrack* QueryTrackByExternalId(const char* externalId) = 0;

            /** @brief Queries tracks belonging to a category with a given value.
             *  @param categoryType The category type, e.g. category::Artist.
             *  @param selectedId The id of the category value to filter by.
             *  @param filter An optional text filter applied to the results.
             *  @param limit The maximum number of results, or -1 for no limit.
             *  @param offset The result offset for pagination.
             *  @return A list of matching tracks. */
            virtual ITrackList* QueryTracksByCategory(
                const char* categoryType,
                int64_t selectedId,
                const char* filter = "",
                int limit = -1,
                int offset = 0) = 0;

            /** @brief Queries tracks belonging to multiple category values.
             *  @param categories An array of category value resources.
             *  @param categoryCount The number of category values.
             *  @param filter An optional text filter applied to the results.
             *  @param limit The maximum number of results, or -1 for no limit.
             *  @param offset The result offset for pagination.
             *  @return A list of matching tracks. */
            virtual ITrackList* QueryTracksByCategories(
                IValue** categories,
                size_t categoryCount,
                const char* filter = "",
                int limit = -1,
                int offset = 0) = 0;

            /** @brief Queries tracks by a set of external ids.
             *  @param externalIds The external ids to look up.
             *  @param externalIdCount The number of external ids.
             *  @return A list of matching tracks. */
            virtual ITrackList* QueryTracksByExternalId(
                const char** externalIds, size_t externalIdCount) = 0;

            /** @brief Lists all categories in the library.
             *  @return A list of category values. */
            virtual IValueList* ListCategories() = 0;

            /** @brief Queries the values of a category, optionally filtered.
             *  @param type The category type.
             *  @param filter An optional text filter applied to the results.
             *  @return A list of category values. */
            virtual IValueList* QueryCategory(
                const char* type,
                const char* filter = "") = 0;

            /** @brief Queries category values that intersect another category value.
             *  @param type The category type to query.
             *  @param predicateType The predicate category type.
             *  @param predicateId The id of the predicate category value.
             *  @param filter An optional text filter applied to the results.
             *  @return A list of category values. */
            virtual IValueList* QueryCategoryWithPredicate(
                const char* type,
                const char* predicateType,
                int64_t predicateId,
                const char* filter = "") = 0;

            /** @brief Queries category values that intersect multiple predicate category values.
             *  @param type The category type to query.
             *  @param predicates An array of predicate category value resources.
             *  @param predicateCount The number of predicates.
             *  @param filter An optional text filter applied to the results.
             *  @return A list of category values. */
            virtual IValueList* QueryCategoryWithPredicates(
                const char* type,
                IValue** predicates,
                size_t predicateCount,
                const char* filter = "") = 0;

            /** @brief Queries albums, optionally filtered.
             *  @param filter An optional text filter applied to the results.
             *  @return A list of album maps. */
            virtual IMapList* QueryAlbums(const char* filter = "") = 0;

            /** @brief Queries albums that contain a category value.
             *  @param categoryIdName The category type used to filter.
             *  @param categoryIdValue The id of the category value to filter by.
             *  @param filter An optional text filter applied to the results.
             *  @return A list of album maps. */
            virtual IMapList* QueryAlbums(
                const char* categoryIdName,
                int64_t categoryIdValue,
                const char* filter = "") = 0;

            /** @brief Saves a playlist from a set of internal track ids.
             *  @param trackIds The track ids to include in the playlist.
             *  @param trackIdCount The number of track ids.
             *  @param playlistName The name of the playlist.
             *  @param playlistId The id of an existing playlist to overwrite, or 0 to create one.
             *  @return The id of the saved playlist, or -1 on failure. */
            virtual int64_t SavePlaylistWithIds(
                int64_t* trackIds,
                size_t trackIdCount,
                const char* playlistName,
                const int64_t playlistId = 0) = 0;

            /** @brief Saves a playlist from a set of external track ids.
             *  @param externalIds The external track ids to include in the playlist.
             *  @param externalIdCount The number of external ids.
             *  @param playlistName The name of the playlist.
             *  @param playlistId The id of an existing playlist to overwrite, or 0 to create one.
             *  @return The id of the saved playlist, or -1 on failure. */
            virtual int64_t SavePlaylistWithExternalIds(
                const char** externalIds,
                size_t externalIdCount,
                const char* playlistName,
                const int64_t playlistId = 0) = 0;

            /** @brief Saves a playlist from an existing track list.
             *  @param trackList The track list to save as a playlist.
             *  @param playlistName The name of the playlist.
             *  @param playlistId The id of an existing playlist to overwrite, or 0 to create one.
             *  @return The id of the saved playlist, or -1 on failure. */
            virtual int64_t SavePlaylistWithTrackList(
                ITrackList* trackList,
                const char* playlistName,
                const int64_t playlistId = 0) = 0;

            /** @brief Renames an existing playlist.
             *  @param playlistId The id of the playlist to rename.
             *  @param playlistName The new name of the playlist.
             *  @return True if the playlist was renamed. */
            virtual bool RenamePlaylist(
                const int64_t playlistId,
                const char* playlistName) = 0;

            /** @brief Deletes an existing playlist.
             *  @param playlistId The id of the playlist to delete.
             *  @return True if the playlist was deleted. */
            virtual bool DeletePlaylist(const int64_t playlistId) = 0;

            /** @brief Appends internal track ids to a playlist.
             *  @param playlistId The id of the playlist to modify.
             *  @param trackIds The track ids to append.
             *  @param trackIdCount The number of track ids.
             *  @param offset The insertion offset, or -1 to append at the end.
             *  @return True if the tracks were appended. */
            virtual bool AppendToPlaylistWithIds(
                const int64_t playlistId,
                const int64_t* trackIds,
                size_t trackIdCount,
                int offset = -1) = 0;

            /** @brief Appends external track ids to a playlist.
             *  @param playlistId The id of the playlist to modify.
             *  @param externalTrackIds The external track ids to append.
             *  @param externalTrackIdCount The number of external ids.
             *  @param offset The insertion offset, or -1 to append at the end.
             *  @return True if the tracks were appended. */
            virtual bool AppendToPlaylistWithExternalIds(
                const int64_t playlistId,
                const char** externalTrackIds,
                size_t externalTrackIdCount,
                int offset = -1) = 0;

            /** @brief Appends the tracks of an existing track list to a playlist.
             *  @param playlistId The id of the playlist to modify.
             *  @param trackList The track list to append.
             *  @param offset The insertion offset, or -1 to append at the end.
             *  @return True if the tracks were appended. */
            virtual bool AppendToPlaylistWithTrackList(
                const int64_t playlistId,
                ITrackList* trackList,
                int offset = -1) = 0;

            /** @brief Removes tracks from a playlist.
             *  @param playlistId The id of the playlist to modify.
             *  @param externalIds The external ids of the tracks to remove.
             *  @param sortOrders The sort order of each track to remove.
             *  @param count The number of tracks to remove.
             *  @return The number of tracks actually removed. */
            virtual size_t RemoveTracksFromPlaylist(
                const int64_t playlistId,
                const char** externalIds,
                const int* sortOrders,
                int count) = 0;

            /** @brief Sends a raw query to the library and returns the serialized result.
             *  @param query The raw query expression.
             *  @param allocator The allocator used to manage the result buffer.
             *  @param resultData On return, points to the allocated result data.
             *  @param resultSize On return, receives the size of the result data.
             *  @return True if the query succeeded. */
            virtual bool SendRawQuery(
                const char* query,
                IAllocator& allocator,
                char** resultData,
                int* resultSize) = 0;

            /** @brief Releases the proxy; callers must invoke this when done. */
            virtual void Release() = 0;
    };

} } }

