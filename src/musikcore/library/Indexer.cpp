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
#include "musikcore/sdk/Filesystem.h"

#include <musikcore/debug.h>
#include <musikcore/library/Indexer.h>

#include <musikcore/config.h>
#include <musikcore/debug.h>
#include <musikcore/library/track/IndexerTrack.h>
#include <musikcore/library/track/LibraryTrack.h>
#include <musikcore/library/query/TrackMetadataQuery.h>
#include <musikcore/library/query/MigrationScanQuery.h>
#include <musikcore/library/LocalLibraryConstants.h>
#include <musikcore/library/LibraryFactory.h>
#include <musikcore/db/Connection.h>
#include <musikcore/db/Statement.h>
#include <musikcore/plugin/PluginFactory.h>
#include <musikcore/support/Common.h>
#include <musikcore/support/Preferences.h>
#include <musikcore/support/PreferenceKeys.h>
#include <musikcore/plugin/analyzer.h>
#include <musikcore/plugin/Plugins.h>
#include <musikcore/plugin/MetadataWriter.h>
#include <common/Contract.h>
#include <musikcore/sdk/IIndexerSource.h>
#include <musikcore/audio/Stream.h>
#include <musikcore/support/ThreadGroup.h>

#include <filesystem>
#include <algorithm>
#include <atomic>
#include <functional>

#define STRESS_TEST_DB 0

constexpr const char* TAG = "Indexer";
constexpr size_t TRANSACTION_INTERVAL = 300;
static FILE* logFile = nullptr;

#ifdef __arm__
constexpr int DEFAULT_MAX_THREADS = 2;
#else
constexpr int DEFAULT_MAX_THREADS = 4;
#endif

using musik::core::sdk::fs::pathToUtf8;

namespace std {
    namespace fs = std::filesystem;
}

using namespace musik::core;
using namespace musik::core::sdk;
using namespace musik::core::audio;
using namespace musik::core::library;
using namespace musik::core::db;
using namespace musik::core::library::query;

using Thread = std::unique_ptr<std::thread>;

using TagReaderDestroyer = PluginFactory::ReleaseDeleter<ITagReader>;
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

static void closeLogFile() noexcept {
    if (logFile) {
        fclose(logFile);
        logFile = nullptr;
    }
}

Indexer::Indexer(const std::string& libraryPath, const std::string& dbFilename)
: state(StateStopped)
, thread(nullptr)
, incrementalUrisScanned(0)
, totalUrisScanned(0)
, prefs(Preferences::ForComponent(prefs::components::Settings)) {
    if (prefs->GetBool(prefs::keys::IndexerLogEnabled, false) && !logFile) {
        openLogFile();
    }

    this->tagReaders = PluginFactory::Instance()
        .QueryInterface<ITagReader, TagReaderDestroyer>("GetTagReader");

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
    this->Shutdown();
}

void Indexer::Shutdown() {
    if (this->thread) {
        {
            std::lock_guard lock(this->stateMutex);
            this->syncQueue.clear();
            this->state = StateStopping;
        }
        {
            /* wait for an in-flight cancellation to release its stream before
            shutdown can unload the stream provider. */
            std::lock_guard lifecycle(this->sourceLifecycleMutex);
            std::shared_ptr<IIndexerSource> source;
            audio::Stream::Shared analysis;
            uint64_t generation;
            {
                std::lock_guard lock(this->stateMutex);
                source = this->currentSource;
                analysis = this->analysisStream;
                generation = this->scanGeneration;
            }
            this->InterruptSource(source, generation);
            if (analysis) {
                analysis->Interrupt();
            }
        }

        this->waitCondition.notify_all();
        this->thread->join();
        this->thread.reset();
    }
}

void Indexer::Schedule(SyncType type) {
    this->Schedule(type, nullptr);
}

void Indexer::Cancel() {
    uint64_t generation;
    {
        std::lock_guard lock(this->stateMutex);
        this->syncQueue.clear();
        generation = this->scanGeneration;
        if (this->state != StateIndexing || this->cancelRequested.exchange(true)) {
            return;
        }
    }

    std::lock_guard lifecycle(this->sourceLifecycleMutex);
    std::shared_ptr<IIndexerSource> source;
    audio::Stream::Shared analysis;
    {
        std::lock_guard lock(this->stateMutex);
        if (this->state == StateIndexing && generation == this->scanGeneration && this->Bail()) {
            source = this->currentSource;
            analysis = this->analysisStream;
        }
    }
    this->InterruptSource(source, generation);
    if (analysis) {
        analysis->Interrupt();
    }
}

void Indexer::InterruptSource(const std::shared_ptr<IIndexerSource>& source, uint64_t generation) {
    if (!source) {
        return;
    }
    /* serialize interruption with provider reset, but allow callbacks to reenter
    Cancel. a delayed interrupt must never spill into a later scan generation. */
    std::lock_guard lifecycle(this->sourceLifecycleMutex);
    bool interrupt;
    {
        std::lock_guard state(this->stateMutex);
        interrupt = generation == this->scanGeneration && this->currentSource == source && this->Bail();
    }
    if (interrupt) {
        source->Interrupt();
    }
}

void Indexer::Schedule(SyncType type, IIndexerSource* source) {
    std::unique_lock<decltype(this->stateMutex)> lock(this->stateMutex);

    if (!this->thread) {
        this->state = StateIdle;
        this->thread = std::make_unique<std::thread>(std::bind(&Indexer::ThreadLoop, this));
    }

    const int sourceId = source ? source->SourceId() : 0;
    for (const SyncContext& context : this->syncQueue) {
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
        std::unique_lock<decltype(this->stateMutex)> lock(this->stateMutex);

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
        std::unique_lock<decltype(this->stateMutex)> lock(this->stateMutex);

        auto it = std::find(this->paths.begin(), this->paths.end(), path);
        if (it != this->paths.end()) {
            this->paths.erase(it);
        }

        this->addRemoveQueue.push_back(context);
    }
}

void Indexer::Synchronize(const SyncContext& context, asio::io_context* io) {
    if (this->Bail()) {
        return;
    }
    LocalLibrary::CreateIndexes(this->dbConnection);

    IndexerTrack::OnIndexerStarted(this->dbConnection);

    this->ProcessAddRemoveQueue();

    this->incrementalUrisScanned = 0;
    this->totalUrisScanned = 0;
    this->lastProgressCommit = std::chrono::steady_clock::now();

    /* always remove tracks that no longer have a corresponding source */
    for (const auto id : this->GetOrphanedSourceIds()) {
        this->RemoveAllForSourceId(id);
    }

    auto type = context.type;
    const auto sourceId = context.sourceId;
    if (type == SyncType::Rebuild) {
        LocalLibrary::InvalidateTrackMetadata(this->dbConnection);

        /* for sources with stable ids: just nuke all of the records and allow
        a rebuild from scratch; things like playlists will remain intact.
        this ensures tracks that should be removed, are */
        for (auto source: sources) {
            if (source->HasStableIds()) {
                this->RemoveAll(source.get());
            }
        }

        type = SyncType::All;
    }

    std::vector<std::string> paths;
    std::vector<int64_t> pathIds;

    /* resolve all the path and path ids (required for local files */
    db::Statement stmt("SELECT id, path FROM paths", this->dbConnection);

    while (stmt.Step() == db::Row) {
        try {
            const int64_t id = stmt.ColumnInt64(0);
            std::string path = stmt.ColumnText(1);
            std::fs::path dir(musik::core::sdk::fs::pathFromUtf8(path));

            if (std::fs::exists(dir)) {
                paths.push_back(path);
                pathIds.push_back(id);
            }
        }
        catch(...) {
        }
    }

    /* refresh sources */
    for (auto it : this->sources) {
        if (this->Bail()) {
            break;
        }

        if (sourceId != 0 && sourceId != it->SourceId()) {
            continue; /* asked to scan a specific source, and this isn't it. */
        }

        /* reset provider interruption before publishing its active lifetime.
        cancellation during this callback prevents Scan from starting. */
        bool runSource;
        {
            std::lock_guard lifecycle(this->sourceLifecycleMutex);
            it->OnBeforeScan();
            std::lock_guard lock(this->stateMutex);
            runSource = !this->Bail();
            if (runSource) {
                this->currentSource = it;
            }
        }
        if (!runSource) {
            it->OnAfterScan();
            break;
        }
        if (this->SyncSource(it.get(), paths) == ScanRollback) {
            this->trackTransaction->Cancel();
        }
        {
            std::lock_guard lock(this->stateMutex);
            this->currentSource.reset();
        }
        this->trackTransaction->CommitAndRestart();

        if (sourceId != 0) {
            break; /* done with the one we were asked to scan */
        }
    }

    /* process local files */
    if (type != SyncType::Sources) {
        if (logFile) {
            fprintf(logFile, "\n\nSYNCING LOCAL FILES:\n");
        }

        /* read metadata from the files  */
        for (std::size_t i = 0; i < paths.size(); ++i) {
            musik::debug::info(TAG, "scanning " + paths[i]);
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

    const auto type = context.type;

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

    /* run analyzers. */
    if (!this->Bail()) {
        this->RunAnalyzers();
    }

    IndexerTrack::OnIndexerFinished(this->dbConnection);
}

void Indexer::ReadMetadataFromFile(
    asio::io_context* io,
    const std::fs::path& file,
    const std::string& pathId)
{
    /* we do this here because work may have already been queued before the abort
    flag was raised */
    if (io && this->Bail()) {
        if (!io->stopped()) {
            musik::debug::info(TAG, "run aborted");
            io->stop();
        }
        return;
    }

    #define APPEND_LOG(x) if (logFile) { fprintf(logFile, "    - [%s] %s\n", x, pathToUtf8(file).c_str()); }

    musik::core::IndexerTrack track(0);

    const bool needsToBeIndexed = track.NeedsToBeIndexed(file, this->dbConnection, this->refreshMetadata);

    /* get cached filesize, parts, size, etc */
    if (needsToBeIndexed) {
        APPEND_LOG("needs to be indexed")

        bool saveToDb = false;

        /* read the tag from the plugin */
        TagStore store(track);
        typedef TagReaderList::iterator Iterator;
        Iterator it = this->tagReaders.begin();
        while (it != this->tagReaders.end()) {
            try {
                if ((*it)->CanRead(track.GetString("extension").c_str())) {
                    APPEND_LOG("can read")
                    if ((*it)->Read(pathToUtf8(file).c_str(), &store)) {
                        APPEND_LOG("did read")
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
            }
#endif
        }
        else {
            APPEND_LOG("read failed")
        }
    }
    else {
        APPEND_LOG("does not need to be indexed")
    }

    #undef APPEND_LOG

    this->IncrementTracksScanned();
}

inline void Indexer::IncrementTracksScanned(int delta) {
    std::unique_lock<std::mutex> lock(IndexerTrack::sharedWriteMutex);

    this->incrementalUrisScanned.fetch_add(delta);
    this->totalUrisScanned.fetch_add(delta);

    const int interval = prefs->GetInt(
        prefs::keys::IndexerTransactionInterval, TRANSACTION_INTERVAL);

    constexpr auto progressInterval = std::chrono::seconds(2);
    const auto now = std::chrono::steady_clock::now();
    if (this->incrementalUrisScanned >= std::max(1, interval) ||
        now - this->lastProgressCommit >= progressInterval)
    {
        this->lastProgressCommit = now;
        this->trackTransaction->CommitAndRestart();
        this->Progress.Publish(this->totalUrisScanned);
        this->incrementalUrisScanned = 0;
    }
}

void Indexer::SyncDirectory(
    asio::io_context* io,
    const std::string &syncRoot,
    const std::string &currentPath,
    int64_t pathId)
{
    std::string normalizedSyncRoot = NormalizeDir(syncRoot);
    std::string normalizedCurrentPath = NormalizeDir(currentPath);

    /* start recursive filesystem scan */

    try {
        /* for each file in the current path... */
        std::fs::path path(musik::core::sdk::fs::pathFromUtf8(currentPath));
        std::fs::directory_iterator end;
        std::fs::directory_iterator file(path);

        std::string pathIdStr = std::to_string(pathId);
        std::vector<Thread> threads;

        for( ; file != end && !this->Bail(); file++) {
            if (this->Bail()) {
                break;
            }
            if (is_directory(file->status())) {
                /* recursion here */
                this->SyncDirectory(io, syncRoot, pathToUtf8(file->path()), pathId);
            }
            else {
                try {
                    std::string extension = pathToUtf8(file->path().extension());
                    for (auto it : this->tagReaders) {
                        if (it->CanRead(extension.c_str())) {
                            if (io) {
                                asio::post(*io, std::bind(
                                    &Indexer::ReadMetadataFromFile,
                                    this,
                                    io,
                                    file->path(),
                                    pathIdStr));
                            }
                            else {
                                this->ReadMetadataFromFile(nullptr, file->path(), pathIdStr);
                            }
                            break;
                        }
                    }
                }
                catch (...) {
                    /* std::filesystem may throw trying to stat the file */
                }
            }
        }
    }
    catch(...) {
        /* std::filesystem may throw trying to open the directory */
    }
}

ScanResult Indexer::SyncSource(
    IIndexerSource* source,
    const std::vector<std::string>& paths)
{
    debug::info(TAG, u8fmt("indexer source %d running...", source->SourceId()));

    if (source->SourceId() == 0) {
        return ScanRollback;
    }

    /* only commit if explicitly succeeded */
    ScanResult result = ScanRollback;

    try {
        /* alloc/init fun interop; we pass all paths to the indexer source */
        const char** pathsList = new const char*[paths.size()];
        for (size_t i = 0; i < paths.size(); i++) {
            auto& p = paths[i];
            auto sz = p.size();
            auto dst = new char[sz + 1];
            strncpy(dst, p.c_str(), sz);
            dst[sz] = '\0';
            pathsList[i] = dst;
        }

        /* now tell it to do a wide-open scan. it can use this opportunity to
        remove old tracks, or add new ones. */
        try {
            result = source->Scan(this, pathsList, (unsigned int) paths.size());
        }
        catch (...) {
            debug::error("Indexer", "failed to index " + std::to_string(source->SourceId()));
        }

        /* free fun interop -- it's done with the paths now. */
        for (size_t i = 0; i < paths.size(); i++) {
            delete[] pathsList[i];
        }
        delete[] pathsList;

        /* finally, allow the source to update metadata for any tracks that it
        previously indexed, if it needs to. */
        {
            if (!this->Bail() && source->NeedsTrackScan()) {
                db::Statement tracks(
                    "SELECT id, filename, external_id FROM tracks WHERE source_id=? ORDER BY id",
                    this->dbConnection);

                tracks.BindInt32(0, source->SourceId());
                while (!this->Bail() && tracks.Step() == db::Row) {
                    TrackPtr track = std::make_shared<IndexerTrack>(tracks.ColumnInt64(0));
                    track->SetValue(constants::Track::FILENAME, tracks.ColumnText(1));

                    if (logFile) {
                        fprintf(logFile, "    - %s\n", track->GetString(constants::Track::FILENAME).c_str());
                    }

                    TagStore* store = new TagStore(track);
                    source->ScanTrack(this, store, tracks.ColumnText(2));
                    store->Release();
                }
            }
        }

        debug::info(TAG, u8fmt("indexer source %d finished", source->SourceId()));
    }
    catch (...) {
        debug::error(TAG, u8fmt("indexer source %d crashed", source->SourceId()));
    }

    source->OnAfterScan();

    return result;
}

void Indexer::ThreadLoop() {
    std::fs::path thumbPath(musik::core::sdk::fs::pathFromUtf8(this->libraryPath + "thumbs/"));

    if (!std::fs::exists(thumbPath)) {
        std::fs::create_directories(thumbPath);
    }

    while (true) {
        SyncContext context;
        /* selecting work and cancellation share one boundary: a later Schedule
        belongs to the next scan and must not inherit the cancelled flag. */
        {
            std::unique_lock<decltype(this->stateMutex)> lock(this->stateMutex);
            while (this->state != StateStopping && this->syncQueue.empty()) {
                this->state = StateIdle;
                this->waitCondition.wait(lock);
            }
            if (this->state == StateStopping) {
                this->state = StateStopped;
                return;
            }
            context = this->syncQueue.front();
            this->syncQueue.pop_front();
            ++this->scanGeneration;
            this->cancelRequested = false;
            this->state = StateIndexing;
        }
        this->incrementalUrisScanned = 0;
        this->totalUrisScanned = 0;
        this->Started.Publish();

        if (this->dbConnection.Open(this->dbFilename.c_str(), 0) != db::Okay) {
            musik::debug::warning(TAG, "could not open catalog for indexing");
            this->dbConnection.Close();
            this->CompleteScan();
            continue;
        }

        const auto previousErrors = this->dbConnection.ErrorCount();
        this->trackTransaction = std::make_shared<db::ScopedTransaction>(this->dbConnection);

        /* Earlier batches may have committed file times before a later
        metadata write failed. Pending migrations revisit those tracks too. */
        this->refreshMetadata = false;
        if (context.type != SyncType::Sources && context.sourceId == 0) {
            library::query::MigrationScanQuery pending;
            this->refreshMetadata = pending.Run(this->dbConnection) && pending.IsPending();
        }

        const int threadCount = prefs->GetInt(
            prefs::keys::IndexerThreadCount, DEFAULT_MAX_THREADS);

        if (threadCount > 1) {
            asio::io_context io;
            auto work = asio::make_work_guard(io); /* need to retain the reference */

            ThreadGroup threadGroup;

            /* initialize the thread pool -- we'll use this to index tracks in parallel. */
            for (int i = 0; i < threadCount; i++) {
                threadGroup.create_thread([&io]() {
                    io.run();
                });
            }

            this->Synchronize(context, &io);

            /* done with sync, remove all the threads in the pool to free resources. they'll
            be re-created later if we index again. */
            asio::post(io, [&io]() {
                if (!io.stopped()) {
                    musik::debug::info(TAG, "scan completed successfully");
                    io.stop();
                }
            });

            threadGroup.join_all();
        }
        else {
            this->Synchronize(context, nullptr);
        }

        this->FinalizeSync(context);

        /* Retire the requirement in the final transaction only when all scan
        writes and earlier commits succeeded, including writes from workers. */
        if (!this->Bail() && this->refreshMetadata) {
            if (this->dbConnection.ErrorCount() == previousErrors) {
                library::query::MigrationScanQuery completed(library::query::MigrationScanQuery::Operation::Complete);
                if (!completed.Run(this->dbConnection)) {
                    musik::debug::warning(TAG, "could not clear pending migration scan");
                }
            }
            else {
                musik::debug::warning(TAG, "catalog writes failed during indexing");
            }
        }
        this->trackTransaction.reset();

        this->dbConnection.Close();

        this->CompleteScan();

        musik::debug::info(TAG, "done!");
    }
}

void Indexer::CompleteScan() {
    bool cancelled;
    {
        std::lock_guard lock(this->stateMutex);
        if (this->state == StateStopping) {
            return;
        }
        cancelled = this->cancelRequested.exchange(false);
        this->state = StateIdle;
    }
    /* publish after retiring the scan, so callbacks may cancel or schedule
    again without changing the terminal outcome selected for this scan. */
    this->Progress.Publish(this->totalUrisScanned);
    if (cancelled) {
        this->Cancelled.Publish(this->totalUrisScanned);
    }
    else {
        this->Finished.Publish(this->totalUrisScanned);
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
                std::fs::path file(musik::core::sdk::fs::pathFromUtf8(fn));
                if (!std::fs::exists(file)) {
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
    std::unique_lock<decltype(this->stateMutex)> lock(this->stateMutex);
    std::copy(this->paths.begin(), this->paths.end(), std::back_inserter(paths));
}

std::set<int> Indexer::GetOrphanedSourceIds() {
    /* build a list of source ids: `(x, y, z)` */
    std::string group = "(0"; /* 0 is the built-in source, it's always valid */
    for (size_t i = 0; i < this->sources.size(); i++) {
        group += "," + std::to_string(this->sources.at(i)->SourceId());
    }
    group += ")";

    std::string query =
        "SELECT DISTINCT source_id "
        "FROM tracks "
        "WHERE source_id NOT IN " + group;

    std::set<int> result;
    db::Statement stmt(query.c_str(), this->dbConnection);
    while (stmt.Step() == db::Row) {
        result.insert(stmt.ColumnInt32(0));
    }
    return result;
}

static int optimize(
    musik::core::db::Connection &connection,
    std::string singular,
    std::string plural)
{

    std::string outer = u8fmt(
        "SELECT id, lower(trim(name)) AS %s FROM %s ORDER BY %s",
        singular.c_str(), plural.c_str(), singular.c_str());

    db::Statement outerStmt(outer.c_str(), connection);

    std::string inner = u8fmt("UPDATE %s SET sort_order=? WHERE id=?", plural.c_str());
    db::Statement innerStmt(inner.c_str(), connection);

    int count = 0;
    while (outerStmt.Step() == db::Row) {
        innerStmt.BindInt32(0, count);
        innerStmt.BindInt64(1, outerStmt.ColumnInt64(0));
        innerStmt.Step();
        innerStmt.Reset();
        ++count;
    }

    std::this_thread::yield();

    return count;
}

void Indexer::SyncOptimize() {
    db::ScopedTransaction transaction(this->dbConnection);
    optimize(this->dbConnection, "genre", "genres");
    optimize(this->dbConnection, "artist", "artists");
    optimize(this->dbConnection, "album", "albums");
}

void Indexer::ProcessAddRemoveQueue() {
    std::unique_lock<decltype(this->stateMutex)> lock(this->stateMutex);
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


namespace {
    struct AnalyzerInstance {
        void* instance = nullptr;
        const mc_analyzer_vtable* api = nullptr;
        mc_preferences_host_api preferences = MC_PREFERENCES_HOST_API_DEFAULT;
        mc_plugin_services services = MC_PLUGIN_SERVICES_DEFAULT;

        ~AnalyzerInstance() {
            if (instance) {
                api->destroy(instance);
            }
        }
    };
}

void Indexer::RunAnalyzers() {
    const std::string dataDirectory = GetDataDirectory();
    std::vector<std::unique_ptr<AnalyzerInstance>> analyzers;
    for (const auto& plugin : musik::core::plugin::ListPlugins()) {
        const auto factory = musik::core::plugin::FindAnalyzerFactory(plugin.id);
        if (!plugin.enabled || !factory) {
            continue;
        }
        auto analyzer = std::make_unique<AnalyzerInstance>();
        analyzer->preferences = musik::core::plugin::ReadSettingsService(plugin.name);
        analyzer->services.data_directory = { dataDirectory.data(), dataDirectory.size() };
        analyzer->services.preferences = &analyzer->preferences;
        analyzer->services.media = &musik::core::plugin::Environment().Media();
        const auto status = factory.Invoke([&](const mc_analyzer_factory& table) {
            analyzer->api = table.instance_api;
            const auto result = table.create(table.context, &analyzer->services, &analyzer->instance, nullptr);
            musik::contract::Assert(result == MC_OK || !analyzer->instance,
                "failed analyzer factory returned an instance");
            return result;
        });
        if (status != MC_OK) {
            musik::debug::error(TAG, "could not create analyzer " + plugin.name +
                " (" + std::to_string(status) + ")");
            return;
        }
        musik::contract::Assert(analyzer->instance, "analyzer factory returned no instance");
        analyzers.push_back(std::move(analyzer));
    }
    if (analyzers.empty()) {
        return;
    }

    int64_t trackId = 0;
    db::Statement getNextTrack(
        "SELECT id FROM tracks WHERE id>? ORDER BY id LIMIT 1",
        this->dbConnection);
    getNextTrack.BindInt64(0, trackId);

    while (getNextTrack.Step() == db::Row) {
        trackId = getNextTrack.ColumnInt64(0);
        getNextTrack.ResetAndUnbind();

        auto track = std::make_shared<IndexerTrack>(trackId);
        TrackMetadataQuery query(track, LibraryFactory::Instance().DefaultLocalLibrary());
        query.Run(this->dbConnection);

        if (query.GetStatus() == IQuery::Finished) {
            TagStore store(track);
            const auto writer = musik::core::plugin::CreateMetadataWriter(store);
            std::vector<AnalyzerInstance*> started;
            std::vector<AnalyzerInstance*> running;
            for (const auto& analyzer : analyzers) {
                started.push_back(analyzer.get());
                if (analyzer->api->begin(analyzer->instance, &writer, nullptr) == MC_OK) {
                    running.push_back(analyzer.get());
                }
            }

            bool decoded = false;
            auto stream = audio::Stream::Create(2048, 2.0, StreamFlags::NoDSP);
            {
                std::lock_guard lock(this->stateMutex);
                if (!this->Bail() && !running.empty()) {
                    this->analysisStream = stream;
                }
            }
            if (this->Bail() && stream) {
                stream->Interrupt();
            }
            if (!running.empty() && stream && !this->Bail() && stream->OpenStream(track->Uri(), nullptr)) {
                IBuffer* buffer = nullptr;
                while (!this->Bail() && !running.empty() && (buffer = stream->GetNextProcessedOutputBuffer())) {
                    const mc_pcm_view pcm {
                        .struct_size = sizeof(mc_pcm_view),
                        .data = buffer->BufferPointer(),
                        .frame_count = static_cast<size_t>(buffer->Samples() / buffer->Channels()),
                        .format = {
                            .struct_size = sizeof(mc_audio_format),
                            .sample_rate = static_cast<uint32_t>(buffer->SampleRate()),
                            .channels = static_cast<uint32_t>(buffer->Channels()),
                            .channel_layout = 0,
                        },
                    };
                    std::erase_if(running, [&](AnalyzerInstance* analyzer) {
                        return analyzer->api->process(analyzer->instance, &pcm, &writer, nullptr) != MC_OK;
                    });
                    /* analyzers consume each slice synchronously, so the next
                    decode can reuse the same bounded pool. */
                    stream->RecycleBuffer(buffer);
                }
                decoded = !this->Bail() && stream->Eof() && stream->DecodeStatus() == MC_OK;
            }

            {
                std::lock_guard lock(this->stateMutex);
                this->analysisStream.reset();
            }

            /* analyzers share one track record. if any fails, discard every
            analyzer's edits for this track so partial results are not saved. */
            const bool commit = decoded && !this->Bail() && running.size() == started.size();
            bool success = commit;
            for (auto* analyzer : started) {
                const auto result = analyzer->api->finish(analyzer->instance, &writer, commit ? 1 : 0, nullptr);
                if (result != MC_OK) {
                    success = false;
                }
            }
            if (success) {
                /* serialize the final save with cancellation admission so a
                cancelled scan cannot persist analyzer results afterward. */
                std::lock_guard lock(this->stateMutex);
                if (!this->Bail()) {
                    track->Save(this->dbConnection, this->libraryPath);
                }
            }
        }

        if (this->Bail()) {
            return;
        }
        getNextTrack.BindInt64(0, trackId);
    }
}

ITagStore* Indexer::CreateWriter() {
    return new TagStore(std::make_shared<IndexerTrack>(0));
}

bool Indexer::Save(IIndexerSource* source, ITagStore* store, const char* externalId) {
    if (!source || source->SourceId() == 0 || !store) {
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
    if (!source || source->SourceId() == 0) {
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
    if (!source || source->SourceId() == 0) {
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
    if (!source) {
        return 0;
    }

    const auto id = source->SourceId();
    return (id != 0) ? this->RemoveAllForSourceId(id) : 0;
}

int Indexer::RemoveAllForSourceId(int sourceId) {
    db::Statement stmt("DELETE FROM tracks WHERE source_id=?", this->dbConnection);
    stmt.BindInt32(0, sourceId);
    return (stmt.Step() == db::Okay) ? dbConnection.LastModifiedRowCount() : 0;
}

void Indexer::CommitProgress(IIndexerSource* source, unsigned updatedTracks) {
    if (source &&
        this->currentSource &&
        this->currentSource->SourceId() == source->SourceId() &&
        trackTransaction)
    {
        trackTransaction->CommitAndRestart();
    }

    if (updatedTracks) {
        this->IncrementTracksScanned(updatedTracks);
    }
}

int64_t Indexer::GetLastModifiedTime(IIndexerSource* source, const char* externalId) {
    if (source && externalId && strlen(externalId)) {
        db::Statement stmt("SELECT filetime FROM tracks t where source_id=? AND external_id=?", dbConnection);
        stmt.BindInt32(0, source->SourceId());
        stmt.BindText(1, externalId);
        if (stmt.Step() == db::Row) {
            return stmt.ColumnInt64(0);
        }
    }

    return -1;
}

void Indexer::ScheduleRescan(IIndexerSource* source) {
    if (source && source->SourceId() != 0) {
        this->Schedule(SyncType::Sources, source);
    }
}

bool Indexer::Bail() noexcept {
    return this->cancelRequested ||
        this->state == StateStopping ||
        this->state == StateStopped;
}
