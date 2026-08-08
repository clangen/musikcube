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

/** @file CategoryTrackListQuery.h
 *  @brief Query that returns the tracks belonging to one or more categories.
 *  @details Selects tracks constrained by category predicates (and an optional
 *      text filter), sorted according to a TrackSortType. Handles both regular
 *      properties and playlists. */

#include <musikcore/db/Connection.h>
#include <musikcore/library/track/Track.h>
#include <musikcore/library/QueryBase.h>
#include <musikcore/library/query/util/CategoryQueryUtil.h>
#include <musikcore/library/query/util/TrackSort.h>
#include <musikcore/db/Statement.h>

#include "TrackListQueryBase.h"

/** @namespace musik::core::library::query
 *  @brief Query classes and helpers executed against a library. */
namespace musik { namespace core { namespace library { namespace query {

    /** @brief Lists tracks matching a set of category predicates.
     *  @details Builds either a playlist query (when predicated on the playlist
     *      type) or a regular category track query, applies the given sort order
     *      and returns a track list with headers and durations. */
    class CategoryTrackListQuery : public TrackListQueryBase {
        public:
            static const std::string kQueryName; /**< Query type name. */

            DELETE_CLASS_DEFAULTS(CategoryTrackListQuery)

            /** @brief Creates a query listing all tracks of a category.
             *  @param library The library to query.
             *  @param filter Optional text filter.
             *  @param sortType The sort order. */
            CategoryTrackListQuery(
                musik::core::ILibraryPtr library,
                const std::string& filter = "",
                TrackSortType sortType = TrackSortType::Album);

            /** @brief Creates a query for a single category value.
             *  @param library The library to query.
             *  @param column The category column name.
             *  @param id The category value id.
             *  @param filter Optional text filter.
             *  @param sortType The sort order. */
            CategoryTrackListQuery(
                musik::core::ILibraryPtr library,
                const std::string& column,
                int64_t id,
                const std::string& filter = "",
                TrackSortType sortType = TrackSortType::Album);

            /** @brief Creates a query for one predicate.
             *  @param library The library to query.
             *  @param predicate The predicate to constrain by.
             *  @param filter Optional text filter.
             *  @param sortType The sort order. */
            CategoryTrackListQuery(
                musik::core::ILibraryPtr library,
                const category::Predicate predicate,
                const std::string& filter = "",
                TrackSortType sortType = TrackSortType::Album);

            /** @brief Creates a query for multiple predicates.
             *  @param library The library to query.
             *  @param predicates The predicates to constrain by.
             *  @param filter Optional text filter.
             *  @param sortType The sort order. */
            CategoryTrackListQuery(
                musik::core::ILibraryPtr library,
                const category::PredicateList predicates,
                const std::string& filter = "",
                TrackSortType sortType = TrackSortType::Album);

            /* IQuery */
            /** @return The query type name. */
            std::string Name() override { return kQueryName; }

            /* TrackListQueryBase */
            /** @return The result track list (or nullptr). */
            Result GetResult() noexcept override;
            /** @return The column headers for the result. */
            Headers GetHeaders() noexcept override;
            /** @return The track durations, in seconds. */
            Durations GetDurations() noexcept override;
            /** @return A hash identifying this query's parameters. */
            size_t GetQueryHash() noexcept override;

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
            static std::shared_ptr<CategoryTrackListQuery> DeserializeQuery(
                musik::core::ILibraryPtr library, const std::string& data);

        protected:
            /* QueryBase */
            /** @brief Runs the query against the database.
             *  @param db The connection to run on.
             *  @return true on success. */
            bool OnRun(musik::core::db::Connection &db) override;

        private:
            /** @brief Which query flavor this instance uses. */
            enum class Type: int { Playlist = 0, Regular = 1 };

            /** @brief Determines the query type from the predicate lists. */
            void ScanPredicateListsForQueryType();

            /** @brief Runs the playlist-based query.
             *  @param db The connection to run on. */
            void PlaylistQuery(musik::core::db::Connection &db);
            /** @brief Runs the regular category query.
             *  @param db The connection to run on. */
            void RegularQuery(musik::core::db::Connection &db);
            /** @brief Adds the current statement row to the result.
             *  @param stmt The statement positioned on the current row. */
            void ProcessResult(musik::core::db::Statement& stmt);

            /* regular instance variables */
            musik::core::ILibraryPtr library; /**< Library to query. */
            bool parseHeaders;  /**< Whether headers should be parsed from the result. */
            size_t hash;        /**< Cached query hash. */
            std::string orderBy;/**< SQL ORDER BY fragment. */
            Type type;          /**< Query flavor. */

            /* serialized result fields */
            Result result;      /**< Result track list. */
            Headers headers;    /**< Result column headers. */
            Durations durations;/**< Result durations. */

            /* serialized query fields */
            std::string filter; /**< Text filter. */
            category::PredicateList regular, extended; /**< Category predicates. */
            TrackSortType sortType; /**< Sort order. */
    };

} } } }
