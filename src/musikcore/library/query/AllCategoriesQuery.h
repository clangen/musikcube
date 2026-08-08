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

/** @file AllCategoriesQuery.h
 *  @brief Query that returns the list of available category types.
 *  @details Returns the set of category types the library supports (albums,
 *      artists, genres, directories, etc.) as an SdkValueList, used to build the
 *      "browse by" navigation. */

#include <musikcore/support/DeleteDefaults.h>
#include <musikcore/library/QueryBase.h>
#include <musikcore/library/query/util/SdkWrappers.h>
#include <musikcore/sdk/IValueList.h>

/** @namespace musik::core::library::query
 *  @brief Query classes and helpers executed against a library. */
namespace musik { namespace core { namespace library { namespace query {

    /** @brief Enumerates all category types available in the library. */
    class AllCategoriesQuery : public musik::core::library::query::QueryBase {
        public:
            static const std::string kQueryName; /**< Query type name. */

            using Result = SdkValueList::Shared; /**< Result alias. */

            DELETE_COPY_AND_ASSIGNMENT_DEFAULTS(AllCategoriesQuery)

            /** @brief Creates a query that lists all categories. */
            AllCategoriesQuery();

            /** @return The list of category types (or nullptr). */
            virtual Result GetResult() noexcept;
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
            static std::shared_ptr<AllCategoriesQuery> DeserializeQuery(const std::string& data);

        protected:
            /* QueryBase */
            /** @brief Runs the query against the database.
             *  @param db The connection to run on.
             *  @return true on success. */
            bool OnRun(musik::core::db::Connection &db) override;

        private:
            Result result; /**< Query result. */
    };

} } } }
