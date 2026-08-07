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

/** @file TrackMetadataBatchQuery.h
 *  @brief Query that loads metadata for many tracks at once.
 *  @details Fetches full metadata for a set of track ids in a single batch query,
 *      returning a map from track id to populated Track. */

#include <musikcore/library/QueryBase.h>
#include <musikcore/library/track/Track.h>
#include <musikcore/library/ILibrary.h>
#include <unordered_set>
#include <unordered_map>

/** @namespace musik::core::library::query
 *  @brief Query classes and helpers executed against a library. */
namespace musik { namespace core { namespace library { namespace query {

/** @brief Loads metadata for a batch of tracks by id.
 *  @details Runs one batched SQL query (tracks.id IN (...)) so a large set of
 *      tracks can be populated in a single pass. */
class TrackMetadataBatchQuery: public QueryBase {
    public:
        static const std::string kQueryName; /**< Query type name. */

        using IdToTrackMap = std::unordered_map<int64_t, TrackPtr>; /**< Track id -> Track map. */

        DELETE_CLASS_DEFAULTS(TrackMetadataBatchQuery)

        /** @brief Creates a batch metadata query.
         *  @param trackIds The track ids to load.
         *  @param library The library to query. */
        TrackMetadataBatchQuery(
            std::unordered_set<int64_t> trackIds,
            musik::core::ILibraryPtr library);

        /** @return The resulting id-to-track map.
         *  @note Only present after the query completes. */
        const IdToTrackMap& Result() noexcept {
            return this->result;
        }

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
        static std::shared_ptr<TrackMetadataBatchQuery> DeserializeQuery(
            musik::core::ILibraryPtr library, const std::string& data);

    protected:
        /* QueryBase */
        /** @brief Runs the query against the database.
         *  @param db The connection to run on.
         *  @return true on success. */
        bool OnRun(musik::core::db::Connection& db) override;

    private:
        musik::core::ILibraryPtr library; /**< Library to query. */
        std::unordered_set<int64_t> trackIds; /**< Track ids to load. */
        IdToTrackMap result; /**< Result map. */
};

} } } }