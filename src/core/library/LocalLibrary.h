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

#pragma once

#include <core/config.h>
#include <core/db/Connection.h>

#include <core/library/ILibrary.h>
#include <core/library/IIndexer.h>
#include <core/library/IQuery.h>
#include <core/library/query/local/LocalQueryBase.h>

#include <thread>
#include <mutex>
#include <condition_variable>

#include <sigslot/sigslot.h>
#include <string>

namespace musik { namespace core { namespace library {

    class LocalLibrary :
        public ILibrary,
        public musik::core::runtime::IMessageTarget,
        public std::enable_shared_from_this<LocalLibrary>
    {
        public:
            using LocalQuery = musik::core::db::LocalQueryBase;
            using LocalQueryPtr = std::shared_ptr<LocalQuery>;

            static ILibraryPtr Create(std::string name, int id);

            LocalLibrary(const LocalLibrary&) = delete;
            virtual ~LocalLibrary();

            /* ILibrary */
            virtual int Enqueue(
                QueryPtr query,
                unsigned int options = 0,
                Callback = Callback()) override;

            virtual musik::core::IIndexer *Indexer() override;
            virtual int Id() override;
            virtual const std::string& Name() override;
            virtual void SetMessageQueue(musik::core::runtime::IMessageQueue& queue) override;
            virtual musik::core::runtime::IMessageQueue& GetMessageQueue() override { return *messageQueue; }
            virtual void Close() override;

            /* IMessageTarget */
            virtual void ProcessMessage(musik::core::runtime::IMessage &message) override;

            /* implementation specific */
            db::Connection& GetConnection() { return this->db; }
            std::string GetLibraryDirectory();
            std::string GetDatabaseFilename();
            static void CreateDatabase(db::Connection &db);

            /* indexes */
            static void DropIndexes(db::Connection &db);
            static void CreateIndexes(db::Connection &db);
            static void InvalidateTrackMetadata(db::Connection &db);

        private:
            class QueryCompletedMessage;

            struct QueryContext {
                LocalQueryPtr query;
                Callback callback;
            };

            using QueryContextPtr = std::shared_ptr<QueryContext>;
            using QueryList = std::list<QueryContextPtr>;

            LocalLibrary(std::string name, int id); /* ctor */

            void RunQuery(QueryContextPtr context, bool notify = true);
            void ThreadProc();
            QueryContextPtr GetNextQuery();

            QueryList queryQueue;

            musik::core::runtime::IMessageQueue* messageQueue;

            std::string identifier;
            int id;
            std::string name;

            std::thread* thread;
            std::condition_variable_any queueCondition;
            std::recursive_mutex mutex;
            std::atomic<bool> exit;

            core::IIndexer *indexer;
            core::db::Connection db;
    };

} } }
