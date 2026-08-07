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

/** @file LocalLibrary.h
 *  @brief Local SQLite-backed music library.
 *  @details Owns a database connection and an Indexer, and executes queued
 *      queries on a dedicated background thread. Implements the ILibrary
 *      interface for the on-disk library. */

#include <musikcore/config.h>
#include <musikcore/db/Connection.h>

#include <musikcore/library/ILibrary.h>
#include <musikcore/library/IIndexer.h>
#include <musikcore/library/IQuery.h>
#include <musikcore/library/QueryBase.h>

#include <thread>
#include <mutex>
#include <condition_variable>
#include <string>

#include <sigslot/sigslot.h>

/** @namespace musik::core::library
 *  @brief Library implementations and the query framework that runs against them. */
namespace musik { namespace core { namespace library {

    /** @brief ILibrary implementation backed by a local SQLite database.
     *  @details Runs a query-processing thread that executes queued QueryBase
     *      queries against a Connection, notifying callbacks and emitting
     *      QueryCompleted on the bound message queue. Also exposes the schema
     *      helpers used to create the database and its indexes. */
    class LocalLibrary :
        public ILibrary,
        public musik::core::runtime::IMessageTarget,
        public std::enable_shared_from_this<LocalLibrary>
    {
        public:
            using LocalQuery = musik::core::library::query::QueryBase; /**< Query base alias. */
            using LocalQueryPtr = std::shared_ptr<LocalQuery>; /**< Shared local query alias. */
            using MessageQueue = musik::core::runtime::IMessageQueue; /**< Queue alias. */
            using IIndexer = musik::core::IIndexer; /**< Indexer interface alias. */

            /** @brief Creates and opens a new local library.
             *  @param name The library display name.
             *  @param id The unique library id.
             *  @param messageQueue The queue used to deliver events.
             *  @return A shared ILibrary instance. */
            static ILibraryPtr Create(std::string name, int id, MessageQueue* messageQueue);

            LocalLibrary(const LocalLibrary&) = delete;
            virtual ~LocalLibrary();

            /* ILibrary */
            /** @brief Queues a query for asynchronous execution.
             *  @param query The query to run.
             *  @param cb Optional completion callback.
             *  @return The query id. */
            int Enqueue(QueryPtr query, Callback cb = Callback()) override;
            /** @brief Runs a query and blocks until it completes.
             *  @param query The query to run.
             *  @param timeoutMs Maximum wait; kWaitIndefinite to block forever.
             *  @param cb Optional completion callback.
             *  @return The query id. */
            int EnqueueAndWait(QueryPtr query, size_t timeoutMs = kWaitIndefinite, Callback cb = Callback()) override;
            /** @return The indexer owned by this library. */
            IIndexer *Indexer() override;
            /** @return The library id. */
            int Id() override;
            /** @return The library display name. */
            const std::string& Name() override;
            /** @brief Binds the message queue used for event delivery.
             *  @param queue The queue to use. */
            void SetMessageQueue(musik::core::runtime::IMessageQueue& queue) override;
            /** @return The bound message queue. */
            MessageQueue& GetMessageQueue() override { return *messageQueue; }
            /** @return The resource locator for local tracks. */
            IResourceLocator& GetResourceLocator() override;
            /** @return true if the library has been configured. */
            bool IsConfigured() override;
            /** @return ConnectionState::Connected (local libraries are always connected). */
            ConnectionState GetConnectionState() const override { return ConnectionState::Connected; }
            /** @return Type::Local. */
            Type GetType() const override { return Type::Local; }
            /** @brief Closes the library, stopping its query thread. */
            void Close() override;

            /* IMessageTarget */
            /** @brief Handles runtime messages sent to the library.
             *  @param message The incoming message. */
            void ProcessMessage(musik::core::runtime::IMessage &message) override;

            /* implementation specific */
            /** @return The database connection used by the library. */
            db::Connection& GetConnection() { return this->db; }
            /** @return The directory holding the library database. */
            std::string GetLibraryDirectory();
            /** @return The filename of the library database. */
            std::string GetDatabaseFilename();
            /** @brief Creates the initial database schema.
             *  @param db The connection to create the schema on. */
            static void CreateDatabase(db::Connection &db);

            /* indexes */
            /** @brief Drops all performance indexes.
             *  @param db The connection to act on. */
            static void DropIndexes(db::Connection &db);
            /** @brief Creates all performance indexes.
             *  @param db The connection to act on. */
            static void CreateIndexes(db::Connection &db);
            /** @brief Invalidates cached track metadata (used after bulk updates).
             *  @param db The connection to act on. */
            static void InvalidateTrackMetadata(db::Connection &db);

        private:
            /** @brief Internal message posted when a query completes. */
            class QueryCompletedMessage;

            /** @brief A queued query plus its optional completion callback. */
            struct QueryContext {
                LocalQueryPtr query;    /**< The query to execute. */
                Callback callback;      /**< Optional completion callback. */
            };

            using QueryContextPtr = std::shared_ptr<QueryContext>; /**< Shared context alias. */
            using QueryList = std::list<QueryContextPtr>; /**< Pending query queue. */

            /** @brief Constructs a local library (use Create()).
             *  @param name The library display name.
             *  @param id The unique library id.
             *  @param messageQueue The queue used to deliver events. */
            LocalLibrary(std::string name, int id, MessageQueue* messageQueue);

            /** @brief Executes a query and posts its completion.
             *  @param context The query to run.
             *  @param notify Whether to emit QueryCompleted. */
            void RunQuery(QueryContextPtr context, bool notify = true);
            /** @brief Query worker thread entry point. */
            void ThreadProc();
            /** @brief Pops the next pending query, blocking until one is available.
             *  @return The next query context. */
            QueryContextPtr GetNextQuery();

            QueryList queryQueue; /**< Pending queries. */

            musik::core::runtime::IMessageQueue* messageQueue; /**< Event delivery queue. */

            std::string identifier; /**< Stable identifier for the library. */
            int id;                 /**< Unique library id. */
            std::string name;       /**< Display name. */

            std::thread* thread;                  /**< Query worker thread. */
            std::condition_variable_any queueCondition; /**< Signals the worker thread. */
            std::recursive_mutex mutex;           /**< Guards the query queue. */
            std::atomic<bool> exit;               /**< Signals thread shutdown. */

            core::IIndexer *indexer; /**< Owning indexer. */
            core::db::Connection db; /**< Database connection. */
    };

} } }
