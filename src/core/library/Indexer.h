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

#pragma once

#include <core/db/Connection.h>
#include <core/sdk/IMetadataReader.h>
#include <core/sdk/IDecoderFactory.h>
#include <core/sdk/IIndexerSink.h>
#include <core/library/IIndexer.h>
#include <core/support/Preferences.h>

#include <sigslot/sigslot.h>

#include <boost/thread/thread.hpp>
#include <boost/thread/condition.hpp>
#include <boost/filesystem.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/interprocess/sync/interprocess_semaphore.hpp>

#include <deque>
#include <vector>
#include <atomic>
#include <map>

namespace musik { namespace core {

    class Indexer : 
        public musik::core::IIndexer, 
        public musik::core::sdk::IIndexerSink,
        private boost::noncopyable 
    {
        public:
            Indexer(
                const std::string& libraryPath,
                const std::string& dbFilename);

            virtual ~Indexer();

            /* IIndexer */
            virtual void AddPath(const std::string& paths);
            virtual void RemovePath(const std::string& paths);
            virtual void GetPaths(std::vector<std::string>& paths);
            virtual void Synchronize(bool restart = false);
            virtual State GetState() { return this->state; }

            /* IIndexerSink */
            virtual musik::core::sdk::IRetainedTrackWriter* CreateWriter();
            virtual void Rescan(musik::core::sdk::IIndexerSource* source);
            virtual bool Remove(musik::core::sdk::IIndexerSource* source, const char* uri);
            virtual int RemoveAll(musik::core::sdk::IIndexerSource* source);
            virtual bool Save(
                musik::core::sdk::IIndexerSource* source, 
                musik::core::sdk::IRetainedTrackWriter* track);

        private:
            void ThreadLoop();

            bool Exited();
            void Wait();
            void Wait(const boost::xtime &oTime);

            bool Restarted();

            void FinalizeSync();
            void SyncDelete();
            void SyncCleanup();
            void ProcessAddRemoveQueue();
            void SyncOptimize();
            void RunAnalyzers();
            void SynchronizeInternal(boost::asio::io_service* io);

            void SyncDirectory(
                boost::asio::io_service* io,
                const std::string& syncRoot,
                const std::string& currentPath,
                DBID pathId);

            void ReadMetadataFromFile(
                const boost::filesystem::path& path,
                const std::string& pathId);

            db::Connection dbConnection;

            std::string libraryPath;
            std::string dbFilename;

            bool restart;
            bool exit;
            State state;

            boost::mutex stateMutex;
            boost::condition waitCondition;

            boost::thread *thread;
            std::atomic<size_t> filesSaved;

            struct AddRemoveContext {
                bool add;
                std::string path;
            };

            typedef std::vector<std::shared_ptr<
                musik::core::sdk::IMetadataReader> > MetadataReaderList;

            typedef std::vector<std::shared_ptr<
                musik::core::sdk::IDecoderFactory> > DecoderList;

            typedef std::vector<std::shared_ptr<
                musik::core::sdk::IIndexerSource> > IndexerSourceList;

            std::deque<AddRemoveContext> addRemoveQueue;

            MetadataReaderList metadataReaders;
            DecoderList audioDecoders;
            IndexerSourceList sources;

            std::shared_ptr<musik::core::Preferences> prefs;
            std::shared_ptr<musik::core::db::ScopedTransaction> trackTransaction;
            boost::interprocess::interprocess_semaphore readSemaphore;
    };

    typedef std::shared_ptr<Indexer> IndexerPtr;

} }
