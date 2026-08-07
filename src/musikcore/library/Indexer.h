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

#pragma once

/** @file Indexer.h
 *  @brief Background metadata indexer for the local library.
 *  @details Implements IIndexer, IIndexerWriter and IIndexerNotifier. Scans
 *      configured directories, reads track tags via ITagReader plugins, and
 *      writes the results into a SQLite database. Runs on a dedicated thread and
 *      synchronizes sources on demand. */

#include <musikcore/db/Connection.h>
#include <musikcore/sdk/ITagReader.h>
#include <musikcore/sdk/IDecoderFactory.h>
#include <musikcore/sdk/IIndexerWriter.h>
#include <musikcore/sdk/IIndexerNotifier.h>
#include <musikcore/library/IIndexer.h>
#include <musikcore/support/Preferences.h>
#include <musikcore/support/ThreadGroup.h>

#pragma warning(push, 0)
#include <sigslot/sigslot.h>
#include <asio/io_context.hpp>
#pragma warning(pop)

#include <filesystem>
#include <thread>
#include <condition_variable>
#include <deque>
#include <vector>
#include <atomic>
#include <set>
#include <map>

/** @namespace musik::core
 *  @brief Core application services: libraries, indexing, playback and utilities. */
namespace musik { namespace core {

    /** @brief Scans and indexes music metadata into the local library database.
     *  @details Walks configured directories, uses ITagReader plugins to extract
     *      metadata, and stores tracks through the IIndexerWriter interface.
     *      IndexerSource plugins can also contribute tracks and request rescans.
     *      The worker thread picks up scheduled syncs and processes metadata
     *      concurrently using an asio io_context. */
    class Indexer :
        public musik::core::IIndexer,
        public musik::core::sdk::IIndexerWriter,
        public musik::core::sdk::IIndexerNotifier
    {
        public:
            /** @brief Creates an indexer for the given library database.
             *  @param libraryPath Directory containing the library database.
             *  @param dbFilename Name of the SQLite database file. */
            Indexer(
                const std::string& libraryPath,
                const std::string& dbFilename);

            Indexer(const Indexer&) = delete;

            virtual ~Indexer();

            /* IIndexer */
            /** @brief Adds a directory to the indexed paths.
             *  @param path The directory to add. */
            void AddPath(const std::string& path) override;
            /** @brief Removes a directory from the indexed paths.
             *  @param path The directory to remove. */
            void RemovePath(const std::string& path) override;
            /** @brief Retrieves the configured index paths.
             *  @param paths Output vector receiving the paths. */
            void GetPaths(std::vector<std::string>& paths) override;
            /** @brief Schedules a synchronization.
             *  @param type The scope of the sync to run. */
            void Schedule(SyncType type) override;
            /** @brief Stops indexing and shuts down the worker thread. */
            void Shutdown() override;

            /** @return The current indexer state. */
            State GetState() noexcept override {
                return this->state;
            }

            /* IIndexerWriter */
            /** @brief Creates a writer for persisting tag metadata.
             *  @return A new ITagStore to fill and pass to Save(). */
            musik::core::sdk::ITagStore* CreateWriter() override;
            /** @brief Removes the track with the given URI from the index.
             *  @param source The source the track belongs to.
             *  @param uri The URI of the track to remove.
             *  @return true on success. */
            bool RemoveByUri(musik::core::sdk::IIndexerSource* source, const char* uri) override;
            /** @brief Removes the track with the given external id from the index.
             *  @param source The source the track belongs to.
             *  @param id The external id of the track to remove.
             *  @return true on success. */
            bool RemoveByExternalId(musik::core::sdk::IIndexerSource* source, const char* id) override;
            /** @brief Removes all tracks contributed by a source.
             *  @param source The source whose tracks should be removed.
             *  @return The number of removed tracks. */
            int RemoveAll(musik::core::sdk::IIndexerSource* source) override;
            /** @brief Commits a source's indexing progress to the database.
             *  @param source The source reporting progress.
             *  @param updatedTracks Number of tracks updated by the source. */
            void CommitProgress(musik::core::sdk::IIndexerSource* source, unsigned updatedTracks) override;
            /** @brief Returns the last modified time recorded for a track.
             *  @param source The source the track belongs to.
             *  @param id The external id of the track.
             *  @return The last modified time, or -1 if unknown. */
            int64_t GetLastModifiedTime(musik::core::sdk::IIndexerSource* source, const char* id) override;

            /** @brief Saves a tag store into the index.
             *  @param source The source that produced the metadata.
             *  @param store The tag store to persist.
             *  @param externalId Optional external id for the track.
             *  @return true on success. */
            bool Save(
                musik::core::sdk::IIndexerSource* source,
                musik::core::sdk::ITagStore* store,
                const char* externalId = "") override;

            /* IIndexerNotifier */
            /** @brief Requests that a source be rescanned.
             *  @param source The source to rescan. */
            void ScheduleRescan(musik::core::sdk::IIndexerSource* source) override;

        private:
            /** @brief A queued add/remove path operation. */
            struct AddRemoveContext {
                bool add{ false }; /**< true to add the path, false to remove it. */
                std::string path;  /**< The path to add or remove. */
            };

            /** @brief A queued synchronization request. */
            struct SyncContext {
                SyncType type; /**< Scope of the sync. */
                int sourceId;  /**< Target source id, if any. */
            };

            typedef std::vector<std::shared_ptr<
                musik::core::sdk::ITagReader>> TagReaderList; /**< Registered tag readers. */

            typedef std::vector<std::shared_ptr<
                musik::core::sdk::IDecoderFactory>> DecoderList; /**< Registered decoder factories. */

            typedef std::vector<std::shared_ptr<
                musik::core::sdk::IIndexerSource>> IndexerSourceList; /**< Registered indexer sources. */

            /** @brief Worker thread entry point. */
            void ThreadLoop();

            /** @brief Runs a synchronization pass for a sync context.
             *  @param context The sync request.
             *  @param io The io_context used for concurrent metadata reads. */
            void Synchronize(const SyncContext& context, asio::io_context* io);

            /** @brief Completes a sync, committing pending work.
             *  @param context The sync request being finalized. */
            void FinalizeSync(const SyncContext& context);

            /** @brief Removes indexed tracks that no longer exist on disk. */
            void SyncDelete();
            /** @brief Cleans up orphaned database records. */
            void SyncCleanup();

            /** @brief Rebuilds the ordering of playlist tracks. */
            void SyncPlaylistTracksOrder();

            /** @brief Scans a single source's paths for metadata.
             *  @param source The source to scan.
             *  @param paths The paths to scan.
             *  @return The scan result. */
            musik::core::sdk::ScanResult SyncSource(
                musik::core::sdk::IIndexerSource* source,
                const std::vector<std::string>& paths);

            /** @brief Processes queued path add/remove operations. */
            void ProcessAddRemoveQueue();
            /** @brief Optimizes the database after a sync. */
            void SyncOptimize();
            /** @brief Runs metadata analyzers over newly indexed tracks. */
            void RunAnalyzers();
            /** @return The ids of sources no longer registered. */
            std::set<int> GetOrphanedSourceIds();
            /** @brief Removes all tracks for a source id.
             *  @param sourceId The source id to clear.
             *  @return The number of removed tracks. */
            int RemoveAllForSourceId(int sourceId);

            /** @brief Schedules a sync targeting a specific source.
             *  @param type The scope of the sync.
             *  @param source The target source. */
            void Schedule(SyncType type, musik::core::sdk::IIndexerSource *source);
            /** @brief Increments the scanned-track counter.
             *  @param delta Number of tracks to add (default 1). */
            void IncrementTracksScanned(int delta = 1);

            /** @brief Recursively scans a directory for music files.
             *  @param io The io_context for concurrent reads.
             *  @param syncRoot The root path being synced.
             *  @param currentPath The directory being scanned.
             *  @param pathId Database id of the directory. */
            void SyncDirectory(
                asio::io_context* io,
                const std::string& syncRoot,
                const std::string& currentPath,
                int64_t pathId);

            /** @brief Reads metadata from a file and persists it.
             *  @param io The io_context for the read.
             *  @param path The file to read.
             *  @param pathId Database id of the containing directory. */
            void ReadMetadataFromFile(
                asio::io_context* io,
                const std::filesystem::path& path,
                const std::string& pathId);

            /** @return true if the indexer should abort its current work. */
            bool Bail() noexcept;

            db::Connection dbConnection; /**< Connection to the library database. */
            std::string libraryPath;     /**< Directory holding the database. */
            std::string dbFilename;      /**< Name of the database file. */
            std::atomic<State> state;    /**< Current indexer state. */
            std::mutex stateMutex;       /**< Guards state changes. */
            std::condition_variable_any waitCondition; /**< Signals the worker thread. */
            std::unique_ptr<std::thread> thread;       /**< Worker thread. */
            std::atomic<int> incrementalUrisScanned, totalUrisScanned; /**< Progress counters. */
            std::deque<AddRemoveContext> addRemoveQueue; /**< Pending path operations. */
            std::deque<SyncContext> syncQueue;           /**< Pending sync requests. */
            TagReaderList tagReaders;      /**< Registered tag readers. */
            DecoderList audioDecoders;     /**< Registered decoder factories. */
            IndexerSourceList sources;     /**< Registered indexer sources. */
            std::shared_ptr<musik::core::Preferences> prefs; /**< Indexer preferences. */
            std::shared_ptr<musik::core::db::ScopedTransaction> trackTransaction; /**< Active track write transaction. */
            std::vector<std::string> paths; /**< Configured index paths. */
            std::shared_ptr<musik::core::sdk::IIndexerSource> currentSource; /**< Source being processed. */
    };

    typedef std::shared_ptr<Indexer> IndexerPtr; /**< Shared indexer alias. */

} }
