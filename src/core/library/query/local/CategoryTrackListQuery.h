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

#include <core/db/Connection.h>
#include <core/library/track/Track.h>
#include <core/library/query/local/LocalQueryBase.h>
#include <core/library/query/local/util/CategoryQueryUtil.h>
#include <core/library/query/local/util/TrackSort.h>
#include <core/db/Statement.h>

#include "TrackListQueryBase.h"

namespace musik { namespace core { namespace db { namespace local {

    class CategoryTrackListQuery : public TrackListQueryBase {
        public:
            CategoryTrackListQuery(
                musik::core::ILibraryPtr library,
                const std::string& filter = "",
                TrackSortType sortType = TrackSortType::Album);

            CategoryTrackListQuery(
                musik::core::ILibraryPtr library,
                const std::string& column,
                int64_t id,
                const std::string& filter = "",
                TrackSortType sortType = TrackSortType::Album);

            CategoryTrackListQuery(
                musik::core::ILibraryPtr library,
                const category::Predicate predicate,
                const std::string& filter = "",
                TrackSortType sortType = TrackSortType::Album);

            CategoryTrackListQuery(
                musik::core::ILibraryPtr library,
                const category::PredicateList predicates,
                const std::string& filter = "",
                TrackSortType sortType = TrackSortType::Album);

            virtual ~CategoryTrackListQuery();

            virtual std::string Name() { return "CategoryTrackListQuery"; }

            virtual Result GetResult();
            virtual Headers GetHeaders();
            virtual size_t GetQueryHash();

        protected:
            virtual bool OnRun(musik::core::db::Connection &db);

        private:
            enum Type { Playlist, Regular };

            void PlaylistQuery(musik::core::db::Connection &db);
            void RegularQuery(musik::core::db::Connection &db);
            void ProcessResult(musik::core::db::Statement& stmt);

            musik::core::ILibraryPtr library;
            bool parseHeaders;
            Result result;
            Headers headers;
            Type type;
            category::PredicateList regular, extended;
            size_t hash;
            std::string orderBy;
            std::string filter;
    };

} } } }
