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

#include <musikcore/db/Connection.h>
#include <musikcore/plugin/TagReader.h>
#include <musikcore/plugin/IndexerSource.h>
#include <musikcore/sdk/ITagStore.h>
#include <musikcore/library/IIndexer.h>
#include <musikcore/support/Preferences.h>
#include <musikcore/support/ThreadGroup.h>

#pragma warning(push, 0)
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

namespace musik { namespace core {

    namespace audio { class Stream; }

    class Indexer :
        public musik::core::IIndexer
    {
        public:
            Indexer(
                const std::string& libraryPath,
                const std::string& dbFilename);

            Indexer(const Indexer&) = delete;

            virtual ~Indexer();

            /* IIndexer */
            void AddPath(const std::string& path) override;
            void RemovePath(const std::string& path) override;
            void GetPaths(std::vector<std::string>& paths) override;
            void Schedule(SyncType type) override;
            void Cancel() override;
            void Shutdown() override;

            State GetState() noexcept override {
                return this->state;
            }

            musik::core::sdk::ITagStore* CreateWriter();
            bool RemoveByUri(musik::core::plugin::IndexerSource* source, const char* uri);
            bool RemoveByExternalId(musik::core::plugin::IndexerSource* source, const char* id);
            int RemoveAll(musik::core::plugin::IndexerSource* source);
            void CommitProgress(musik::core::plugin::IndexerSource* source, unsigned updatedTracks);
            bool GetLastModifiedTime(musik::core::plugin::IndexerSource* source, const char* id, int64_t& modified);

            bool Save(
                musik::core::plugin::IndexerSource* source,
                musik::core::sdk::ITagStore* store,
                const char* externalId = "");

            bool ScheduleRescan(musik::core::plugin::IndexerSource* source);
            void StartPendingSourceScans();

        private:
            struct AddRemoveContext {
                bool add{ false };
                std::string path;
            };

            struct SyncContext {
                SyncType type;
                int sourceId;
            };

            typedef std::vector<std::shared_ptr<
                musik::core::plugin::TagReader>> TagReaderList;

            typedef std::vector<std::shared_ptr<
                musik::core::plugin::IndexerSource>> IndexerSourceList;

            void ThreadLoop();
            void CompleteScan();
            void InterruptSource(const std::shared_ptr<musik::core::plugin::IndexerSource>& source, uint64_t generation);

            void Synchronize(const SyncContext& context, asio::io_context* io);

            void FinalizeSync(const SyncContext& context);

            void SyncDelete();
            void SyncCleanup();

            void SyncPlaylistTracksOrder();

            musik::core::sdk::ScanResult SyncSource(
                musik::core::plugin::IndexerSource* source,
                const std::vector<std::string>& paths);

            void ProcessAddRemoveQueue();
            void SyncOptimize();
            void RunAnalyzers();
            std::set<int> GetOrphanedSourceIds();
            int RemoveAllForSourceId(int sourceId);

            bool Schedule(SyncType type, musik::core::plugin::IndexerSource *source);
            void IncrementTracksScanned(int delta = 1);

            void SyncDirectory(
                asio::io_context* io,
                const std::string& syncRoot,
                const std::string& currentPath,
                int64_t pathId);

            void ReadMetadataFromFile(
                asio::io_context* io,
                const std::filesystem::path& path,
                const std::string& pathId);

            bool Bail() noexcept;

            db::Connection dbConnection;
            std::string libraryPath;
            std::string dbFilename;
            std::atomic<State> state;
            std::atomic<bool> cancelRequested { false };
            std::mutex stateMutex;
            bool shuttingDown { false };
            bool sourcesActive { false };
            std::set<musik::core::plugin::IndexerSource*> pendingSourceRescans;
            std::recursive_mutex sourceLifecycleMutex;
            uint64_t scanGeneration { 0 };
            std::condition_variable_any waitCondition;
            std::unique_ptr<std::thread> thread;
            std::atomic<int> incrementalUrisScanned, totalUrisScanned;
            std::deque<AddRemoveContext> addRemoveQueue;
            std::deque<SyncContext> syncQueue;
            TagReaderList tagReaders;
            IndexerSourceList sources;
            std::shared_ptr<musik::core::Preferences> prefs;
            std::shared_ptr<musik::core::db::ScopedTransaction> trackTransaction;
            bool refreshMetadata { false };
            std::vector<std::string> paths;
            std::shared_ptr<musik::core::plugin::IndexerSource> currentSource;
            std::shared_ptr<musik::core::audio::Stream> analysisStream;
    };

    typedef std::shared_ptr<Indexer> IndexerPtr;

} }
