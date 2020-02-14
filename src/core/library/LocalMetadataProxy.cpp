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
#include "LocalMetadataProxy.h"

#include <core/debug.h>
#include <core/db/ScopedTransaction.h>
#include <core/library/query/local/AlbumListQuery.h>
#include <core/library/query/local/AllCategoriesQuery.h>
#include <core/library/query/local/AppendPlaylistQuery.h>
#include <core/library/query/local/CategoryListQuery.h>
#include <core/library/query/local/CategoryTrackListQuery.h>
#include <core/library/query/local/DeletePlaylistQuery.h>
#include <core/library/query/local/SearchTrackListQuery.h>
#include <core/library/query/local/GetPlaylistQuery.h>
#include <core/library/query/local/SavePlaylistQuery.h>
#include <core/library/query/local/TrackMetadataQuery.h>
#include <core/library/query/local/TrackListQueryBase.h>
#include <core/library/track/LibraryTrack.h>
#include <core/library/LocalLibraryConstants.h>
#include <core/runtime/Message.h>
#include <core/support/Messages.h>
#include <core/support/Common.h>
#include <vector>
#include <map>

#define TAG "LocalMetadataProxy"

using namespace musik::core;
using namespace musik::core::db;
using namespace musik::core::db::local;
using namespace musik::core::library;
using namespace musik::core::runtime;
using namespace musik::core::sdk;

using PredicateList = musik::core::db::local::category::PredicateList;

/* HELPERS */

#ifdef __APPLE__
static __thread char threadLocalBuffer[4096];
#else
static thread_local char threadLocalBuffer[4096];
#endif

static inline std::string getValue(IValue* value) {
    threadLocalBuffer[0] = 0;
    if (value->GetValue(threadLocalBuffer, sizeof(threadLocalBuffer))) {
        return std::string(threadLocalBuffer);
    }
    return "";
}

static inline PredicateList toPredicateList(IValue** predicates, size_t count) {
    PredicateList predicateList;
    if (predicates && count) {
        for (size_t i = 0; i < count; i++) {
            IValue* predicate = predicates[i];
            predicateList.push_back({ getValue(predicate), predicate->GetId() });
        }
    }
    return predicateList;
}

/* QUERIES */

class ExternalIdListToTrackListQuery : public TrackListQueryBase {
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

        virtual std::shared_ptr<TrackList> GetResult() override {
            return this->result;
        }

        virtual Headers GetHeaders() override {
            return Headers();
        }

        virtual size_t GetQueryHash() override {
            return 0;
        }

    protected:
        virtual bool OnRun(musik::core::db::Connection& db) override {
            std::string sql = "SELECT id, external_id FROM tracks WHERE external_id IN(";
            for (size_t i = 0; i < externalIdCount; i++) {
                sql += (i == 0) ? "?" : ",?";
            }
            sql += ");";

            Statement query(sql.c_str(), db);

            for (size_t i = 0; i < externalIdCount; i++) {
                query.BindText((int) i, externalIds[i]);
            }

            /* gotta eat up some memory to preserve the input order. map the
            external id to the id so we can ensure we return the list in the
            same order it was requested. this is faster than executing one
            query per ID (we do this because WHERE IN() does not preserve input
            ordering... */
            struct Record { int64_t id; std::string externalId; };
            std::map<std::string, int64_t> records;

            while (query.Step() == Row) {
                records[query.ColumnText(1)] = query.ColumnInt64(0);
            }

            /* order the output here... */
            this->result = std::make_shared<TrackList>(this->library);
            auto end = records.end();
            for (size_t i = 0; i < externalIdCount; i++) {
                auto r = records.find(externalIds[i]);
                if (r != end) {
                    this->result->Add(r->second);
                }
            }

            return true;
        }

        virtual std::string Name() override {
            return "ExternalIdListToTrackListQuery";
        }

    private:
        ILibraryPtr library;
        const char** externalIds;
        size_t externalIdCount;
        std::shared_ptr<TrackList> result;
};

class RemoveFromPlaylistQuery : public LocalQueryBase {
    public:
        RemoveFromPlaylistQuery(
            ILibraryPtr library,
            int64_t playlistId,
            const char** externalIds,
            const int* sortOrders,
            size_t count)
        {
            this->library = library;
            this->playlistId = playlistId;
            this->externalIds = externalIds;
            this->sortOrders = sortOrders;
            this->count = count;
            this->updated = 0;
        }

        virtual ~RemoveFromPlaylistQuery() {
        }

        size_t GetResult() {
            return this->updated;
        }

    protected:
        virtual bool OnRun(musik::core::db::Connection& db) {
            this->updated = 0;

            ScopedTransaction transaction(db);

            {
                Statement deleteStmt(
                    "DELETE FROM playlist_tracks "
                    "WHERE playlist_id=? AND track_external_id=? AND sort_order=?",
                    db);

                for (size_t i = 0; i < count; i++) {
                    auto id = this->externalIds[i];
                    auto o = this->sortOrders[i];

                    deleteStmt.ResetAndUnbind();
                    deleteStmt.BindInt64(0, this->playlistId);
                    deleteStmt.BindText(1, this->externalIds[i]);
                    deleteStmt.BindInt32(2, this->sortOrders[i]);
                    if (deleteStmt.Step() == Done) {
                        ++this->updated;
                    }
                }
            }

            bool error = false;

            {
                Statement playlistTracks(
                    "SELECT track_external_id, sort_order FROM playlist_tracks "
                    "WHERE playlist_id=? ORDER BY sort_order ASC",
                    db);

                Statement updateStmt(
                    "UPDATE playlist_tracks "
                    "SET sort_order=? "
                    "WHERE playlist_id=? AND track_external_id=? AND sort_order=?",
                    db);

                int order = 0;

                playlistTracks.BindInt64(0, this->playlistId);
                while (playlistTracks.Step() == Row) {
                    updateStmt.ResetAndUnbind();
                    updateStmt.BindInt32(0, order++);
                    updateStmt.BindInt64(1, this->playlistId);
                    updateStmt.BindText(2, playlistTracks.ColumnText(0));
                    updateStmt.BindInt32(3, playlistTracks.ColumnInt32(1));
                    if (updateStmt.Step() != Done) {
                        error = true;
                        break;
                    }
                }
            }

            if (!error) {
                transaction.CommitAndRestart();
            }
            else {
                this->updated = 0;
            }

            if (this->updated > 0) {
                this->library->GetMessageQueue().Broadcast(
                    Message::Create(nullptr, message::PlaylistModified, playlistId));
            }

            return true;
        }

        virtual std::string Name() {
            return "RemoveFromPlaylistQuery";
        }

    private:
        ILibraryPtr library;
        int64_t playlistId;
        const char** externalIds;
        const int* sortOrders;
        size_t count;
        size_t updated;
        std::shared_ptr<TrackList> result;
};

/* DATA PROVIDER */

LocalMetadataProxy::LocalMetadataProxy(musik::core::ILibraryPtr library)
: library(library) {

}

LocalMetadataProxy::~LocalMetadataProxy() {

}

void LocalMetadataProxy::Release() {
    delete this;
}

ITrackList* LocalMetadataProxy::QueryTracks(const char* query, int limit, int offset) {
    try {
        std::shared_ptr<SearchTrackListQuery> search(
            new SearchTrackListQuery(
                this->library,
                std::string(query ? query : ""),
                TrackSortType::Album));

        if (limit >= 0) {
            search->SetLimitAndOffset(limit, offset);
        }

        this->library->Enqueue(search, ILibrary::QuerySynchronous);

        if (search->GetStatus() == IQuery::Finished) {
            return search->GetSdkResult();
        }
    }
    catch (...) {
        musik::debug::error(TAG, "QueryTracks failed");
    }

    return nullptr;
}

ITrack* LocalMetadataProxy::QueryTrackById(int64_t trackId) {
    try {
        TrackPtr target(new LibraryTrack(trackId, this->library));

        std::shared_ptr<TrackMetadataQuery> search(
            new TrackMetadataQuery(target, this->library));

        this->library->Enqueue(search, ILibrary::QuerySynchronous);

        if (search->GetStatus() == IQuery::Finished) {
            return target->GetSdkValue();
        }
    }
    catch (...) {
        musik::debug::error(TAG, "QueryTrackById failed");
    }

    return nullptr;
}

ITrack* LocalMetadataProxy::QueryTrackByExternalId(const char* externalId) {
    if (strlen(externalId)) {
        try {
            TrackPtr target(new LibraryTrack(0, this->library));
            target->SetValue("external_id", externalId);

            std::shared_ptr<TrackMetadataQuery> search(
                new TrackMetadataQuery(target, this->library));

            this->library->Enqueue(search, ILibrary::QuerySynchronous);

            if (search->GetStatus() == IQuery::Finished) {
                return target->GetSdkValue();
            }
        }
        catch (...) {
            musik::debug::error(TAG, "QueryTrackByExternalId failed");
        }
    }

    return nullptr;
}

ITrackList* LocalMetadataProxy::QueryTracksByCategory(
    const char* categoryType, int64_t selectedId, const char* filter, int limit, int offset)
{
    try {
        std::shared_ptr<TrackListQueryBase> search;

        if (std::string(categoryType) == constants::Playlists::TABLE_NAME) {
            search.reset(new GetPlaylistQuery(this->library, selectedId));
        }
        else {
            if (categoryType && strlen(categoryType) && selectedId > 0) {
                search.reset(new CategoryTrackListQuery(
                    this->library, categoryType, selectedId, filter));
            }
            else {
                search.reset(new CategoryTrackListQuery(this->library, filter));
            }
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
        musik::debug::error(TAG, "QueryTracksByCategory failed");
    }

    return nullptr;
}

ITrackList* LocalMetadataProxy::QueryTracksByCategories(
    IValue** categories, size_t categoryCount, const char* filter, int limit, int offset)
{
    try {
        PredicateList list = toPredicateList(categories, categoryCount);

        auto query = std::make_shared<CategoryTrackListQuery>(this->library, list, filter);

        if (limit >= 0) {
            query->SetLimitAndOffset(limit, offset);
        }

        this->library->Enqueue(query, ILibrary::QuerySynchronous);

        if (query->GetStatus() == IQuery::Finished) {
            return query->GetSdkResult();
        }
    }
    catch (...) {
        musik::debug::error(TAG, "QueryTracksByCategory failed");
    }

    return nullptr;
}

IValueList* LocalMetadataProxy::QueryCategory(const char* type, const char* filter) {
    return QueryCategoryWithPredicate(type, "", -1LL, filter);
}

IValueList* LocalMetadataProxy::ListCategories() {
    try {
        auto query = std::make_shared<AllCategoriesQuery>();
        this->library->Enqueue(query, ILibrary::QuerySynchronous);

        if (query->GetStatus() == IQuery::Finished) {
            return query->GetSdkResult();
        }
    }
    catch (...) {
        musik::debug::error(TAG, "ListCategories failed");
    }

    return nullptr;
}


IValueList* LocalMetadataProxy::QueryCategoryWithPredicate(
    const char* type, const char* predicateType, int64_t predicateId, const char* filter)
{
    try {
        std::string field = predicateType ? predicateType : "";

        std::shared_ptr<CategoryListQuery> search(
            new CategoryListQuery(
                type,
                { field, predicateId },
                std::string(filter ? filter : "")));

        this->library->Enqueue(search, ILibrary::QuerySynchronous);

        if (search->GetStatus() == IQuery::Finished) {
            return search->GetSdkResult();
        }
    }
    catch (...) {
        musik::debug::error(TAG, "QueryCategory failed");
    }

    return nullptr;
}

IValueList* LocalMetadataProxy::QueryCategoryWithPredicates(
    const char* type, IValue** predicates, size_t predicateCount, const char* filter)
{
    try {
        auto predicateList = toPredicateList(predicates, predicateCount);

        auto query = std::make_shared<CategoryListQuery>(
            type, predicateList, std::string(filter ? filter : ""));

        this->library->Enqueue(query, ILibrary::QuerySynchronous);

        if (query->GetStatus() == IQuery::Finished) {
            return query->GetSdkResult();
        }
    }
    catch (...) {
        musik::debug::error(TAG, "QueryCategory failed");
    }

    return nullptr;
}

IMapList* LocalMetadataProxy::QueryAlbums(
    const char* categoryIdName, int64_t categoryIdValue, const char* filter)
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
        musik::debug::error(TAG, "QueryAlbums failed");
    }

    return nullptr;
}

IMapList* LocalMetadataProxy::QueryAlbums(const char* filter) {
    return this->QueryAlbums(nullptr, -1, filter);
}

template <typename TrackListType>
static uint64_t savePlaylist(
    ILibraryPtr library,
    TrackListType trackList,
    const char* playlistName,
    const int64_t playlistId)
{
    try {
        /* replacing (and optionally renaming) an existing playlist */
        if (playlistId != 0) {
            std::shared_ptr<SavePlaylistQuery> query =
                SavePlaylistQuery::Replace(library, playlistId, trackList);

            library->Enqueue(query, ILibrary::QuerySynchronous);

            if (query->GetStatus() == IQuery::Finished) {
                if (strlen(playlistName)) {
                    query = SavePlaylistQuery::Rename(library, playlistId, playlistName);

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
                SavePlaylistQuery::Save(library, playlistName, trackList);

            library->Enqueue(query, ILibrary::QuerySynchronous);

            if (query->GetStatus() == IQuery::Finished) {
                return query->GetPlaylistId();
            }
        }
    }
    catch (...) {
        musik::debug::error(TAG, "SavePlaylist failed");
    }

    return 0;
}

int64_t LocalMetadataProxy::SavePlaylistWithIds(
    int64_t* trackIds,
    size_t trackIdCount,
    const char* playlistName,
    const int64_t playlistId)
{
    if (playlistId == 0 && (!playlistName || !strlen(playlistName))) {
        return 0;
    }

    std::shared_ptr<TrackList> trackList =
        std::make_shared<TrackList>(this->library, trackIds, trackIdCount);

    return savePlaylist(this->library, trackList, playlistName, playlistId);
}

int64_t LocalMetadataProxy::SavePlaylistWithExternalIds(
    const char** externalIds,
    size_t externalIdCount,
    const char* playlistName,
    const int64_t playlistId)
{
    if (playlistId == 0 && (!playlistName || !strlen(playlistName))) {
        return 0;
    }

    try {
        using Query = ExternalIdListToTrackListQuery;

        std::shared_ptr<Query> query =
            std::make_shared<Query>(this->library, externalIds, externalIdCount);

        library->Enqueue(query, ILibrary::QuerySynchronous);

        if (query->GetStatus() == IQuery::Finished) {
            return savePlaylist(this->library, query->GetResult(), playlistName, playlistId);
        }
    }
    catch (...) {
        musik::debug::error(TAG, "SavePlaylistWithExternalIds failed");
    }

    return 0;
}

int64_t LocalMetadataProxy::SavePlaylistWithTrackList(
    ITrackList* trackList,
    const char* playlistName,
    const int64_t playlistId)
{
    if (playlistId == 0 && (!playlistName || !strlen(playlistName))) {
        return 0;
    }

    return savePlaylist(this->library, trackList, playlistName, playlistId);
}

bool LocalMetadataProxy::RenamePlaylist(const int64_t playlistId, const char* name)
{
    if (strlen(name)) {
        try {
            std::shared_ptr<SavePlaylistQuery> query =
                SavePlaylistQuery::Rename(library, playlistId, name);

            this->library->Enqueue(query, ILibrary::QuerySynchronous);

            if (query->GetStatus() == IQuery::Finished) {
                return true;
            }
        }
        catch (...) {
            musik::debug::error(TAG, "RenamePlaylist failed");
        }
    }

    return false;
}

bool LocalMetadataProxy::DeletePlaylist(const int64_t playlistId) {
    try {
        std::shared_ptr<DeletePlaylistQuery> query =
            std::make_shared<DeletePlaylistQuery>(library, playlistId);

        this->library->Enqueue(query, ILibrary::QuerySynchronous);

        if (query->GetStatus() == IQuery::Finished) {
            return true;
        }
    }
    catch (...) {
        musik::debug::error(TAG, "DeletePlaylist failed");
    }

    return false;
}

template <typename TrackListType>
static bool appendToPlaylist(
    ILibraryPtr library,
    const int64_t playlistId,
    TrackListType trackList,
    int offset)
{
    try {
        std::shared_ptr<AppendPlaylistQuery> query =
            std::make_shared<AppendPlaylistQuery>(
                library, playlistId, trackList, offset);

        library->Enqueue(query, ILibrary::QuerySynchronous);

        if (query->GetStatus() == IQuery::Finished) {
            return true;
        }
    }
    catch (...) {
        musik::debug::error(TAG, "AppendToPlaylist failed");
    }

    return false;
}

bool LocalMetadataProxy::AppendToPlaylistWithIds(
    const int64_t playlistId,
    const int64_t* ids,
    size_t idCount,
    int offset)
{
    std::shared_ptr<TrackList> trackList =
        std::make_shared<TrackList>(this->library, ids, idCount);

    return appendToPlaylist(this->library, playlistId, trackList, offset);
}

bool LocalMetadataProxy::AppendToPlaylistWithExternalIds(
    const int64_t playlistId,
    const char** externalIds,
    size_t externalIdCount,
    int offset)
{
    using Query = ExternalIdListToTrackListQuery;

    try {
        std::shared_ptr<Query> query =
            std::make_shared<Query>(this->library, externalIds, externalIdCount);

        library->Enqueue(query, ILibrary::QuerySynchronous);

        if (query->GetStatus() == IQuery::Finished) {
            return appendToPlaylist(this->library, playlistId, query->GetResult(), offset);
        }
    }
    catch (...) {
        musik::debug::error(TAG, "AppendToPlaylistWithExternalIds failed");
    }

    return 0;

}

bool LocalMetadataProxy::AppendToPlaylistWithTrackList(
    const int64_t playlistId, ITrackList* trackList, int offset)
{
    static auto deleter = [](musik::core::sdk::ITrackList* trackList) {};

    bool result = appendToPlaylist(
        this->library, playlistId, trackList, offset);

    return result;
}

size_t LocalMetadataProxy::RemoveTracksFromPlaylist(
    const int64_t playlistId,
    const char** externalIds,
    const int* sortOrders,
    int count)
{
    try {
        auto query = std::make_shared<RemoveFromPlaylistQuery>(
            this->library, playlistId, externalIds, sortOrders, count);

        library->Enqueue(query, ILibrary::QuerySynchronous);

        if (query->GetStatus() == IQuery::Finished) {
            return query->GetResult();
        }
    }
    catch (...) {
        musik::debug::error(TAG, "RemoveTracksFromPlaylist failed");
    }

    return 0;
}

ITrackList* LocalMetadataProxy::QueryTracksByExternalId(
    const char** externalIds, size_t externalIdCount)
{
    try {
        auto query = std::make_shared<ExternalIdListToTrackListQuery>(
            this->library, externalIds, externalIdCount);

        library->Enqueue(query, ILibrary::QuerySynchronous);

        if (query->GetStatus() == IQuery::Finished) {
            return query->GetSdkResult();
        }
    }
    catch (...) {
        musik::debug::error(TAG, "QueryTracksByExternalId failed");
    }

    return nullptr;
}
