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

/** @file LocalMetadataProxy.h
 *  @brief IMetadataProxy implementation backed by the local library.
 *  @details Translates SDK metadata queries into local library queries, running
 *      them on the library's query thread and returning their results. Used by
 *      plugins that need access to library metadata through the SDK interface. */

#include <musikcore/library/ILibrary.h>
#include <musikcore/sdk/IMetadataProxy.h>

/** @namespace musik::core::library::query
 *  @brief Query classes and helpers executed against a library. */
namespace musik { namespace core { namespace library { namespace query {

    /** @brief Implements IMetadataProxy against a local library.
     *  @details Each method constructs the appropriate query, runs it synchronously
     *      via EnqueueAndWait(), and returns the resulting data. Instances are
     *      released with Release(). */
    class LocalMetadataProxy : public musik::core::sdk::IMetadataProxy {
        public:
            /** @brief Creates a proxy bound to a library.
             *  @param library The library to run queries against. */
            LocalMetadataProxy(musik::core::ILibraryPtr library);

            /** @brief Queries tracks matching the given search text.
             *  @param query The search filter.
             *  @param limit Maximum results, or -1 for all.
             *  @param offset Result offset for pagination.
             *  @return A new ITrackList. */
            musik::core::sdk::ITrackList*
                QueryTracks(
                    const char* query = "",
                    int limit = -1,
                    int offset = 0) override;

            /** @brief Looks up a single track by its internal id.
             *  @param trackId The track id.
             *  @return A new ITrack, or nullptr. */
            musik::core::sdk::ITrack*
                QueryTrackById(int64_t trackId) override;

            /** @brief Looks up a single track by its external id.
             *  @param externalId The external id assigned by the indexer source.
             *  @return A new ITrack, or nullptr. */
            musik::core::sdk::ITrack*
                QueryTrackByExternalId(const char* externalId) override;

            /** @brief Queries tracks belonging to a single category value.
             *  @param categoryType The category type (e.g. "genre", "artist").
             *  @param selectedId The category value id.
             *  @param filter Optional text filter.
             *  @param limit Maximum results, or -1 for all.
             *  @param offset Result offset for pagination.
             *  @return A new ITrackList. */
            musik::core::sdk::ITrackList*
                QueryTracksByCategory(
                    const char* categoryType,
                    int64_t selectedId,
                    const char* filter = "",
                    int limit = -1,
                    int offset = 0) override;

            /** @brief Queries tracks belonging to several category values at once.
             *  @param categories Array of category values.
             *  @param categoryCount Number of categories.
             *  @param filter Optional text filter.
             *  @param limit Maximum results, or -1 for all.
             *  @param offset Result offset for pagination.
             *  @return A new ITrackList. */
            musik::core::sdk::ITrackList*
                QueryTracksByCategories(
                    musik::core::sdk::IValue** categories,
                    size_t categoryCount,
                    const char* filter = "",
                    int limit = -1,
                    int offset = 0) override;

            /** @brief Queries tracks by their external ids.
             *  @param externalIds Array of external ids.
             *  @param externalIdCount Number of ids.
             *  @return A new ITrackList. */
            musik::core::sdk::ITrackList* QueryTracksByExternalId(
                const char** externalIds, size_t externalIdCount) override;

            /** @return A list of all category types (genres, artists, albums, ...). */
            musik::core::sdk::IValueList* ListCategories() override;

            /** @brief Lists the values of a category type.
             *  @param type The category type.
             *  @param filter Optional text filter.
             *  @return A new IValueList. */
            musik::core::sdk::IValueList*
                QueryCategory(
                    const char* type,
                    const char* filter = "") override;

            /** @brief Lists category values constrained by one predicate.
             *  @param type The category type.
             *  @param predicateType The constraining category type.
             *  @param predicateId The constraining category value id.
             *  @param filter Optional text filter.
             *  @return A new IValueList. */
            musik::core::sdk::IValueList*
                QueryCategoryWithPredicate(
                    const char* type,
                    const char* predicateType,
                    int64_t predicateId,
                    const char* filter = "") override;

            /** @brief Lists category values constrained by multiple predicates.
             *  @param type The category type.
             *  @param predicates Array of constraining categories.
             *  @param predicateCount Number of predicates.
             *  @param filter Optional text filter.
             *  @return A new IValueList. */
            musik::core::sdk::IValueList*
                QueryCategoryWithPredicates(
                    const char* type,
                    musik::core::sdk::IValue** predicates,
                    size_t predicateCount,
                    const char* filter = "") override;

            /** @brief Queries albums, optionally filtered.
             *  @param filter Optional text filter.
             *  @return A new IMapList of album metadata. */
            musik::core::sdk::IMapList*
                QueryAlbums(const char* filter = "") override;

            /** @brief Queries albums for a given category value.
             *  @param categoryIdName The category type to constrain by.
             *  @param categoryIdValue The category value id.
             *  @param filter Optional text filter.
             *  @return A new IMapList of album metadata. */
            musik::core::sdk::IMapList* QueryAlbums(
                const char* categoryIdName,
                int64_t categoryIdValue,
                const char* filter = "") override;

            /** @brief Saves a playlist from a list of internal track ids.
             *  @param trackIds Array of track ids.
             *  @param trackIdCount Number of ids.
             *  @param name The playlist name.
             *  @param playlistId Id of an existing playlist to overwrite, or 0 to create.
             *  @return The playlist id. */
            int64_t SavePlaylistWithIds(
                int64_t* trackIds,
                size_t trackIdCount,
                const char* name,
                const int64_t playlistId = 0) override;

            /** @brief Saves a playlist from a list of external track ids.
             *  @param externalIds Array of external ids.
             *  @param externalIdCount Number of ids.
             *  @param playlistName The playlist name.
             *  @param playlistId Id of an existing playlist to overwrite, or 0 to create.
             *  @return The playlist id. */
            int64_t SavePlaylistWithExternalIds(
                const char** externalIds,
                size_t externalIdCount,
                const char* playlistName,
                const int64_t playlistId = 0) override;

            /** @brief Saves a playlist from a track list.
             *  @param trackList The source track list.
             *  @param playlistName The playlist name.
             *  @param playlistId Id of an existing playlist to overwrite, or 0 to create.
             *  @return The playlist id. */
            int64_t SavePlaylistWithTrackList(
                musik::core::sdk::ITrackList* trackList,
                const char* playlistName,
                const int64_t playlistId = 0) override;

            /** @brief Renames a playlist.
             *  @param playlistId The playlist id.
             *  @param name The new name.
             *  @return true on success. */
            bool RenamePlaylist(
                const int64_t playlistId,
                const char* name) override;

            /** @brief Deletes a playlist.
             *  @param playlistId The playlist id.
             *  @return true on success. */
            bool DeletePlaylist(const int64_t playlistId) override;

            /** @brief Appends tracks to a playlist by internal id.
             *  @param playlistId The playlist id.
             *  @param trackIds Array of track ids.
             *  @param trackIdCount Number of ids.
             *  @param offset Insert position, or -1 to append.
             *  @return true on success. */
            bool AppendToPlaylistWithIds(
                const int64_t playlistId,
                const int64_t* trackIds,
                size_t trackIdCount,
                int offset = -1) override;

            /** @brief Appends tracks to a playlist by external id.
             *  @param playlistId The playlist id.
             *  @param externalIds Array of external ids.
             *  @param externalIdCount Number of ids.
             *  @param offset Insert position, or -1 to append.
             *  @return true on success. */
            bool AppendToPlaylistWithExternalIds(
                const int64_t playlistId,
                const char** externalIds,
                size_t externalIdCount,
                int offset = -1) override;

            /** @brief Appends a track list to a playlist.
             *  @param playlistId The playlist id.
             *  @param trackList The source track list.
             *  @param offset Insert position, or -1 to append.
             *  @return true on success. */
            bool AppendToPlaylistWithTrackList(
                const int64_t playlistId,
                musik::core::sdk::ITrackList* trackList,
                int offset = -1) override;

            /** @brief Removes tracks from a playlist.
             *  @param playlistId The playlist id.
             *  @param externalIds External ids of tracks to remove.
             *  @param sortOrders Sort orders of tracks to remove.
             *  @param count Number of tracks to remove.
             *  @return The number of tracks removed. */
            size_t RemoveTracksFromPlaylist(
                const int64_t playlistId,
                const char** externalIds,
                const int* sortOrders,
                int count) override;

            /** @brief Executes a raw SQL query and returns its results.
             *  @param query The SQL to run.
             *  @param allocator Allocator used for result data.
             *  @param resultData Output buffer receiving the serialized result.
             *  @param resultSize Size of the result, in bytes.
             *  @return true on success. */
            bool SendRawQuery(
                const char* query,
                musik::core::sdk::IAllocator& allocator,
                char** resultData,
                int* resultSize) override;

            /** @brief Frees the proxy (deletes this instance). */
            void Release() noexcept override;

        private:
            musik::core::ILibraryPtr library; /**< Library used to run queries. */
    };

} } } }
