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

#include "pch.hpp"

#include "QueryRegistry.h"
#include <core/library/query/AlbumListQuery.h>
#include <core/library/query/AllCategoriesQuery.h>
#include <core/library/query/AppendPlaylistQuery.h>
#include <core/library/query/GetPlaylistQuery.h>
#include <core/library/query/CategoryListQuery.h>
#include <core/library/query/CategoryTrackListQuery.h>
#include <core/library/query/DeletePlaylistQuery.h>
#include <core/library/query/DirectoryTrackListQuery.h>
#include <core/library/query/LyricsQuery.h>
#include <core/library/query/MarkTrackPlayedQuery.h>
#include <core/library/query/NowPlayingTrackListQuery.h>
#include <core/library/query/PersistedPlayQueueQuery.h>
#include <core/library/query/SavePlaylistQuery.h>
#include <core/library/query/SearchTrackListQuery.h>
#include <core/library/query/SetTrackRatingQuery.h>
#include <core/library/query/TrackMetadataBatchQuery.h>
#include <core/library/query/TrackMetadataQuery.h>

using namespace musik::core;
using namespace musik::core::db;
using namespace musik::core::library::query;

namespace musik { namespace core { namespace library {

    namespace QueryRegistry {
        std::shared_ptr<ISerializableQuery> CreateLocalQueryFor(
            const std::string& name, const std::string& data, ILibraryPtr library)
        {
            if (name == AlbumListQuery::kQueryName) {
                return AlbumListQuery::DeserializeQuery(data);
            }
            if (name == AllCategoriesQuery::kQueryName) {
                return AllCategoriesQuery::DeserializeQuery(data);
            }
            if (name == AppendPlaylistQuery::kQueryName) {
                return AppendPlaylistQuery::DeserializeQuery(library, data);
            }
            if (name == GetPlaylistQuery::kQueryName) {
                return GetPlaylistQuery::DeserializeQuery(library, data);
            }
            if (name == CategoryListQuery::kQueryName) {
                return CategoryListQuery::DeserializeQuery(data);
            }
            if (name == CategoryTrackListQuery::kQueryName) {
                return CategoryTrackListQuery::DeserializeQuery(library, data);
            }
            if (name == DeletePlaylistQuery::kQueryName) {
                return DeletePlaylistQuery::DeserializeQuery(library, data);
            }
            if (name == DirectoryTrackListQuery::kQueryName) {
                return DirectoryTrackListQuery::DeserializeQuery(library, data);
            }
            if (name == LyricsQuery::kQueryName) {
                return LyricsQuery::DeserializeQuery(data);
            }
            if (name == MarkTrackPlayedQuery::kQueryName) {
                return MarkTrackPlayedQuery::DeserializeQuery(data);
            }
            if (name == SavePlaylistQuery::kQueryName) {
                return SavePlaylistQuery::DeserializeQuery(library, data);
            }
            if (name == SearchTrackListQuery::kQueryName) {
                return SearchTrackListQuery::DeserializeQuery(library, data);
            }
            if (name == SetTrackRatingQuery::kQueryName) {
                return SetTrackRatingQuery::DeserializeQuery(data);
            }
            if (name == TrackMetadataBatchQuery::kQueryName) {
                return TrackMetadataBatchQuery::DeserializeQuery(library, data);
            }
            if (name == TrackMetadataQuery::kQueryName) {
                return TrackMetadataQuery::DeserializeQuery(library, data);
            }
            return std::shared_ptr<ISerializableQuery>();
        }

        bool IsLocalOnlyQuery(const std::string& queryName) {
            static const std::set<std::string> sLocalOnlyQuerys = {
                NowPlayingTrackListQuery::kQueryName,
                PersistedPlayQueueQuery::kQueryName
            };

            return sLocalOnlyQuerys.find(queryName) != sLocalOnlyQuerys.end();
        }
    }

} } }
