//////////////////////////////////////////////////////////////////////////////
//
// License Agreement:
//
// The following are Copyright © 2008, Daniel Önnerby
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

#include <core/config.h>
#include <core/ThreadHelper.h>
#include <core/PluginFactory.h>
#include <core/Plugin/IMetaDataReader.h>
#include <core/db/Connection.h>

#include <boost/thread/thread.hpp>
#include <boost/thread/condition.hpp>
#include <boost/thread/mutex.hpp>

#include <deque>

namespace musik{ namespace core{
    class Indexer : public ThreadHelper,private boost::noncopyable {
        public:
            Indexer(void);
            ~Indexer(void);

            void AddPath(utfstring sPath);
            void RemovePath(utfstring sPath);
            std::vector<utfstring> GetPaths();

            bool Startup(utfstring setLibraryPath);
            void ThreadLoop();

            int iTimeout;

//            double getProgress();
            utfstring GetStatus();
            int GetStatusCode();
            void RestartSync(bool bNewRestart=true);
            bool Restarted();

            utfstring database;

        private:
            
            db::Connection dbConnection;

            utfstring libraryPath;
            int iStatus;
            bool bRestart;

            boost::thread *oThread;
            boost::mutex oProgressMutex;

            double iProgress;
            int iNOFFiles;
            int iFilesIndexed;

            void CountFiles(utfstring &sFolder);

            void Synchronize();
            void SyncDirectory(utfstring &sFolder,DBINT iParentFolderId,DBINT iPathId);
            void SyncDelete(std::vector<DBINT> aPaths);
            void SyncCleanup();
            void SyncAddRemovePaths();
            void SyncOptimize();

            static int LongRunningQueryInterrupt(void *indexer);

            class _AddRemovePath{
                public:
                    bool add;
                    utfstring path;
            };

            typedef std::vector<boost::shared_ptr<Plugin::IMetaDataReader>> MetadataReaderList;

            std::deque<_AddRemovePath> addRemoveQueue;

            MetadataReaderList metadataReaders;

    };
} }


