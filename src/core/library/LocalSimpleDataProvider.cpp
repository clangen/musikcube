//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2007-2017 musikcube team
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
#include <core/library/query/local/DeletePlaylistQuery.h>
#include <core/library/query/local/SearchTrackListQuery.h>
#include <core/library/query/local/GetPlaylistQuery.h>
#include <core/library/query/local/SavePlaylistQuery.h>
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

class ExternalIdListToTrackListQuery : public LocalQueryBase {
    public:
        ExternalIdListToTrackListQuery(
            ILibraryPtr library,
            const char** externalIds,
            size_t externalIdCount)
        {
            this->library = library;
            this->externalIds = externalIds;
            this->externalIdCount = externalIdCount;
        }

        virtual ~ExternalIdListToTrackListQuery() {
        }

        std::shared_ptr<TrackList> Result() {
            return this->result;
        }

    protected:
        virtual bool OnRun(musik::core::db::Connection& db) {
            std::string sql = "SELECT id FROM tracks WHERE external_id IN(";
            for (size_t i = 0; i < externalIdCount; i++) {
                sql += (i == 0) ? "?" : ",?";
            }
            sql += ");";

            Statement query(sql.c_str(), db);

            for (size_t i = 0; i < externalIdCount; i++) {
                query.BindText(i, externalIds[i]);
            }

            this->result = std::make_shared<TrackList>(this->library);

            while (query.Step() == Row) {
                result->Add(query.ColumnInt64(0));
            }

            return true;
        }

        virtual std::string Name() {
            return "ExternalIdListToTrackListQuery";
        }

    private:
        ILibraryPtr library;
        const char** externalIds;
        size_t externalIdCount;
        std::shared_ptr<TrackList> result;
};

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

IRetainedTrack* LocalSimpleDataProvider::QueryTrackById(int64_t trackId) {
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
    int64_t selectedId,
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
    int64_t categoryIdValue,
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

static uint64_t savePlaylist(
    ILibraryPtr library,
    std::shared_ptr<TrackList> trackList,
    const char* playlistName,
    const uint64_t playlistId)
{
    try {
        /* replacing (and optionally renaming) an existing playlist */
        if (playlistId != 0) {
            std::shared_ptr<SavePlaylistQuery> query =
                SavePlaylistQuery::Replace(playlistId, trackList);

            library->Enqueue(query, ILibrary::QuerySynchronous);

            if (query->GetStatus() == IQuery::Finished) {
                if (strlen(playlistName)) {
                    query = SavePlaylistQuery::Rename(playlistId, playlistName);

                    library->Enqueue(query, ILibrary::QuerySynchronous);

                    if (query->GetStatus() == IQuery::Finished) {
                        return playlistId;
                    }
                }
                else {
                    return playlistId;
                }
            }
        }
        else {
            std::shared_ptr<SavePlaylistQuery> query =
                SavePlaylistQuery::Save(playlistName, trackList);

            library->Enqueue(query, ILibrary::QuerySynchronous);

            if (query->GetStatus() == IQuery::Finished) {
                return query->GetPlaylistId();
            }
        }
    }
    catch (...) {
        musik::debug::err(TAG, "SavePlaylist failed");
    }

    return 0;
}

uint64_t LocalSimpleDataProvider::SavePlaylistWithIds(
    int64_t* trackIds,
    size_t trackIdCount,
    const char* playlistName,
    const uint64_t playlistId)
{
    if (playlistId == 0 && (!playlistName || !strlen(playlistName))) {
        return 0;
    }

    std::shared_ptr<TrackList> trackList =
        std::make_shared<TrackList>(this->library, trackIds, trackIdCount);

    return savePlaylist(this->library, trackList, playlistName, playlistId);
}

uint64_t LocalSimpleDataProvider::SavePlaylistWithExternalIds(
    const char** externalIds,
    size_t externalIdCount,
    const char* playlistName,
    const uint64_t playlistId)
{
    if (playlistId == 0 && (!playlistName || !strlen(playlistName))) {
        return 0;
    }

    using Query = ExternalIdListToTrackListQuery;

    std::shared_ptr<Query> query =
        std::make_shared<Query> (this->library, externalIds, externalIdCount);

    library->Enqueue(query, ILibrary::QuerySynchronous);

    if (query->GetStatus() == IQuery::Finished) {
        return savePlaylist(this->library, query->Result(), playlistName, playlistId);
    }

    return 0;
}

bool LocalSimpleDataProvider::RenamePlaylist(const uint64_t playlistId, const char* name)
{
    try {
        std::shared_ptr<SavePlaylistQuery> query =
            SavePlaylistQuery::Rename(playlistId, name);

        this->library->Enqueue(query, ILibrary::QuerySynchronous);

        if (query->GetStatus() == IQuery::Finished) {
            return true;
        }
    }
    catch (...) {
        musik::debug::err(TAG, "RenamePlaylist failed");
    }

    return false;
}

bool LocalSimpleDataProvider::DeletePlaylist(const uint64_t playlistId) {
    try {
        std::shared_ptr<DeletePlaylistQuery> query =
            std::make_shared<DeletePlaylistQuery>(playlistId);

        this->library->Enqueue(query, ILibrary::QuerySynchronous);

        if (query->GetStatus() == IQuery::Finished) {
            return true;
        }
    }
    catch (...) {
        musik::debug::err(TAG, "DeletePlaylist failed");
    }

    return false;
}