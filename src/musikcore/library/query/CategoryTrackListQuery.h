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

#include <musikcore/db/Connection.h>
#include <musikcore/library/track/Track.h>
#include <musikcore/library/QueryBase.h>
#include <musikcore/library/query/util/CategoryQueryUtil.h>
#include <musikcore/library/query/util/TrackSort.h>
#include <musikcore/db/Statement.h>

#include "TrackListQueryBase.h"

namespace musik { namespace core { namespace library { namespace query {

    class CategoryTrackListQuery : public TrackListQueryBase {
        public:
            EXPORT static const std::string kQueryName;

            DELETE_CLASS_DEFAULTS(CategoryTrackListQuery)

            EXPORT CategoryTrackListQuery(
                musik::core::ILibraryPtr library,
                const std::string& filter = "",
                TrackSortType sortType = TrackSortType::Album);

            EXPORT CategoryTrackListQuery(
                musik::core::ILibraryPtr library,
                const std::string& column,
                int64_t id,
                const std::string& filter = "",
                TrackSortType sortType = TrackSortType::Album);

            EXPORT CategoryTrackListQuery(
                musik::core::ILibraryPtr library,
                const category::Predicate predicate,
                const std::string& filter = "",
                TrackSortType sortType = TrackSortType::Album);

            EXPORT CategoryTrackListQuery(
                musik::core::ILibraryPtr library,
                const category::PredicateList predicates,
                const std::string& filter = "",
                TrackSortType sortType = TrackSortType::Album);

            /* IQuery */
            EXPORT std::string Name() override { return kQueryName; }

            /* TrackListQueryBase */
            EXPORT Result GetResult() noexcept override;
            EXPORT Headers GetHeaders() noexcept override;
            EXPORT Durations GetDurations() noexcept override;
            EXPORT size_t GetQueryHash() noexcept override;

            /* ISerializableQuery */
            EXPORT std::string SerializeQuery() override;
            EXPORT std::string SerializeResult() override;
            EXPORT void DeserializeResult(const std::string& data) override;
            EXPORT static std::shared_ptr<CategoryTrackListQuery> DeserializeQuery(
                musik::core::ILibraryPtr library, const std::string& data);

        protected:
            /* QueryBase */
            EXPORT bool OnRun(musik::core::db::Connection &db) override;

        private:
            enum class Type: int { Playlist = 0, Regular = 1 };

            void ScanPredicateListsForQueryType();

            void PlaylistQuery(musik::core::db::Connection &db);
            void RegularQuery(musik::core::db::Connection &db);
            void ProcessResult(musik::core::db::Statement& stmt);

            /* regular instance variables */
            musik::core::ILibraryPtr library;
            bool parseHeaders;
            size_t hash;
            std::string orderBy;
            Type type;

            /* serialized result fields */
            Result result;
            Headers headers;
            Durations durations;

            /* serialized query fields */
            std::string filter;
            category::PredicateList regular, extended;
            TrackSortType sortType;
    };

} } } }
