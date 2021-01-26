//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004-2020 musikcube team
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

#include <musikcore/library/QueryBase.h>
#include <musikcore/library/query/util/CategoryQueryUtil.h>
#include <musikcore/library/query/util/SdkWrappers.h>
#include <musikcore/db/Statement.h>
#include <musikcore/db/Connection.h>
#include <musikcore/sdk/IValueList.h>
#include <musikcore/support/Common.h>
#include <memory>

namespace musik { namespace core { namespace library { namespace query {

    class CategoryListQuery : public musik::core::library::query::QueryBase {
        public:
            static const std::string kQueryName;

            using Result = SdkValueList::Shared;

            DELETE_COPY_AND_ASSIGNMENT_DEFAULTS_WITH_DEFAULT_CTOR(CategoryListQuery)

            CategoryListQuery(
                MatchType matchType,
                const std::string& trackField,
                const std::string& filter = "");

            CategoryListQuery(
                MatchType matchType,
                const std::string& trackField,
                const category::Predicate predicate,
                const std::string& filter = "");

            CategoryListQuery(
                MatchType matchType,
                const std::string& trackField,
                const category::PredicateList predicate,
                const std::string& filter = "");

            Result GetResult() noexcept;
            int GetIndexOf(int64_t id);
            musik::core::sdk::IValueList* GetSdkResult();

            /* IQuery */
            std::string Name() override { return kQueryName; }

            /* ISerializableQuery */
            std::string SerializeQuery() override;
            std::string SerializeResult() override;
            void DeserializeResult(const std::string& data) override;
            static std::shared_ptr<CategoryListQuery> DeserializeQuery(const std::string& data);

        protected:
            /* QueryBase */
            bool OnRun(musik::core::db::Connection &db) override;

            enum class OutputType: int {
                Regular = 1,
                Extended = 2,
                Playlist = 3
            };

            void QueryPlaylist(musik::core::db::Connection &db);
            void QueryRegular(musik::core::db::Connection &db);
            void QueryExtended(musik::core::db::Connection &db);
            void ProcessResult(musik::core::db::Statement &stmt);

            std::string trackField;
            std::string filter;
            MatchType matchType;
            OutputType outputType;
            category::PredicateList regular, extended;
            Result result;
    };

} } } }
