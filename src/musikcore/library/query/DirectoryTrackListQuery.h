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

/** @file DirectoryTrackListQuery.h
 *  @brief Query that returns the tracks within a single directory.
 *  @details Loads all tracks belonging to the given directory path, with an
 *      optional text filter, ordered by album grouping. */

#include <musikcore/library/query/TrackListQueryBase.h>
#include "TrackListQueryBase.h"

/** @namespace musik::core::library::query
 *  @brief Query classes and helpers executed against a library. */
namespace musik { namespace core { namespace library { namespace query {

    /** @brief Lists the tracks in one directory.
     *  @details The result includes headers (for UI column layout) and durations,
     *      and is identified by a stable query hash. */
    class DirectoryTrackListQuery : public TrackListQueryBase {
        public:
            static const std::string kQueryName; /**< Query type name. */

            DELETE_COPY_AND_ASSIGNMENT_DEFAULTS(DirectoryTrackListQuery)

            /** @brief Creates a directory track list query.
             *  @param library The library to query.
             *  @param directory The directory path.
             *  @param filter Optional text filter. */
            DirectoryTrackListQuery(
                musik::core::ILibraryPtr library,
                const std::string& directory,
                const std::string& filter = "");

            /* IQuery */
            /** @return The query type name. */
            std::string Name() override { return kQueryName; }

            /* TrackListQueryBase */
            /** @return The result track list (or nullptr). */
            Result GetResult() noexcept override { return this->result; }
            /** @return The column headers for the result. */
            Headers GetHeaders() noexcept override { return this->headers; }
            /** @return A hash identifying this query's parameters. */
            size_t GetQueryHash() noexcept override { return this->hash; }
            /** @return The track durations, in seconds. */
            Durations GetDurations() noexcept override { return this->durations; }

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
            static std::shared_ptr<DirectoryTrackListQuery> DeserializeQuery(
                musik::core::ILibraryPtr library, const std::string& data);

        protected:
            /* QueryBase */
            /** @brief Runs the query against the database.
             *  @param db The connection to run on.
             *  @return true on success. */
            bool OnRun(musik::core::db::Connection &db) override;

        private:
            musik::core::ILibraryPtr library; /**< Library to query. */
            std::string directory, filter; /**< Directory path and text filter. */
            Result result;     /**< Result track list. */
            Headers headers;   /**< Result column headers. */
            Durations durations; /**< Result durations. */
            size_t hash;       /**< Cached query hash. */
    };

} } } }
