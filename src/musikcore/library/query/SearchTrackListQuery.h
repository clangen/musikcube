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

/** @file SearchTrackListQuery.h
 *  @brief Query that searches the library for tracks matching a text filter.
 *  @details Matches the filter against track metadata (title, artist, album,
 *      genre, ...) using substring or regex matching, sorted per the requested
 *      TrackSortType. */

#include "TrackListQueryBase.h"
#include <musikcore/library/query/util/TrackSort.h>

/** @namespace musik::core::library::query
 *  @brief Query classes and helpers executed against a library. */
namespace musik { namespace core { namespace library { namespace query {

    /** @brief Full-text search over the library's tracks.
     *  @details Results are sorted according to the configured TrackSortType; the
     *      sort display string is exposed for the UI. */
    class SearchTrackListQuery : public TrackListQueryBase {
        public:
            static const std::string kQueryName; /**< Query type name. */

            DELETE_CLASS_DEFAULTS(SearchTrackListQuery)

            /** @brief Creates a search query.
             *  @param library The library to search.
             *  @param matchType Substring or regex matching.
             *  @param filter The search text.
             *  @param sort The result sort order. */
            SearchTrackListQuery(
                musik::core::ILibraryPtr library,
                MatchType matchType,
                const std::string& filter,
                TrackSortType sort);

            /** @return A localized display string describing the active sort. */
            std::string GetSortDisplayString();

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
            /** @return The track durations, in seconds. */
            Durations GetDurations() noexcept override;

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
            static std::shared_ptr<SearchTrackListQuery> DeserializeQuery(
                musik::core::ILibraryPtr library, const std::string& data);

        protected:
            /* QueryBase */
            /** @brief Runs the query against the database.
             *  @param db The connection to run on.
             *  @return true on success. */
            bool OnRun(musik::core::db::Connection &db) override;

        private:
            musik::core::ILibraryPtr library; /**< Library to search. */
            MatchType matchType; /**< Filter match type. */
            bool parseHeaders;   /**< Whether headers should be parsed from the result. */
            std::string orderBy; /**< SQL ORDER BY fragment. */
            std::string orderByPredicate; /**< SQL predicate isolating ranked results. */
            std::string displayString; /**< Localized sort display string. */
            size_t hash;         /**< Cached query hash. */

            /* serialized query fields */
            std::string filter; /**< Search text. */
            TrackSortType sortType; /**< Sort order. */

            /* serialized result fields */
            Result result;      /**< Result track list. */
            Headers headers;    /**< Result column headers. */
            Durations durations;/**< Result durations. */
    };

} } } }
