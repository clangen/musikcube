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

#include "pch.hpp"
#include <sqlite3.h>
#include "musikcore/sdk/Filesystem.h"

#include <musikcore/library/LocalLibrary.h>
#include <musikcore/config.h>
#include <musikcore/library/QueryBase.h>
#include <musikcore/support/Common.h>
#include <musikcore/support/Preferences.h>
#include <musikcore/library/Indexer.h>
#include <common/runtime/Message.h>
#include <musikcore/debug.h>

#include <initializer_list>
#include <limits>
#include <filesystem>

static const std::string TAG = "LocalLibrary";
static bool scheduleSyncDueToDbUpgrade = false;

using musik::core::sdk::fs::pathToUtf8;

namespace fs = std::filesystem;

using namespace musik::core;
using namespace musik::core::library;
using namespace musik::core::runtime;
using namespace std::chrono;

#define DATABASE_VERSION 10
#define VERBOSE_LOGGING 1
#define MESSAGE_QUERY_COMPLETED 5000

class LocalResourceLocator: public ILibrary::IResourceLocator {
    public:
        std::string GetTrackUri(
            musik::core::sdk::ITrack* track,
            const std::string& defaultUri) override
        {
            char buffer[4096];
            const int size = track->Uri(buffer, sizeof(buffer));
            return size > 0 ? std::string(buffer) : defaultUri;
        }
} sResourceLocator;

class LocalLibrary::QueryCompletedMessage: public Message {
    public:
        using QueryContextPtr = LocalLibrary::QueryContextPtr;

        QueryCompletedMessage(IMessageTarget* target, QueryContextPtr context)
        : Message(target, MESSAGE_QUERY_COMPLETED, 0, 0) {
            this->context = context;
        }

        QueryContextPtr GetContext() noexcept {
            return this->context;
        }

    private:
        QueryContextPtr context;
};

ILibraryPtr LocalLibrary::Create(std::string name, int id, MessageQueue* messageQueue) {
    ILibraryPtr lib(new LocalLibrary(name, id, messageQueue));
    return lib;
}

LocalLibrary::LocalLibrary(std::string name, int id, MessageQueue* messageQueue)
: name(name)
, id(id)
, exit(false)
, messageQueue(messageQueue) {
    this->identifier = std::to_string(id);

    const auto filename = this->GetDatabaseFilename();
    if (this->db.Open(filename) != SQLITE_OK || sqlite3_db_readonly(this->db.NativeHandle(), "main") != 0) {
        throw fs::filesystem_error("Could not open writable library database", fs::u8path(filename),
            std::make_error_code(std::errc::io_error));
    }
    LocalLibrary::CreateDatabase(this->db);

    auto indexer = std::make_unique<core::Indexer>(
        this->GetLibraryDirectory(),
        this->GetDatabaseFilename());

    if (scheduleSyncDueToDbUpgrade) {
        indexer->Schedule(IIndexer::SyncType::Local);
    }

    /* register only once schema and indexer startup have succeeded. */
    if (this->messageQueue) {
        this->messageQueue->Register(this);
    }
    try {
        this->thread = new std::thread(std::bind(&LocalLibrary::ThreadProc, this));
    }
    catch (...) {
        if (this->messageQueue) {
            this->messageQueue->Unregister(this);
        }
        throw;
    }
    this->indexer = indexer.release();
}

LocalLibrary::~LocalLibrary() {
    this->Close();
    if (this->messageQueue) {
        this->messageQueue->Unregister(this);
    }
}

int LocalLibrary::Id() {
    return this->id;
}

const std::string& LocalLibrary::Name() {
    return this->name;
}

void LocalLibrary::Close() {
    std::thread* thread = nullptr;

    {
        std::unique_lock<std::recursive_mutex> lock(this->mutex);

        delete this->indexer;
        this->indexer = nullptr;

        if (this->thread) {
            thread = this->thread;
            this->thread = nullptr;
            this->queryQueue.clear();
            this->exit = true;
        }
    }

    if (thread) {
        this->queueCondition.notify_all();
        thread->join();
        delete thread;
    }
}

bool LocalLibrary::IsConfigured() {
    std::vector<std::string> paths;
    Indexer()->GetPaths(paths);
    return paths.size() > 0;
}

std::string LocalLibrary::GetLibraryDirectory() {
    std::string directory(musik::core::GetDataDirectory());

    if (!this->identifier.empty()) {
        directory.append(this->identifier + "/" );
    }

    fs::path dir(fs::u8path(directory));
    if(!fs::exists(dir)){
        fs::create_directories(dir);
    }

    directory = pathToUtf8(dir);

    return directory;
}

std::string LocalLibrary::GetDatabaseFilename() {
    return this->GetLibraryDirectory() + "musik.db";
}

int LocalLibrary::Enqueue(QueryPtr query, Callback callback) {
    return this->EnqueueAndWait(query, 0LL, callback);
}

int LocalLibrary::EnqueueAndWait(QueryPtr query, size_t timeoutMs, Callback callback) {
    LocalQueryPtr localQuery = std::dynamic_pointer_cast<LocalQuery>(query);

    if (localQuery) {
        std::unique_lock<std::recursive_mutex> lock(this->mutex);

        if (this->exit) { /* closed */
            return -1;
        }

        if (VERBOSE_LOGGING) {
            musik::debug::info(TAG, "query '" + localQuery->Name() + "' enqueued");
        }

        auto context = std::make_shared<QueryContext>();
        context->query = localQuery;
        context->callback = callback;

        if (timeoutMs == kWaitIndefinite) {
            this->RunQuery(context, !(localQuery->GetOptions() & LocalQuery::SuppressCompletionBroadcast));
        }
        else {
            queryQueue.push_back(context);
            queueCondition.notify_all();

            if (timeoutMs > 0) {
                while (!this->exit && (
                    context->query->GetStatus() == db::IQuery::Idle ||
                    context->query->GetStatus() == db::IQuery::Running)
                    )
                {
                    const auto result = this->queueCondition.wait_for(lock, timeoutMs * milliseconds(1));
                    if (result == std::cv_status::timeout) {
                        break;
                    }
                }
            }
        }

        return localQuery->GetId();
    }

    return -1;
}

LocalLibrary::QueryContextPtr LocalLibrary::GetNextQuery() {
    std::unique_lock<std::recursive_mutex> lock(this->mutex);
    while (!this->queryQueue.size() && !this->exit) {
        this->queueCondition.wait(lock);
    }

    if (this->exit) {
        return QueryContextPtr();
    }
    else {
        auto front = queryQueue.front();
        queryQueue.pop_front();
        return front;
    }
}

void LocalLibrary::ThreadProc() {
    while (!this->exit) {
        auto query = GetNextQuery();
        if (query) {
            this->RunQuery(query);
            this->queueCondition.notify_all();
        }
    }
}

void LocalLibrary::RunQuery(QueryContextPtr context, bool notify) {
    if (context) {
        auto query = context->query;

        if (VERBOSE_LOGGING) {
            musik::debug::info(TAG, "query '" + query->Name() + "' running");
        }

        query->Run(this->db);

        if (notify) {
            if (this->messageQueue) {
                this->messageQueue->Post(std::make_shared<QueryCompletedMessage>(this, context));
            }
            else {
                this->QueryCompleted(query.get());
            }
        }
        else if (context->callback) {
            context->callback(context->query);
        }

        if (VERBOSE_LOGGING) {
            musik::debug::info(TAG, u8fmt(
                "query '%s' finished with status=%d",
                query->Name().c_str(),
                query->GetStatus()));
        }
    }
}

void LocalLibrary::SetMessageQueue(musik::core::runtime::IMessageQueue& queue) {
    if (this->messageQueue && this->messageQueue != &queue) {
        this->messageQueue->Unregister(this);
    }
    this->messageQueue = &queue;
    queue.Register(this);
}

ILibrary::IResourceLocator& LocalLibrary::GetResourceLocator() {
    return sResourceLocator;
}

void LocalLibrary::ProcessMessage(musik::core::runtime::IMessage &message) {
    if (message.Type() == MESSAGE_QUERY_COMPLETED) {
        auto context = dynamic_cast<QueryCompletedMessage*>(&message)->GetContext();
        auto query = context->query;

        this->QueryCompleted(context->query.get());

        if (context->callback) {
            context->callback(query);
        }
    }
}

musik::core::IIndexer* LocalLibrary::Indexer() {
    return this->indexer;
}

static void requireSchema(bool valid) {
    if (!valid) {
        throw fs::filesystem_error("Could not initialize library schema", std::make_error_code(std::errc::io_error));
    }
}

static void executeSchema(db::Connection& db, const char* sql) {
    requireSchema(db.Execute(sql) == db::Okay);
}

static bool hasColumn(db::Connection& db, const char* table, const char* column) {
    return sqlite3_table_column_metadata(db.NativeHandle(), "main", table, column,
        nullptr, nullptr, nullptr, nullptr, nullptr) == SQLITE_OK;
}

static void upgradeV1toV2(db::Connection &db) {
    /* ensure each track has an external_id */
    {
        int step;
        db::Statement update("UPDATE tracks SET external_id=? WHERE id=?", db);
        db::Statement query("SELECT id FROM tracks WHERE coalesce(external_id, '') == ''", db);
        while ((step = query.Step()) == db::Row) {
            const auto id = query.ColumnInt64(0);
            update.Reset();
            update.BindText(0, "local://" + std::to_string(id));
            update.BindInt64(1, id);
            requireSchema(update.Step() == db::Done);
        }
        requireSchema(step == db::Done);
    }

    /* update playlist_tracks.track_external_id */
    if (!hasColumn(db, "playlist_tracks", "track_id")) {
        return;
    }

    const char* externalIdUpdate =
        "UPDATE playlist_tracks "
        "SET track_external_id = coalesce(( "
        "  SELECT tracks.external_id"
        "  FROM tracks"
        "  WHERE playlist_tracks.track_id = tracks.id), track_external_id);";

    db::Statement update(externalIdUpdate, db);
    requireSchema(update.Step() == db::Done);
}

static void upgradeV2ToV3(db::Connection& db) {
    executeSchema(db, "DROP TABLE IF EXISTS albums");
    executeSchema(db, "DELETE from tracks");

    executeSchema(db,
        "CREATE TABLE IF NOT EXISTS albums ("
        "id INTEGER PRIMARY KEY,"
        "name TEXT default '',"
        "thumbnail_id INTEGER default 0,"
        "sort_order INTEGER DEFAULT 0)");

    scheduleSyncDueToDbUpgrade = true;
}

static void upgradeV3ToV4(db::Connection& db) {
    executeSchema(db, "UPDATE tracks SET filetime=0");
    scheduleSyncDueToDbUpgrade = true;
}

static void upgradeV4ToV5(db::Connection& db) {
    executeSchema(db, "UPDATE tracks SET filetime=0");
    executeSchema(db, "UPDATE playlist_tracks SET sort_order=sort_order-1");
    scheduleSyncDueToDbUpgrade = true;
}

static void upgradeV5ToV6(db::Connection& db) {
    executeSchema(db, "UPDATE tracks SET filetime=0");

    executeSchema(db,
        "CREATE TABLE IF NOT EXISTS replay_gain ("
        "id INTEGER PRIMARY KEY,"
        "track_id INTEGER DEFAULT 0,"
        "album_gain REAL default 1.0,"
        "album_peak REAL default 1.0,"
        "track_gain REAL default 1.0,"
        "track_peak REAL default 1.0)");

    scheduleSyncDueToDbUpgrade = true;
}

static void upgradeV6ToV7(db::Connection& db) {
    requireSchema(LocalLibrary::InvalidateTrackMetadata(db));
    scheduleSyncDueToDbUpgrade = true;
}

static void upgradeV7ToV8(db::Connection& db) {
    if (!hasColumn(db, "tracks", "directory_id")) {
        executeSchema(db, "ALTER TABLE tracks ADD COLUMN directory_id INTEGER");
    }
    requireSchema(LocalLibrary::InvalidateTrackMetadata(db));
    scheduleSyncDueToDbUpgrade = true;
}

static void upgradeV8ToV9(db::Connection& db) {
    if (!hasColumn(db, "tracks", "rating")) {
        executeSchema(db, "ALTER TABLE tracks ADD COLUMN rating INTEGER DEFAULT 0");
    }
    if (!hasColumn(db, "tracks", "last_played")) {
        executeSchema(db, "ALTER TABLE tracks ADD COLUMN last_played REAL DEFAULT null");
    }
    if (!hasColumn(db, "tracks", "play_count")) {
        executeSchema(db, "ALTER TABLE tracks ADD COLUMN play_count INTEGER DEFAULT 0");
    }
    if (!hasColumn(db, "tracks", "date_added")) {
        executeSchema(db, "ALTER TABLE tracks ADD COLUMN date_added REAL DEFAULT null");
    }
    if (!hasColumn(db, "tracks", "date_updated")) {
        executeSchema(db, "ALTER TABLE tracks ADD COLUMN date_updated REAL DEFAULT null");
    }
}

static void upgradeV9ToV10(db::Connection& db) {
    executeSchema(db, "UPDATE tracks set disc=1 where disc is null or disc like \"\"");
}

static void setVersion(db::Connection& db, int version) {
    executeSchema(db, "DELETE FROM version");
    db::Statement stmt("INSERT INTO version VALUES(?)", db);
    stmt.BindInt32(0, version);
    requireSchema(stmt.Step() == db::Done);
}

static void createSchema(db::Connection& db) {
    const bool existing = hasColumn(db, "tracks", "id");
    /* tracks */
    executeSchema(db,
        "CREATE TABLE IF NOT EXISTS tracks ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "track INTEGER DEFAULT 0,"
            "disc TEXT DEFAULT '1',"
            "bpm REAL DEFAULT 0,"
            "duration INTEGER DEFAULT 0,"
            "filesize INTEGER DEFAULT 0,"
            "visual_genre_id INTEGER DEFAULT 0,"
            "visual_artist_id INTEGER DEFAULT 0,"
            "album_artist_id INTEGER DEFAULT 0,"
            "path_id INTEGER,"
            "directory_id INTEGER,"
            "album_id INTEGER DEFAULT 0,"
            "title TEXT DEFAULT '',"
            "filename TEXT DEFAULT '',"
            "filetime INTEGER DEFAULT 0,"
            "thumbnail_id INTEGER DEFAULT 0,"
            "source_id INTEGER DEFAULT 0,"
            "visible INTEGER DEFAULT 1,"
            "external_id TEXT DEFAULT null,"
            "rating INTEGER DEFAULT 0,"
            "last_played REAL DEFAULT null,"
            "play_count INTEGER DEFAULT 0,"
            "date_added REAL DEFAULT null,"
            "date_updated REAL DEFAULT null)");

    /* genres tables */
    executeSchema(db,
        "CREATE TABLE IF NOT EXISTS genres ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "name TEXT default '',"
            "aggregated INTEGER DEFAULT 0,"
            "sort_order INTEGER DEFAULT 0)");

    executeSchema(db,
        "CREATE TABLE IF NOT EXISTS track_genres ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "track_id INTEGER DEFAULT 0,"
            "genre_id INTEGER DEFAULT 0)");

    /* artist tables */
    executeSchema(db,
        "CREATE TABLE IF NOT EXISTS artists ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "name TEXT default '',"
            "aggregated INTEGER DEFAULT 0,"
            "sort_order INTEGER DEFAULT 0)");

    executeSchema(db,
        "CREATE TABLE IF NOT EXISTS track_artists ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "track_id INTEGER DEFAULT 0,"
            "artist_id INTEGER DEFAULT 0)");

    /* arbitrary metadata */
    executeSchema(db,
        "CREATE TABLE IF NOT EXISTS meta_keys ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "name TEXT)");

    executeSchema(db,
        "CREATE TABLE IF NOT EXISTS meta_values ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "meta_key_id INTEGER DEFAULT 0,"
            "sort_order INTEGER DEFAULT 0,"
            "content TEXT)");

    executeSchema(db,
        "CREATE TABLE IF NOT EXISTS track_meta ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "track_id INTEGER DEFAULT 0,"
            "meta_value_id INTEGER DEFAULT 0)");

    /* albums */
    executeSchema(db,
        "CREATE TABLE IF NOT EXISTS albums ("
            "id INTEGER PRIMARY KEY,"
            "name TEXT default '',"
            "thumbnail_id INTEGER default 0,"
            "sort_order INTEGER DEFAULT 0)");

    /* indexer paths */
    executeSchema(db,
        "CREATE TABLE IF NOT EXISTS paths ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "path TEXT default '')");

    /* browse directories */
    executeSchema(db,
        "CREATE TABLE IF NOT EXISTS directories ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "name TEXT NOT NULL)");

    /* thumbnails */
    executeSchema(db,
        "CREATE TABLE IF NOT EXISTS thumbnails ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "filename TEXT DEFAULT '',"
            "filesize INTEGER DEFAULT 0,"
            "checksum INTEGER DEFAULT 0"
            ")");

    /* playlists */
    executeSchema(db,
        "CREATE TABLE IF NOT EXISTS playlists ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "name TEXT default '',"
            "user_id INTEGER default 0"
            ")");

    /* playlist tracks */
    executeSchema(db,
        "CREATE TABLE IF NOT EXISTS playlist_tracks ("
            "playlist_id INTEGER DEFAULT 0,"
            "track_external_id TEXT NOT NULL DEFAULT '',"
            "source_id INTEGER DEFAULT 0,"
            "sort_order INTEGER DEFAULT 0"
            ")");

    /* replay gain */
    executeSchema(db,
        "CREATE TABLE IF NOT EXISTS replay_gain ("
        "id INTEGER PRIMARY KEY,"
        "track_id INTEGER DEFAULT 0,"
        "album_gain REAL default 1.0,"
        "album_peak REAL default 1.0,"
        "track_gain REAL default 1.0,"
        "track_peak REAL default 1.0)");

    /* version */
    executeSchema(db, "CREATE TABLE IF NOT EXISTS version (version INTEGER default 1)");

    int lastVersion = existing ? 1 : DATABASE_VERSION;
    {
        db::Statement statement("SELECT version FROM version", db);
        const auto step = statement.Step();
        requireSchema(step == db::Row || step == db::Done);
        if (step == db::Row) {
            lastVersion = statement.ColumnInt32(0);
        }
    }

    /* older catalogs can already contain some migration columns. */
    if (!hasColumn(db, "tracks", "source_id")) {
        executeSchema(db, "ALTER TABLE tracks ADD COLUMN source_id INTEGER DEFAULT 0");
        executeSchema(db, "UPDATE tracks SET source_id=0 WHERE source_id is null");
    }
    if (!hasColumn(db, "tracks", "visible")) {
        executeSchema(db, "ALTER TABLE tracks ADD COLUMN visible INTEGER DEFAULT 1");
        executeSchema(db, "UPDATE tracks SET visible=1 WHERE visible is null");
    }
    if (!hasColumn(db, "tracks", "external_id")) {
        executeSchema(db, "ALTER TABLE tracks ADD COLUMN external_id TEXT DEFAULT null");
    }

    /* create our simplified, de-normalized track table view */
    executeSchema(db, "DROP VIEW IF EXISTS tracks_view");

    executeSchema(db,
        "CREATE VIEW tracks_view AS "
        "SELECT DISTINCT "
            " t.id, t.track, t.disc, t.bpm, t.duration, t.filesize, t.title, t.filename, "
            " t.thumbnail_id, t.external_id, t.rating, t.last_played, t.play_count, t.date_added, "
            " t.date_updated, al.name AS album, alar.name AS album_artist, gn.name AS genre, "
            " ar.name AS artist, t.filetime, t.visual_genre_id, t.visual_artist_id, t.album_artist_id, "
            " t.album_id "
        "FROM "
            " tracks t, albums al, artists alar, artists ar, genres gn "
        "WHERE "
            " t.album_id=al.id AND t.album_artist_id=alar.id AND "
            " t.visual_genre_id=gn.id AND t.visual_artist_id=ar.id ");

    /* ensure the 'track_external_id' column exists, then create the indexes */
    if (!hasColumn(db, "playlist_tracks", "track_external_id")) {
        executeSchema(db, "ALTER TABLE playlist_tracks ADD COLUMN track_external_id TEXT NOT NULL DEFAULT ''");
    }
    if (!hasColumn(db, "playlist_tracks", "source_id")) {
        executeSchema(db, "ALTER TABLE playlist_tracks ADD COLUMN source_id INTEGER DEFAULT 0");
    }

    /* rebuild the extended metadata track view. */
    executeSchema(db, "DROP VIEW IF EXISTS extended_metadata");
    executeSchema(db,
        "CREATE VIEW extended_metadata AS "
        "SELECT DISTINCT "
            "tracks.id, tracks.external_id, tracks.source_id, meta_keys.id AS meta_key_id, track_meta.meta_value_id, "
            "meta_keys.name AS key, meta_values.content AS value "
        "FROM "
            "track_meta, meta_values, meta_keys, tracks "
        "WHERE "
            "tracks.id == track_meta.track_id AND "
            "meta_values.id = track_meta.meta_value_id AND "
            "meta_values.meta_key_id == meta_keys.id ");

    /* session play queue table */
    executeSchema(db,
        "CREATE TABLE IF NOT EXISTS last_session_play_queue ( "
            "id INTEGER PRIMARY KEY AUTOINCREMENT, "
            "track_id INTEGER)");

    /* upgrade playlist tracks table */
    if (lastVersion == 1) {
        upgradeV1toV2(db);
    }

    if (lastVersion >= 1 && lastVersion < 3) {
        upgradeV2ToV3(db);
    }

    if (lastVersion >= 1 && lastVersion < 4) {
        upgradeV3ToV4(db);
    }

    if (lastVersion >= 1 && lastVersion < 5) {
        upgradeV4ToV5(db);
    }

    if (lastVersion >= 1 && lastVersion < 6) {
        upgradeV5ToV6(db);
    }

    if (lastVersion >= 1 && lastVersion < 7) {
        upgradeV6ToV7(db);
    }

    if (lastVersion >= 1 && lastVersion < 8) {
        upgradeV7ToV8(db);
    }

    if (lastVersion >= 1 && lastVersion < 9) {
        upgradeV8ToV9(db);
    }

    if (lastVersion >= 1 && lastVersion < 10) {
        upgradeV9ToV10(db);
    }

    /* verify persisted queue and views before advancing the schema version. */
    executeSchema(db, "SELECT track_id FROM last_session_play_queue ORDER BY id LIMIT 0");
    executeSchema(db, "SELECT * FROM tracks_view LIMIT 0");
    executeSchema(db, "SELECT * FROM extended_metadata LIMIT 0");
    requireSchema(LocalLibrary::CreateIndexes(db));
    setVersion(db, DATABASE_VERSION);
}

void LocalLibrary::CreateDatabase(db::Connection& db) {
    const bool previousSync = scheduleSyncDueToDbUpgrade;
    executeSchema(db, "BEGIN IMMEDIATE");
    try {
        createSchema(db);
        executeSchema(db, "COMMIT");
    }
    catch (...) {
        db.Execute("ROLLBACK");
        scheduleSyncDueToDbUpgrade = previousSync;
        throw;
    }
}

void LocalLibrary::DropIndexes(db::Connection &db) {
    db.Execute("DROP INDEX IF EXISTS paths_index");

    db.Execute("DROP INDEX IF EXISTS genre_index");
    db.Execute("DROP INDEX IF EXISTS artist_index");
    db.Execute("DROP INDEX IF EXISTS album_index");
    db.Execute("DROP INDEX IF EXISTS thumbnail_index");

    db.Execute("DROP INDEX IF EXISTS trackgenre_index1");
    db.Execute("DROP INDEX IF EXISTS trackgenre_index2");
    db.Execute("DROP INDEX IF EXISTS trackartist_index1");
    db.Execute("DROP INDEX IF EXISTS trackartist_index2");
    db.Execute("DROP INDEX IF EXISTS trackmeta_index1");
    db.Execute("DROP INDEX IF EXISTS trackmeta_index2");
    db.Execute("DROP INDEX IF EXISTS metakey_index1");
    db.Execute("DROP INDEX IF EXISTS metavalues_index1");

    db.Execute("DROP INDEX IF EXISTS tracks_external_id_index");
    db.Execute("DROP INDEX IF EXISTS tracks_filename_id_index");
    db.Execute("DROP INDEX IF EXISTS tracks_dirty_index");
    db.Execute("DROP INDEX IF EXISTS tracks_external_id_filetime_index");
    db.Execute("DROP INDEX IF EXISTS tracks_by_source_index");

    db.Execute("DROP INDEX IF EXISTS playlist_tracks_index_1");
    db.Execute("DROP INDEX IF EXISTS playlist_tracks_index_2");
    db.Execute("DROP INDEX IF EXISTS playlist_tracks_index_3");
}

static bool executeAll(db::Connection& db, std::initializer_list<const char*> statements) {
    bool okay = true;
    for (const auto* sql : statements) {
        if (db.Execute(sql) != db::Okay) {
            okay = false;
        }
    }
    return okay;
}

bool LocalLibrary::CreateIndexes(db::Connection &db) {
    return executeAll(db, {
        "CREATE INDEX IF NOT EXISTS paths_index ON paths (path)",

        "CREATE INDEX IF NOT EXISTS genre_index ON genres (sort_order)",
        "CREATE INDEX IF NOT EXISTS artist_index ON artists (sort_order)",
        "CREATE INDEX IF NOT EXISTS album_index ON albums (sort_order)",
        "CREATE INDEX IF NOT EXISTS thumbnail_index ON thumbnails (filesize)",

        "CREATE INDEX IF NOT EXISTS trackgenre_index1 ON track_genres (track_id,genre_id)",
        "CREATE INDEX IF NOT EXISTS trackgenre_index2 ON track_genres (genre_id,track_id)",
        "CREATE INDEX IF NOT EXISTS trackartist_index1 ON track_artists (track_id,artist_id)",
        "CREATE INDEX IF NOT EXISTS trackartist_index2 ON track_artists (artist_id,track_id)",
        "CREATE INDEX IF NOT EXISTS trackmeta_index1 ON track_meta (track_id,meta_value_id)",
        "CREATE INDEX IF NOT EXISTS trackmeta_index2 ON track_meta (meta_value_id,track_id)",
        "CREATE INDEX IF NOT EXISTS metakey_index1 ON meta_keys (name)",
        "CREATE INDEX IF NOT EXISTS metakey_index2 ON meta_keys (id, name)",
        "CREATE INDEX IF NOT EXISTS metavalues_index1 ON meta_values (meta_key_id)",
        "CREATE INDEX IF NOT EXISTS metavalues_index2 ON meta_values (content)",
        "CREATE INDEX IF NOT EXISTS metavalues_index3 ON meta_values (id, meta_key_id, content)",
        "CREATE INDEX IF NOT EXISTS metavalues_index4 ON meta_values (id, content)",

        "CREATE INDEX IF NOT EXISTS tracks_external_id_index ON tracks (external_id)",
        "CREATE INDEX IF NOT EXISTS tracks_filename_index ON tracks (filename)",
        "CREATE INDEX IF NOT EXISTS tracks_dirty_index ON tracks (id, filename, filesize, filetime)",
        "CREATE INDEX IF NOT EXISTS tracks_external_id_filetime_index ON tracks (external_id, filetime)",
        "CREATE INDEX IF NOT EXISTS tracks_by_source_index ON tracks (id, external_id, filename, source_id)",

        "CREATE INDEX IF NOT EXISTS playlist_tracks_index_1 ON playlist_tracks (track_external_id,playlist_id,sort_order)",
        "CREATE INDEX IF NOT EXISTS playlist_tracks_index_2 ON playlist_tracks (track_external_id,sort_order)",
        "CREATE INDEX IF NOT EXISTS playlist_tracks_index_3 ON playlist_tracks (track_external_id)",
    });
}

bool LocalLibrary::InvalidateTrackMetadata(db::Connection& db) {
    return executeAll(db, {
        "UPDATE tracks SET filetime=0",
        "DELETE FROM track_meta;",
        "DELETE FROM meta_keys;",
        "DELETE FROM meta_values;",
    });
}
