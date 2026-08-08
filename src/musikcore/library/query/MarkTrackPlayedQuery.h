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

/** @file MarkTrackPlayedQuery.h
 *  @brief Query that records that a track has been played.
 *  @details Updates the track's last-played timestamp and increments its play
 *      count, used for recently-played sorting and statistics. */

#include <musikcore/library/QueryBase.h>

/** @namespace musik::core::library::query
 *  @brief Query classes and helpers executed against a library. */
namespace musik { namespace core { namespace library { namespace query {

    /** @brief Marks a track as played in the library.
     *  @details On success the play count and last-played timestamp are updated
     *      in the tracks table. */
    class MarkTrackPlayedQuery: public QueryBase {
        public:
            static const std::string kQueryName; /**< Query type name. */

            DELETE_CLASS_DEFAULTS(MarkTrackPlayedQuery)

            /** @brief Creates a mark-played query.
             *  @param trackId The id of the track to mark. */
            MarkTrackPlayedQuery(const int64_t trackId) noexcept;

            /* IQuery */
            /** @return The query type name. */
            std::string Name() override { return "MarkTrackPlayedQuery"; }

            /* ISerializableQuery */
            /** @return The serialized query parameters. */
            std::string SerializeQuery() override;
            /** @return The serialized result. */
            std::string SerializeResult() override;
            /** @brief Populates the result from serialized data.
             *  @param data The serialized result. */
            void DeserializeResult(const std::string& data) override;
            /** @brief Recreates a query from serialized parameters.
             *  @param data The serialized query.
             *  @return The deserialized query. */
            static std::shared_ptr<MarkTrackPlayedQuery> DeserializeQuery(const std::string& data);

        protected:
            /* QueryBase */
            /** @brief Runs the query against the database.
             *  @param db The connection to run on.
             *  @return true on success. */
            bool OnRun(musik::core::db::Connection &db) override;

        private:
            int64_t trackId; /**< Track to mark as played. */
            bool result{ false }; /**< Whether the update succeeded. */
    };

} } } }
