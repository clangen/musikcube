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

/** @file CategoryListQuery.h
 *  @brief Query that lists category values (albums, artists, genres, ...).
 *  @details Returns the values of a category type from the library, optionally
 *      constrained by predicates and a text filter. Supports regular properties,
 *      extended (plugin-defined) properties, and playlists. */

#include <musikcore/library/QueryBase.h>
#include <musikcore/library/query/util/CategoryQueryUtil.h>
#include <musikcore/library/query/util/SdkWrappers.h>
#include <musikcore/db/Statement.h>
#include <musikcore/db/Connection.h>
#include <musikcore/sdk/IValueList.h>
#include <musikcore/support/Common.h>
#include <memory>

/** @namespace musik::core::library::query
 *  @brief Query classes and helpers executed against a library. */
namespace musik { namespace core { namespace library { namespace query {

    /** @brief Lists the values of a category type.
     *  @details The result is an SdkValueList of id/name pairs. Which query is run
     *      depends on the tracked field and predicates: regular properties use the
     *      optimized regular query, extended properties query the key/value store,
     *      and playlists are queried from the playlists table. */
    class CategoryListQuery : public musik::core::library::query::QueryBase {
        public:
            static const std::string kQueryName; /**< Query type name. */

            using Result = SdkValueList::Shared; /**< Result alias. */

            DELETE_COPY_AND_ASSIGNMENT_DEFAULTS_WITH_DEFAULT_CTOR(CategoryListQuery)

            /** @brief Creates a query listing all values of a category.
             *  @param matchType How the filter text is matched.
             *  @param trackField The category field (e.g. "album").
             *  @param filter Optional text filter. */
            CategoryListQuery(
                MatchType matchType,
                const std::string& trackField,
                const std::string& filter = "");

            /** @brief Creates a query listing values constrained by one predicate.
             *  @param matchType How the filter text is matched.
             *  @param trackField The category field (e.g. "album").
             *  @param predicate The predicate to constrain by.
             *  @param filter Optional text filter. */
            CategoryListQuery(
                MatchType matchType,
                const std::string& trackField,
                const category::Predicate predicate,
                const std::string& filter = "");

            /** @brief Creates a query listing values constrained by multiple predicates.
             *  @param matchType How the filter text is matched.
             *  @param trackField The category field (e.g. "album").
             *  @param predicate The predicates to constrain by.
             *  @param filter Optional text filter. */
            CategoryListQuery(
                MatchType matchType,
                const std::string& trackField,
                const category::PredicateList predicate,
                const std::string& filter = "");

            /** @return The result value list (or nullptr). */
            Result GetResult() noexcept;
            /** @return The index of the value with the given id, or -1.
             *  @param id The value id. */
            int GetIndexOf(int64_t id);
            /** @return The result as a raw SDK IValueList (borrowed). */
            musik::core::sdk::IValueList* GetSdkResult();

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
             *  @param data The serialized query.
             *  @return The deserialized query. */
            static std::shared_ptr<CategoryListQuery> DeserializeQuery(const std::string& data);

        protected:
            /* QueryBase */
            /** @brief Runs the query against the database.
             *  @param db The connection to run on.
             *  @return true on success. */
            bool OnRun(musik::core::db::Connection &db) override;

            /** @brief Which source the category values are queried from. */
            enum class OutputType: int {
                Regular = 1, /**< Regular property (album, artist, ...). */
                Extended = 2,/**< Plugin-defined property. */
                Playlist = 3 /**< The user's playlists. */
            };

            /** @brief Queries playlist values.
             *  @param db The connection to run on. */
            void QueryPlaylist(musik::core::db::Connection &db);
            /** @brief Queries regular property values.
             *  @param db The connection to run on. */
            void QueryRegular(musik::core::db::Connection &db);
            /** @brief Queries extended property values.
             *  @param db The connection to run on. */
            void QueryExtended(musik::core::db::Connection &db);
            /** @brief Adds the current statement row to the result.
             *  @param stmt The statement positioned on the current row. */
            void ProcessResult(musik::core::db::Statement &stmt);

            std::string trackField; /**< Category field being listed. */
            std::string filter;     /**< Text filter. */
            MatchType matchType;    /**< Filter match type. */
            OutputType outputType;  /**< Query source type. */
            category::PredicateList regular, extended; /**< Category predicates. */
            Result result;          /**< Query result. */
    };

} } } }
