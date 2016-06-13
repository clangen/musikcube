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

#include <core/support/ThreadHelper.h>
#include <core/db/Connection.h>
#include <core/sdk/IMetadataReader.h>
#include <core/sdk/IDecoderFactory.h>
#include <core/library/IIndexer.h>

#include <sigslot/sigslot.h>

#include <boost/thread/thread.hpp>

#include <deque>
#include <vector>

namespace musik { namespace core {

    //////////////////////////////////////////
    ///\brief
    ///The Indexer is the class that syncronizes musik tracks with the database
    ///
    ///The Indexer is often a member of classes like the LocalLibrary
    ///but can also be used as a standalone class for indexing files.
    ///All you need to do is create a Indexer object and call Startup()
    //////////////////////////////////////////
    class Indexer : public IIndexer, public ThreadHelper, private boost::noncopyable {
        public:
            Indexer(
                const std::string& libraryPath,
                const std::string& dbFilename);

            virtual ~Indexer();

            virtual void AddPath(const std::string& paths);
            virtual void RemovePath(const std::string& paths);
            virtual void GetPaths(std::vector<std::string>& paths);
            virtual void Synchronize(bool restart = false);

        private:
            void ThreadLoop();

            bool Restarted();

            void SyncDelete();
            void SyncCleanup();
            void ProcessAddRemoveQueue();
            void SyncOptimize();
            void RunAnalyzers();

            void SynchronizeInternal();

            void SyncDirectory(
                const std::string& syncRoot,
                const std::string& currentPath,
                DBID pathId);

            db::Connection dbConnection;

            std::string libraryPath;
            std::string dbFilename;

            int status;
            bool restart;

            boost::thread *thread;
            boost::mutex progressMutex;

            int filesIndexed;
            int filesSaved;


            class AddRemoveContext {
                public:
                    bool add;
                    std::string path;
            };

            typedef std::vector<std::shared_ptr<
                metadata::IMetadataReader> > MetadataReaderList;

            typedef std::vector<std::shared_ptr<
                musik::core::audio::IDecoderFactory> > DecoderList;

            std::deque<AddRemoveContext> addRemoveQueue;

            MetadataReaderList metadataReaders;
            DecoderList audioDecoders;
    };

    typedef std::shared_ptr<Indexer> IndexerPtr;

} }
