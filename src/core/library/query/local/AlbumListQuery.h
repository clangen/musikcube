//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004-2019 musikcube team
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

#include <core/library/query/local/LocalQueryBase.h>
#include <core/library/query/local/util/CategoryQueryUtil.h>
#include <core/library/metadata/MetadataMapList.h>
#include <core/db/Connection.h>

namespace musik { namespace core { namespace db { namespace local {

    class AlbumListQuery : public musik::core::db::LocalQueryBase {
        public:
            AlbumListQuery(
                const std::string& filter = "");

            AlbumListQuery(
                const std::string& fieldIdName,
                int64_t fieldIdValue,
                const std::string& filter = "");

            AlbumListQuery(
                const category::Predicate predicate,
                const std::string& filter = "");

            AlbumListQuery(
                const category::PredicateList predicates,
                const std::string& filter = "");

            virtual ~AlbumListQuery();

            std::string Name() { return "AlbumListQuery"; }

            musik::core::MetadataMapListPtr GetResult();
            musik::core::sdk::IMapList* GetSdkResult();

        protected:
            virtual bool OnRun(musik::core::db::Connection &db);

            std::string filter;
            category::PredicateList regular, extended;
            musik::core::MetadataMapListPtr result;
    };

} } } }
