//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2007-2016 musikcube team
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
#include "LocalSimpleDataProvider.h"

#include <core/debug.h>

#include <core/library/query/local/AlbumListQuery.h>
#include <core/library/query/local/CategoryListQuery.h>
#include <core/library/query/local/CategoryTrackListQuery.h>
#include <core/library/query/local/SearchTrackListQuery.h>
#include <core/library/query/local/GetPlaylistQuery.h>
#include <core/library/query/local/TrackMetadataQuery.h>
#include <core/library/track/LibraryTrack.h>
#include <core/library/track/RetainedTrack.h>
#include <core/library/LocalLibraryConstants.h>


#define TAG "LocalSimpleDataProvider"

using namespace musik::core;
using namespace musik::core::db;
using namespace musik::core::db::local;
using namespace musik::core::library;
using namespace musik::core::sdk;

LocalSimpleDataProvider::LocalSimpleDataProvider(musik::core::ILibraryPtr library)
: library(library) {

}

LocalSimpleDataProvider::~LocalSimpleDataProvider() {

}

ITrackList* LocalSimpleDataProvider::QueryTracks(const char* query, int limit, int offset) {
    try {
        std::shared_ptr<SearchTrackListQuery> search(
            new SearchTrackListQuery(this->library, std::string(query ? query : "")));

        if (limit >= 0) {
            search->SetLimitAndOffset(limit, offset);
        }

        this->library->Enqueue(search, ILibrary::QuerySynchronous);

        if (search->GetStatus() == IQuery::Finished) {
            return search->GetSdkResult();
        }
    }
    catch (...) {
        musik::debug::err(TAG, "QueryTracks failed");
    }

    return nullptr;
}

IRetainedTrack* LocalSimpleDataProvider::QueryTrackById(uint64_t trackId) {
    try {
        TrackPtr target(new LibraryTrack(trackId, this->library));

        std::shared_ptr<TrackMetadataQuery> search(
            new TrackMetadataQuery(target, this->library));

        this->library->Enqueue(search, ILibrary::QuerySynchronous);

        if (search->GetStatus() == IQuery::Finished) {
            return new RetainedTrack(target);
        }
    }
    catch (...) {
        musik::debug::err(TAG, "QueryTrackById failed");
    }

    return nullptr;
}

IRetainedTrack* LocalSimpleDataProvider::QueryTrackByExternalId(const char* externalId) {
    if (strlen(externalId)) {
        try {
            TrackPtr target(new LibraryTrack(0, this->library));
            target->SetValue("external_id", externalId);

            std::shared_ptr<TrackMetadataQuery> search(
                new TrackMetadataQuery(target, this->library));

            this->library->Enqueue(search, ILibrary::QuerySynchronous);

            if (search->GetStatus() == IQuery::Finished) {
                return new RetainedTrack(target);
            }
        }
        catch (...) {
            musik::debug::err(TAG, "QueryTrackByExternalId failed");
        }
    }

    return nullptr;
}

ITrackList* LocalSimpleDataProvider::QueryTracksByCategory(
    const char* categoryType,
    uint64_t selectedId,
    const char* filter,
    int limit,
    int offset)
{
    try {
        std::shared_ptr<TrackListQueryBase> search;

        if (std::string(categoryType) == constants::Playlists::TABLE_NAME) {
            search.reset(new GetPlaylistQuery(this->library, selectedId));
        }
        else {
            search.reset(new CategoryTrackListQuery(
                this->library, categoryType, selectedId, filter));
        }

        if (limit >= 0) {
            search->SetLimitAndOffset(limit, offset);
        }

        this->library->Enqueue(search, ILibrary::QuerySynchronous);

        if (search->GetStatus() == IQuery::Finished) {
            return search->GetSdkResult();
        }
    }
    catch (...) {
        musik::debug::err(TAG, "QueryTracksByCategory failed");
    }

    return nullptr;
}

IMetadataValueList* LocalSimpleDataProvider::QueryCategory(const char* type, const char* filter) {
    try {
        std::shared_ptr<CategoryListQuery> search(
            new CategoryListQuery(type, std::string(filter ? filter : "")));

        this->library->Enqueue(search, ILibrary::QuerySynchronous);

        if (search->GetStatus() == IQuery::Finished) {
            return search->GetSdkResult();
        }
    }
    catch (...) {
        musik::debug::err(TAG, "QueryCategory failed");
    }

    return nullptr;
}

IMetadataMapList* LocalSimpleDataProvider::QueryAlbums(
    const char* categoryIdName,
    uint64_t categoryIdValue,
    const char* filter)
{
    try {
        std::shared_ptr<AlbumListQuery> search(new AlbumListQuery(
            std::string(categoryIdName ? categoryIdName : ""),
            categoryIdValue,
            std::string(filter ? filter : "")));

        this->library->Enqueue(search, ILibrary::QuerySynchronous);

        if (search->GetStatus() == IQuery::Finished) {
            return search->GetSdkResult();
        }
    }
    catch (...) {
        musik::debug::err(TAG, "QueryAlbums failed");
    }

    return nullptr;
}

IMetadataMapList* LocalSimpleDataProvider::QueryAlbums(const char* filter) {
    return this->QueryAlbums(nullptr, -1, filter);
}