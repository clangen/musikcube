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

#include <core/debug.h>
#include <core/library/Indexer.h>

#include <core/config.h>
#include <core/library/track/IndexerTrack.h>
#include <core/library/track/LibraryTrack.h>
#include <core/library/LocalLibraryConstants.h>
#include <core/db/Connection.h>
#include <core/db/Statement.h>
#include <core/plugin/PluginFactory.h>
#include <core/support/Common.h>
#include <core/support/Preferences.h>
#include <core/support/PreferenceKeys.h>
#include <core/sdk/IAnalyzer.h>
#include <core/sdk/IIndexerSource.h>
#include <core/audio/Stream.h>

#include <algorithm>

#include <boost/thread/xtime.hpp>
#include <boost/bind.hpp>

#include <atomic>

#define MULTI_THREADED_INDEXER 1
#define STRESS_TEST_DB 0

static const std::string TAG = "Indexer";
static const size_t TRANSACTION_INTERVAL = 300;
static FILE* logFile = nullptr;

#ifdef __arm__
static const int MAX_THREADS = 2;
#else
static const int MAX_THREADS = 4;
#endif

using namespace musik::core;
using namespace musik::core::sdk;
using namespace musik::core::audio;
using namespace musik::core::library;

using Thread = std::unique_ptr<boost::thread>;

using TagReaderDestroyer = PluginFactory::ReleaseDeleter<ITagReader>;
using DecoderDeleter = PluginFactory::ReleaseDeleter<IDecoderFactory>;
using SourceDeleter = PluginFactory::ReleaseDeleter<IIndexerSource>;

static void openLogFile() {
    if (!logFile) {
        std::string path = GetDataDirectory() + "/indexer_log.txt";
#ifdef WIN32
        logFile = _wfopen(u8to16(path).c_str(), L"w");
#else
        logFile = fopen(path.c_str(), "w");
#endif
    }
}

static void closeLogFile() {
    if (logFile) {
        fclose(logFile);
        logFile = nullptr;
    }
}

static std::string normalizePath(const std::string& path) {
    return boost::filesystem::path(path).make_preferred().string();
}

Indexer::Indexer(const std::string& libraryPath, const std::string& dbFilename)
: thread(nullptr)
, tracksScanned(0)
, state(StateStopped)
, prefs(Preferences::ForComponent(prefs::components::Settings))
, readSemaphore(prefs->GetInt(prefs::keys::MaxTagReadThreads, MAX_THREADS)) {
    if (prefs->GetBool(prefs::keys::IndexerLogEnabled, false) && !logFile) {
        openLogFile();
    }

    this->tagReaders = PluginFactory::Instance()
        .QueryInterface<ITagReader, TagReaderDestroyer>("GetTagReader");

    this->audioDecoders = PluginFactory::Instance()
        .QueryInterface<IDecoderFactory, DecoderDeleter>("GetDecoderFactory");

    this->sources = PluginFactory::Instance()
        .QueryInterface<IIndexerSource, SourceDeleter>("GetIndexerSource");

    this->dbFilename = dbFilename;
    this->libraryPath = libraryPath;

    db::Connection connection;
    connection.Open(this->dbFilename.c_str());
    db::Statement stmt("SELECT path FROM paths ORDER BY id", connection);
    while (stmt.Step() == db::Row) {
        this->paths.push_back(stmt.ColumnText(0));
    }
}

Indexer::~Indexer() {
    closeLogFile();
    this->Stop();
}

void Indexer::Stop() {
    if (this->thread) {
        {
            boost::mutex::scoped_lock lock(this->stateMutex);

            this->syncQueue.clear();
            this->state = StateStopping;

            if (this->currentSource) {
                this->currentSource->Interrupt();
            }
        }

        this->waitCondition.notify_all();
        this->thread->join();
        delete this->thread;
        this->thread = nullptr;
    }
}

void Indexer::Schedule(SyncType type) {
    this->Schedule(type, nullptr);
}

void Indexer::Schedule(SyncType type, IIndexerSource* source) {
    boost::mutex::scoped_lock lock(this->stateMutex);

    if (!this->thread) {
        this->state = StateIdle;
        this->thread = new boost::thread(boost::bind(&Indexer::ThreadLoop, this));
    }

    int sourceId = source ? source->SourceId() : 0;
    for (SyncContext& context : this->syncQueue) {
        if (context.type == type && context.sourceId == sourceId) {
            return;
        }
    }

    SyncContext context;
    context.type = type;
    context.sourceId = sourceId;
    syncQueue.push_back(context);

    this->waitCondition.notify_all();
}

void Indexer::AddPath(const std::string& path) {
    Indexer::AddRemoveContext context;
    context.add = true;
    context.path = NormalizeDir(path);

    {
        boost::mutex::scoped_lock lock(this->stateMutex);

        if (std::find(this->paths.begin(), this->paths.end(), path) == this->paths.end()) {
            this->paths.push_back(path);
        }

        this->addRemoveQueue.push_back(context);
    }
}

void Indexer::RemovePath(const std::string& path) {
    Indexer::AddRemoveContext context;
    context.add = false;
    context.path = NormalizeDir(path);

    {
        boost::mutex::scoped_lock lock(this->stateMutex);

        auto it = std::find(this->paths.begin(), this->paths.end(), path);
        if (it != this->paths.end()) {
            this->paths.erase(it);
        }

        this->addRemoveQueue.push_back(context);
    }
}

void Indexer::Synchronize(const SyncContext& context, boost::asio::io_service* io) {
    this->ProcessAddRemoveQueue();

    this->tracksScanned = 0;

    auto type = context.type;
    auto sourceId = context.sourceId;

    LocalLibrary::DropIndexes(this->dbConnection);

    if (type == SyncType::Rebuild) {
        LocalLibrary::InvalidateTrackMetadata(this->dbConnection);
        type = SyncType::All;
    }

    /* process ALL IIndexerSource plugins, if applicable */

    if (type == SyncType::All || (type == SyncType::Sources && sourceId == 0)) {
        for (auto it : this->sources) {
            if (this->Bail()) {
                break;
            }

            this->currentSource = it;
            if (this->SyncSource(it.get()) == ScanRollback) {
                this->trackTransaction->Cancel();
            }
            this->trackTransaction->CommitAndRestart();

            break;
        }

        this->currentSource.reset();
    }

    /* otherwise, we may have just been asked to index a single one... */

    else if (type == SyncType::Sources && sourceId != 0) {
        for (auto it : this->sources) {
            if (this->Bail()) {
                break;
            }

            if (it->SourceId() == sourceId) {
                this->currentSource = it;
                if (this->SyncSource(it.get()) == ScanRollback) {
                    this->trackTransaction->Cancel();
                }
                this->trackTransaction->CommitAndRestart();
            }
        }

        this->currentSource.reset();
    }

    /* process local files */
    if (type == SyncType::All || type == SyncType::Local) {
        if (logFile) {
            fprintf(logFile, "\n\nSYNCING LOCAL FILES:\n");
        }

        std::vector<std::string> paths;
        std::vector<int64_t> pathIds;

        /* resolve all the path and path ids (required for local files */
        db::Statement stmt("SELECT id, path FROM paths", this->dbConnection);

        while (stmt.Step() == db::Row) {
            try {
                int64_t id = stmt.ColumnInt64(0);
                std::string path = stmt.ColumnText(1);
                boost::filesystem::path dir(path);

                if (boost::filesystem::exists(dir)) {
                    paths.push_back(path);
                    pathIds.push_back(id);
                }
            }
            catch(...) {
            }
        }

        /* read metadata from the files  */

        for (std::size_t i = 0; i < paths.size(); ++i) {
            this->SyncDirectory(io, paths[i], paths[i], pathIds[i]);
        }

        /* close any pending transaction */

        this->trackTransaction->CommitAndRestart();

        /* re-index */
        LocalLibrary::CreateIndexes(this->dbConnection);
    }
}

void Indexer::FinalizeSync(const SyncContext& context) {
    /* remove undesired entries from db (files themselves will remain) */
    musik::debug::info(TAG, "cleanup 1/2");

    auto type = context.type;

    if (type != SyncType::Sources) {
        if (!this->Bail()) {
            this->SyncDelete();
        }
    }

    /* cleanup -- remove stale artists, albums, genres, etc */
    musik::debug::info(TAG, "cleanup 2/2");

    if (!this->Bail()) {
        this->SyncCleanup();
    }

    /* optimize and sort */
    musik::debug::info(TAG, "optimizing");

    if (!this->Bail()) {
        this->SyncOptimize();
    }

    /* notify observers */
    this->Progress(this->tracksScanned);

    /* run analyzers. */
    this->RunAnalyzers();

    IndexerTrack::ResetIdCache();
}

void Indexer::ReadMetadataFromFile(
    const boost::filesystem::path& file,
    const std::string& pathId)
{
    musik::core::IndexerTrack track(0);
    TagStore* store = nullptr;

    /* get cached filesize, parts, size, etc */
    if (track.NeedsToBeIndexed(file, this->dbConnection)) {
        bool saveToDb = false;

        /* read the tag from the plugin */
        store = new TagStore(track);
        typedef TagReaderList::iterator Iterator;
        Iterator it = this->tagReaders.begin();
        while (it != this->tagReaders.end()) {
            try {
                if ((*it)->CanRead(track.GetString("extension").c_str())) {
                    if (logFile) {
                        fprintf(logFile, "    - %s\n", file.string().c_str());
                    }

                    if ((*it)->Read(file.string().c_str(), store)) {
                        saveToDb = true;
                        break;
                    }
                }
            }
            catch (...) {
                /* sometimes people have files with crazy tags that cause the
                tag reader to throw fits. not a lot we can do. just move on. */
            }

            it++;
        }

        /* no tag? well... if a decoder can play it, add it to the database
        with the file as the name. */
        if (!saveToDb) {
            std::string fullPath = file.string();
            auto it = this->audioDecoders.begin();
            while (it != this->audioDecoders.end()) {
                if ((*it)->CanHandle(fullPath.c_str())) {
                    if (logFile) {
                        fprintf(logFile, "    - %s\n", file.string().c_str());
                    }

                    saveToDb = true;
                    track.SetValue("title", file.leaf().string().c_str());
                    break;
                }
                ++it;
            }
        }

        /* write it to the db, if read successfully */
        if (saveToDb) {
            track.SetValue("path_id", pathId.c_str());
            track.Save(this->dbConnection, this->libraryPath);

#if STRESS_TEST_DB != 0
            #define INC(track, key, x) \
                { \
                    std::string val = track.GetValue(key); \
                    val += (char) ('a' + x); \
                    track.ClearValue(key); \
                    track.SetValue(key, val.c_str()); \
                }

            for (int i = 0; i < 20; i++) {
                track.SetId(0);
                INC(track, "title", i);
                INC(track, "artist", i);
                INC(track, "album_artist", i);
                INC(track, "album", i);
                track.Save(this->dbConnection, this->libraryPath);
                this->filesSaved++;
            }
#endif
        }
    }

    if (store) {
        store->Release();
    }

    this->IncrementTracksScanned();

#ifdef MULTI_THREADED_INDEXER
    this->readSemaphore.post();
#endif
}

inline void Indexer::IncrementTracksScanned(size_t delta) {
    std::unique_lock<std::mutex> lock(IndexerTrack::sharedWriteMutex);

    this->tracksScanned.fetch_add(delta);

    if (this->tracksScanned > TRANSACTION_INTERVAL) {
        this->trackTransaction->CommitAndRestart();
        this->Progress(this->tracksScanned);
        this->tracksScanned = 0;
    }
}

void Indexer::SyncDirectory(
    boost::asio::io_service* io,
    const std::string &syncRoot,
    const std::string &currentPath,
    int64_t pathId)
{
    if (this->Bail()) {
        return;
    }

    std::string normalizedSyncRoot = NormalizeDir(syncRoot);
    std::string normalizedCurrentPath = NormalizeDir(currentPath);
    std::string leaf = boost::filesystem::path(currentPath).leaf().string(); /* trailing subdir in currentPath */

    /* start recursive filesystem scan */

    try { /* boost::filesystem may throw */

        /* for each file in the current path... */

        boost::filesystem::path path(currentPath);
        boost::filesystem::directory_iterator end;
        boost::filesystem::directory_iterator file(path);

        std::string pathIdStr = boost::lexical_cast<std::string>(pathId);
        std::vector<Thread> threads;

        for( ; file != end && !this->Bail(); file++) {
            if (is_directory(file->status())) {
                /* recursion here */
                musik::debug::info(TAG, "scanning " + file->path().string());
                this->SyncDirectory(io, syncRoot, file->path().string(), pathId);
            }
            else {
                if (io) {
                    this->readSemaphore.wait();

                    io->post(boost::bind(
                        &Indexer::ReadMetadataFromFile,
                        this,
                        file->path(),
                        pathIdStr));
                }
                else {
                    this->ReadMetadataFromFile(file->path(), pathIdStr);
                }
            }
        }
    }
    catch(...) {
    }

    #undef WAIT_FOR_ACTIVE
}

ScanResult Indexer::SyncSource(IIndexerSource* source) {
    if (logFile) {
        fprintf(logFile, "\n\nSYNCING SOURCE WITH ID: %d\n", source->SourceId());
    }

    if (source->SourceId() == 0) {
        return ScanRollback;
    }

    source->OnBeforeScan();

    /* first allow the source to update metadata for any tracks that it
    previously indexed. */
    {
        db::Statement tracks(
            "SELECT id, filename, external_id FROM tracks WHERE source_id=? ORDER BY id",
            this->dbConnection);

        tracks.BindInt32(0, source->SourceId());
        while (tracks.Step() == db::Row) {
            TrackPtr track(new IndexerTrack(tracks.ColumnInt64(0)));
            track->SetValue(constants::Track::FILENAME, tracks.ColumnText(1));

            if (logFile) {
                fprintf(logFile, "    - %s\n", track->GetString(constants::Track::FILENAME).c_str());
            }

            TagStore* store = new TagStore(track);
            source->ScanTrack(this, store, tracks.ColumnText(2));
            store->Release();
            this->IncrementTracksScanned();
        }
    }

    /* now tell it to do a wide-open scan. it can use this opportunity to
    remove old tracks, or add new ones. */
    auto result = source->Scan(this);

    source->OnAfterScan();

    return result;
}

void Indexer::ThreadLoop() {
    boost::filesystem::path thumbPath(this->libraryPath + "thumbs/");

    if (!boost::filesystem::exists(thumbPath)) {
        boost::filesystem::create_directories(thumbPath);
    }

    while (true) {
        /* wait for some work. */
        {
            boost::mutex::scoped_lock lock(this->stateMutex);
            while (!this->Bail() && this->syncQueue.size() == 0) {
                this->state = StateIdle;
                this->waitCondition.wait(lock);
            }
        }

        if (this->Bail()) {
            return;
        }

        SyncContext context = this->syncQueue.front();
        this->syncQueue.pop_front();

        this->state = StateIndexing;
        this->Started();

        this->dbConnection.Open(this->dbFilename.c_str(), 0);
        this->trackTransaction.reset(new db::ScopedTransaction(this->dbConnection));

#if MULTI_THREADED_INDEXER
        boost::asio::io_service io;
        boost::thread_group threadPool;
        boost::asio::io_service::work work(io);

        /* initialize the thread pool -- we'll use this to index tracks in parallel. */
        int threadCount = prefs->GetInt(prefs::keys::MaxTagReadThreads, MAX_THREADS);
        for (int i = 0; i < threadCount; i++) {
            threadPool.create_thread(boost::bind(&boost::asio::io_service::run, &io));
        }

        this->Synchronize(context, &io);

        /* done with sync, remove all the threads in the pool to free resources. they'll
        be re-created later if we index again. */
        io.stop();
        threadPool.join_all();
#else
        this->Synchronize(context, nullptr);
#endif

        this->FinalizeSync(context);

        this->trackTransaction.reset();

        this->dbConnection.Close();

        if (!this->Bail()) {
            this->Finished(this->tracksScanned);
        }

        musik::debug::info(TAG, "done!");
    }
}

void Indexer::SyncDelete() {
    /* remove all tracks that no longer reference a valid path entry */

    this->dbConnection.Execute("DELETE FROM tracks WHERE source_id == 0 AND path_id NOT IN (SELECT id FROM paths)");

    /* remove files that are no longer on the filesystem. */

    if (prefs->GetBool(prefs::keys::RemoveMissingFiles, true)) {
        db::Statement stmtRemove("DELETE FROM tracks WHERE id=?", this->dbConnection);

        db::Statement allTracks(
            "SELECT t.id, t.filename "
            "FROM tracks t "
            "WHERE source_id == 0", /* IIndexerSources delete their own tracks */
            this->dbConnection);

        while (allTracks.Step() == db::Row && !this->Bail()) {
            bool remove = false;
            std::string fn = allTracks.ColumnText(1);

            try {
                boost::filesystem::path file(fn);
                if (!boost::filesystem::exists(file)) {
                    remove = true;
                }
            }
            catch (...) {
            }

            if (remove) {
                stmtRemove.BindInt32(0, allTracks.ColumnInt32(0));
                stmtRemove.Step();
                stmtRemove.Reset();
            }
        }
    }
}

void Indexer::SyncCleanup() {
    /* remove old artists */
    this->dbConnection.Execute("DELETE FROM track_artists WHERE track_id NOT IN (SELECT id FROM tracks)");
    this->dbConnection.Execute("DELETE FROM artists WHERE id NOT IN (SELECT DISTINCT(visual_artist_id) FROM tracks) AND id NOT IN (SELECT DISTINCT(album_artist_id) FROM tracks) AND id NOT IN (SELECT DISTINCT(artist_id) FROM track_artists)");

    /* remove old genres */
    this->dbConnection.Execute("DELETE FROM track_genres WHERE track_id NOT IN (SELECT id FROM tracks)");
    this->dbConnection.Execute("DELETE FROM genres WHERE id NOT IN (SELECT DISTINCT(visual_genre_id) FROM tracks) AND id NOT IN (SELECT DISTINCT(genre_id) FROM track_genres)");

    /* remove old albums */
    this->dbConnection.Execute("DELETE FROM albums WHERE id NOT IN (SELECT DISTINCT(album_id) FROM tracks)");

    /* orphaned metadata */
    this->dbConnection.Execute("DELETE FROM track_meta WHERE track_id NOT IN (SELECT id FROM tracks)");
    this->dbConnection.Execute("DELETE FROM meta_values WHERE id NOT IN (SELECT DISTINCT(meta_value_id) FROM track_meta)");
    this->dbConnection.Execute("DELETE FROM meta_keys WHERE id NOT IN (SELECT DISTINCT(meta_key_id) FROM meta_values)");

    /* orphaned replay gain and directories */
    this->dbConnection.Execute("DELETE FROM replay_gain WHERE track_id NOT IN (SELECT id FROM tracks)");
    this->dbConnection.Execute("DELETE FROM directories WHERE id NOT IN (SELECT DISTINCT directory_id FROM tracks)");

    /* NOTE: we used to remove orphaned local library tracks here, but we don't anymore because
    the indexer generates stable external ids by hashing various file and metadata fields */

    /* orphaned playlist tracks from source plugins that do not have stable
    ids need to be cleaned up. */
    for (auto source : this->sources) {
        if (!source->HasStableIds()) {
            std::string query =
                "DELETE FROM playlist_tracks "
                "WHERE source_id=? AND track_external_id NOT IN ( "
                "  SELECT DISTINCT external_id "
                "  FROM tracks "
                "  WHERE source_id == ?)";

            db::Statement stmt(query.c_str(), this->dbConnection);
            stmt.BindInt32(0, source->SourceId());
            stmt.BindInt32(1, source->SourceId());
            stmt.Step();
        }
    }

    this->SyncPlaylistTracksOrder();

    /* optimize and shrink */
    this->dbConnection.Execute("VACUUM");
}

void Indexer::SyncPlaylistTracksOrder() {
    /* make sure playlist sort orders are always sequential without holes. we
    do this anyway, as playlists are updated, but there's no way to guarantee
    it stays this way -- plugins, external processes, etc can cause problems */

    db::Statement playlists(
        "SELECT DISTINCT id FROM playlists",
        this->dbConnection);

    db::Statement tracks(
        "SELECT track_external_id, sort_order "
        "FROM playlist_tracks WHERE playlist_id=? "
        "ORDER BY sort_order",
        this->dbConnection);

    db::Statement update(
        "UPDATE playlist_tracks "
        "SET sort_order=? "
        "WHERE track_external_id=? AND sort_order=?",
        this->dbConnection);

    struct Record { std::string id; int order; };

    while (playlists.Step() == db::Row) {
        tracks.ResetAndUnbind();
        tracks.BindInt64(0, playlists.ColumnInt64(0));

        /* gotta cache these in memory because we can't update the
        table at the same time we're iterating */
        std::vector<Record> records;
        while (tracks.Step() == db::Row) {
            records.push_back({ tracks.ColumnText(0), tracks.ColumnInt32(1) });
        }

        int order = 0;
        for (auto& r : records) {
            update.ResetAndUnbind();
            update.BindInt32(0, order++);
            update.BindText(1, r.id);
            update.BindInt32(2, r.order);
            update.Step();
        }
    }
}

void Indexer::GetPaths(std::vector<std::string>& paths) {
    boost::mutex::scoped_lock lock(this->stateMutex);
    std::copy(this->paths.begin(), this->paths.end(), std::back_inserter(paths));
}

static int optimize(
    musik::core::db::Connection &connection,
    std::string singular,
    std::string plural)
{
    std::string outer = boost::str(
        boost::format("SELECT id, lower(trim(name)) AS %1% FROM %2% ORDER BY %3%")
        % singular % plural % singular);

    db::Statement outerStmt(outer.c_str(), connection);

    std::string inner = boost::str(boost::format("UPDATE %1% SET sort_order=? WHERE id=?") % plural);
    db::Statement innerStmt(inner.c_str(), connection);

    int count = 0;
    while (outerStmt.Step() == db::Row) {
        innerStmt.BindInt32(0, count);
        innerStmt.BindInt64(1, outerStmt.ColumnInt64(0));
        innerStmt.Step();
        innerStmt.Reset();
        ++count;
    }

    boost::thread::yield();

    return count;
}

void Indexer::SyncOptimize() {
    db::ScopedTransaction transaction(this->dbConnection);
    optimize(this->dbConnection, "genre", "genres");
    optimize(this->dbConnection, "artist", "artists");
    optimize(this->dbConnection, "album", "albums");
    optimize(this->dbConnection, "content", "meta_values");
}

void Indexer::ProcessAddRemoveQueue() {
    boost::mutex::scoped_lock lock(this->stateMutex);

    while (!this->addRemoveQueue.empty()) {
        AddRemoveContext context = this->addRemoveQueue.front();

        if (context.add) { /* insert new paths */
            db::Statement stmt("SELECT id FROM paths WHERE path=?", this->dbConnection);
            stmt.BindText(0, context.path);

            if (stmt.Step() != db::Row) {
                db::Statement insertPath("INSERT INTO paths (path) VALUES (?)", this->dbConnection);
                insertPath.BindText(0, context.path);
                insertPath.Step();
            }
        }
        else { /* remove old ones */
            db::Statement stmt("DELETE FROM paths WHERE path=?", this->dbConnection);
            stmt.BindText(0, context.path);
            stmt.Step();
        }

        this->addRemoveQueue.pop_front();
    }
}

void Indexer::RunAnalyzers() {
    typedef sdk::IAnalyzer PluginType;
    typedef PluginFactory::ReleaseDeleter<PluginType> Deleter;
    typedef std::shared_ptr<PluginType> PluginPtr;
    typedef std::vector<PluginPtr> PluginVector;

    /* short circuit if there aren't any analyzers */

    PluginVector analyzers = PluginFactory::Instance()
        .QueryInterface<PluginType, Deleter>("GetAudioAnalyzer");

    if (analyzers.empty()) {
        return;
    }

    /* for each track... */

    int64_t trackId = 0;

    db::Statement getNextTrack(
        "SELECT id FROM tracks WHERE id>? ORDER BY id LIMIT 1",
        this->dbConnection);

    getNextTrack.BindInt64(0, trackId);

    while(getNextTrack.Step() == db::Row ) {
        trackId = getNextTrack.ColumnInt64(0);

        getNextTrack.ResetAndUnbind();

        IndexerTrack track(trackId);

        if (LibraryTrack::Load(&track, this->dbConnection)) {
            PluginVector runningAnalyzers;

            TagStore* store = new TagStore(track);
            PluginVector::iterator plugin = analyzers.begin();
            for ( ; plugin != analyzers.end(); ++plugin) {
                if ((*plugin)->Start(store)) {
                    runningAnalyzers.push_back(*plugin);
                }
            }

            if (!runningAnalyzers.empty()) {
                audio::IStreamPtr stream = audio::Stream::Create(audio::IStream::NoDSP);

                if (stream) {
                    if (stream->OpenStream(track.Uri())) {

                        /* decode the stream quickly, passing to all analyzers */

                        audio::Buffer* buffer;

                        while ((buffer = stream->GetNextProcessedOutputBuffer()) && !runningAnalyzers.empty()) {
                            PluginVector::iterator plugin = runningAnalyzers.begin();
                            while(plugin != runningAnalyzers.end()) {
                                if ((*plugin)->Analyze(store, buffer)) {
                                    ++plugin;
                                }
                                else {
                                    plugin = runningAnalyzers.erase(plugin);
                                }
                            }
                        }

                        /* done with track decoding and analysis, let the plugins know */

                        int successPlugins = 0;
                        PluginVector::iterator plugin = analyzers.begin();

                        for ( ; plugin != analyzers.end(); ++plugin) {
                            if ((*plugin)->End(store)) {
                                successPlugins++;
                            }
                        }

                        /* the analyzers can write metadata back to the DB, so if any of them
                        completed successfully, then save the track. */

                        if (successPlugins>0) {
                            track.Save(this->dbConnection, this->libraryPath);
                        }
                    }
                }
            }

            if (store) {
                store->Release();
            }
        }

        if (this->Bail()) {
            return;
        }

        getNextTrack.BindInt64(0, trackId);
    }
}

ITagStore* Indexer::CreateWriter() {
    std::shared_ptr<Track> track(new IndexerTrack(0));
    return new TagStore(track);
}

bool Indexer::Save(IIndexerSource* source, ITagStore* store, const char* externalId) {
    if (source->SourceId() == 0) {
        return false;
    }

    if (!externalId || strlen(externalId) == 0) {
        return false;
    }

    /* two levels of unpacking with dynamic_casts. don't tell anyone,
    it'll be our little secret. */
    TagStore* ts = dynamic_cast<TagStore*>(store);
    if (ts) {
        IndexerTrack* it = ts->As<IndexerTrack*>();
        if (it) {
            it->SetValue(constants::Track::EXTERNAL_ID, externalId);
            it->SetValue(constants::Track::SOURCE_ID, std::to_string(source->SourceId()).c_str());
            return it->Save(this->dbConnection, this->libraryPath);
        }
    }
    return false;
}

bool Indexer::RemoveByUri(IIndexerSource* source, const char* uri) {
    if (source->SourceId() == 0) {
        return false;
    }

    if (!uri || strlen(uri) == 0) {
        return false;
    }

    db::Statement stmt(
        "DELETE FROM tracks WHERE source_id=? AND filename=?",
        this->dbConnection);

    stmt.BindInt32(0, source->SourceId());
    stmt.BindText(1, uri);

    return (stmt.Step() == db::Okay);
}

bool Indexer::RemoveByExternalId(IIndexerSource* source, const char* id) {
    if (source->SourceId() == 0) {
        return false;
    }

    if (!id || strlen(id) == 0) {
        return false;
    }

    db::Statement stmt(
        "DELETE FROM tracks WHERE source_id=? AND external_id=?",
        this->dbConnection);

    stmt.BindInt32(0, source->SourceId());
    stmt.BindText(1, id);

    return (stmt.Step() == db::Okay);
}

int Indexer::RemoveAll(IIndexerSource* source) {
    if (source->SourceId() == 0) {
        return 0;
    }

    db::Statement stmt(
        "DELETE FROM tracks WHERE source_id=?",
        this->dbConnection);

    stmt.BindInt32(0, source->SourceId());

    if (stmt.Step() == db::Okay) {
        return dbConnection.LastModifiedRowCount();
    }

    return 0;
}

void Indexer::ScheduleRescan(IIndexerSource* source) {
    if (source->SourceId() != 0) {
        this->Schedule(SyncType::Sources, source);
    }
}

bool Indexer::Bail() {
    return
        this->state == StateStopping ||
        this->state == StateStopped;
}
