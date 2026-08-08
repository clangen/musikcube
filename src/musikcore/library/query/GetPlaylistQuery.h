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

/** @file GetPlaylistQuery.h
 *  @brief Query that loads the tracks of a single playlist.
 *  @details Returns the ordered track list of a playlist together with column
 *      headers, used to populate the playlist editor UI. */

#include <musikcore/db/Connection.h>
#include <musikcore/library/track/Track.h>
#include <musikcore/library/QueryBase.h>

#include "TrackListQueryBase.h"

/** @namespace musik::core::library::query
 *  @brief Query classes and helpers executed against a library. */
namespace musik { namespace core { namespace library { namespace query {

    /** @brief Loads a playlist's tracks.
     *  @details Tracks are returned in playlist order. Durations are not provided
     *      by this query. */
    class GetPlaylistQuery : public TrackListQueryBase {
        public:
            static const std::string kQueryName; /**< Query type name. */

            DELETE_CLASS_DEFAULTS(GetPlaylistQuery)

            /** @brief Creates a get-playlist query.
             *  @param library The library to query.
             *  @param playlistId The playlist to load. */
            GetPlaylistQuery(
                musik::core::ILibraryPtr library,
                int64_t playlistId);

            /* IQuery */
            /** @return The query type name. */
            std::string Name() override { return kQueryName; }

            /* TrackListQueryBase */
            /** @return The result track list (or nullptr). */
            Result GetResult() noexcept override;
            /** @return The column headers for the result. */
            Headers GetHeaders() noexcept override;
            /** @return A hash identifying this query's parameters. */
            size_t GetQueryHash() noexcept override;
            /** @return An empty durations map (durations are not provided). */
            Durations GetDurations() noexcept override {
                return std::make_shared<std::map<size_t, size_t>>();
            }

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
            static std::shared_ptr<GetPlaylistQuery> DeserializeQuery(
                musik::core::ILibraryPtr library, const std::string& data);

        protected:
            /* QueryBase */
            /** @brief Runs the query against the database.
             *  @param db The connection to run on.
             *  @return true on success. */
            bool OnRun(musik::core::db::Connection &db) override;

        private:
            musik::core::ILibraryPtr library; /**< Library to query. */
            int64_t playlistId; /**< Playlist to load. */
            size_t hash;        /**< Cached query hash. */
            Result result;      /**< Result track list. */
            Headers headers;    /**< Result column headers. */
    };

} } } }
