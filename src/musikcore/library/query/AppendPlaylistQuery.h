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

/** @file AppendPlaylistQuery.h
 *  @brief Query that appends tracks to an existing playlist.
 *  @details Adds tracks (given as a TrackList or an SDK ITrackList) to the end of
 *      a playlist, or at a specific offset. Broadcasts a mutation message so
 *      listeners can refresh. */

#include <musikcore/library/QueryBase.h>
#include <musikcore/library/track/TrackList.h>
#include <stdint.h>
#include <vector>

/** @namespace musik::core::library::query
 *  @brief Query classes and helpers executed against a library. */
namespace musik { namespace core { namespace library { namespace query {

    /** @brief Appends tracks to a playlist.
     *  @details Tracks may be supplied as an internal TrackList or as an SDK
     *      ITrackList. The offset controls the insertion point (-1 appends). */
    class AppendPlaylistQuery : public musik::core::library::query::QueryBase {
        public:
            static const std::string kQueryName; /**< Query type name. */

            DELETE_CLASS_DEFAULTS(AppendPlaylistQuery)

            /** @brief Creates a query appending an internal track list.
             *  @param library The library to mutate.
             *  @param playlistId The playlist id.
             *  @param tracks The tracks to append.
             *  @param offset Insert position, or -1 to append at the end. */
            AppendPlaylistQuery(
                musik::core::ILibraryPtr library,
                const int64_t playlistId,
                std::shared_ptr<musik::core::TrackList> tracks,
                const int offset = -1) noexcept;

            /** @brief Creates a query appending an SDK track list.
             *  @param library The library to mutate.
             *  @param playlistId The playlist id.
             *  @param tracks The tracks to append.
             *  @param offset Insert position, or -1 to append at the end. */
            AppendPlaylistQuery(
                musik::core::ILibraryPtr library,
                const int64_t playlistId,
                musik::core::sdk::ITrackList *tracks,
                const int offset = -1) noexcept;

            /* IQuery */
            /** @return The query type name. */
            std::string Name() override { return kQueryName; }

            /* ISerializableQuery */
            /** @return The serialized query parameters. */
            std::string SerializeQuery() override;
            /** @return The serialized result. */
            std::string SerializeResult() override;
            /** @brief Populates the result from serialized data.
             *  @param data The serialized result. */
            void DeserializeResult(const std::string& data) override;
            /** @brief Recreates a query from serialized parameters.
             *  @param library The library the query will run on.
             *  @param data The serialized query.
             *  @return The deserialized query. */
            static std::shared_ptr<AppendPlaylistQuery> DeserializeQuery(
                musik::core::ILibraryPtr library, const std::string& data);

        protected:
            /* QueryBase */
            /** @brief Runs the query against the database.
             *  @param db The connection to run on.
             *  @return true on success. */
            bool OnRun(musik::core::db::Connection &db) override;

        private:
            /** @brief Broadcasts a playlist mutation to listeners. */
            void SendPlaylistMutationBroadcast();

            musik::core::ILibraryPtr library; /**< Library to mutate. */
            std::shared_ptr<musik::core::TrackList> sharedTracks; /**< Tracks to append (shared variant). */
            musik::core::sdk::ITrackList* rawTracks; /**< Tracks to append (SDK variant). */
            int64_t playlistId; /**< Target playlist id. */
            int offset; /**< Insert position (-1 appends). */
            bool result{ false }; /**< Whether the append succeeded. */
    };

} } } }
