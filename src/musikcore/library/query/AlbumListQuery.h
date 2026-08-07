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

/** @file AlbumListQuery.h
 *  @brief Query that returns a list of albums, optionally filtered and predicated.
 *  @details Retrieves album metadata (id, name, album artist, thumbnail) from the
 *      local library, optionally constrained by category predicates and a text
 *      filter. Result is exposed as a MetadataMapList. */

#include <musikcore/library/QueryBase.h>
#include <musikcore/library/query/util/CategoryQueryUtil.h>
#include <musikcore/library/metadata/MetadataMapList.h>
#include <musikcore/db/Connection.h>
#include <musikcore/support/DeleteDefaults.h>

/** @namespace musik::core::library::query
 *  @brief Query classes and helpers executed against a library. */
namespace musik { namespace core { namespace library { namespace query {

    /** @brief Lists albums from the library.
     *  @details Supports a free-text filter, a single predicate (e.g. all albums
     *      by one artist), or a list of predicates. Each result entry is a
     *      MetadataMap with album id, name, album artist and thumbnail id. */
    class AlbumListQuery : public musik::core::library::query::QueryBase {
        public:
            static const std::string kQueryName; /**< Query type name. */

            DELETE_COPY_AND_ASSIGNMENT_DEFAULTS(AlbumListQuery)

            /** @brief Creates a query listing all albums, optionally filtered.
             *  @param filter Optional text filter. */
            AlbumListQuery(
                const std::string& filter = "");

            /** @brief Creates a query listing albums for a single category value.
             *  @param fieldIdName The category field name.
             *  @param fieldIdValue The category value id.
             *  @param filter Optional text filter. */
            AlbumListQuery(
                const std::string& fieldIdName,
                int64_t fieldIdValue,
                const std::string& filter = "");

            /** @brief Creates a query listing albums matching one predicate.
             *  @param predicate The predicate to constrain by.
             *  @param filter Optional text filter. */
            AlbumListQuery(
                const category::Predicate predicate,
                const std::string& filter = "");

            /** @brief Creates a query listing albums matching multiple predicates.
             *  @param predicates The predicates to constrain by.
             *  @param filter Optional text filter. */
            AlbumListQuery(
                const category::PredicateList predicates,
                const std::string& filter = "");

            virtual ~AlbumListQuery();

            /* IQuery */
            /** @return The query type name. */
            std::string Name() override { return kQueryName; }
            /** @return The result album map list (or nullptr). */
            musik::core::MetadataMapListPtr GetResult() noexcept;

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
            static std::shared_ptr<AlbumListQuery> DeserializeQuery(const std::string& data);

            /* AlbumListQuery */
            /** @return The result as a raw SDK IMapList (borrowed). */
            musik::core::sdk::IMapList* GetSdkResult();

        protected:
            /* QueryBase */
            /** @brief Runs the query against the database.
             *  @param db The connection to run on.
             *  @return true on success. */
            bool OnRun(musik::core::db::Connection &db) override;

            std::string filter; /**< Text filter. */
            category::PredicateList regular, extended; /**< Category predicates. */
            musik::core::MetadataMapListPtr result; /**< Query result. */
    };

} } } }
