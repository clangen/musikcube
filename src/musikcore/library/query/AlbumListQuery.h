//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004-2021 musikcube team
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
#include <musikcore/library/metadata/MetadataMapList.h>
#include <musikcore/db/Connection.h>
#include <musikcore/support/DeleteDefaults.h>

namespace musik { namespace core { namespace library { namespace query {

    class AlbumListQuery : public musik::core::library::query::QueryBase {
        public:
            EXPORT static const std::string kQueryName;

            DELETE_COPY_AND_ASSIGNMENT_DEFAULTS(AlbumListQuery)

            EXPORT AlbumListQuery(
                const std::string& filter = "");

            EXPORT AlbumListQuery(
                const std::string& fieldIdName,
                int64_t fieldIdValue,
                const std::string& filter = "");

            EXPORT AlbumListQuery(
                const category::Predicate predicate,
                const std::string& filter = "");

            EXPORT AlbumListQuery(
                const category::PredicateList predicates,
                const std::string& filter = "");

            EXPORT virtual ~AlbumListQuery();

            /* IQuery */
            EXPORT std::string Name() override { return kQueryName; }
            EXPORT musik::core::MetadataMapListPtr GetResult() noexcept;

            /* ISerializableQuery */
            EXPORT std::string SerializeQuery() override;
            EXPORT std::string SerializeResult() override;
            EXPORT void DeserializeResult(const std::string& data) override;
            EXPORT static std::shared_ptr<AlbumListQuery> DeserializeQuery(const std::string& data);

            /* AlbumListQuery */
            EXPORT musik::core::sdk::IMapList* GetSdkResult();

        protected:
            /* QueryBase */
            EXPORT bool OnRun(musik::core::db::Connection &db) override;

            std::string filter;
            category::PredicateList regular, extended;
            musik::core::MetadataMapListPtr result;
    };

} } } }
