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

/** @file RemoteLibrary.h
 *  @brief ILibrary implementation backed by a remote musikcube server.
 *  @details Serializes queries and sends them over a WebSocket connection to a
 *      remote library. Results are deserialized when the server responds. Also
 *      acts as the IResourceLocator, producing streamable URIs for remote tracks. */

#include <musikcore/config.h>
#include <musikcore/db/Connection.h>

#include <musikcore/library/ILibrary.h>
#include <musikcore/library/IIndexer.h>
#include <musikcore/library/IQuery.h>
#include <musikcore/net/WebSocketClient.h>

#include <thread>
#include <mutex>
#include <condition_variable>
#include <unordered_map>
#include <string>

/** @namespace musik::core::library
 *  @brief Library implementations and the query framework that runs against them. */
namespace musik { namespace core { namespace library {

    /** @brief ILibrary that executes serialized queries against a remote server.
     *  @details Runs queued queries on a worker thread, serializes them and sends
     *      them through a WebSocketClient. Query results are matched back to their
     *      pending contexts and delivered via callbacks and signals. Reconnects
     *      are driven by preferences and WebSocket state changes. */
    class RemoteLibrary :
        public ILibrary,
        public musik::core::runtime::IMessageTarget,
        public std::enable_shared_from_this<RemoteLibrary>,
        public musik::core::net::WebSocketClient::Listener,
        public ILibrary::IResourceLocator
    {
        public:
            using Client = musik::core::net::WebSocketClient; /**< Client alias. */
            using Query = std::shared_ptr<musik::core::db::ISerializableQuery>; /**< Serializable query alias. */
            using MessageQueue = musik::core::runtime::IMessageQueue; /**< Queue alias. */

            /** @brief Creates and connects a remote library.
             *  @param name The library display name.
             *  @param id The unique library id.
             *  @param messageQueue The queue used to deliver events.
             *  @return A shared ILibrary instance. */
            static ILibraryPtr Create(std::string name, int id, MessageQueue* messageQueue);

            /** @brief Constructs a remote library (use Create()).
             *  @param name The library display name.
             *  @param id The unique library id.
             *  @param messageQueue The queue used to deliver events. */
            RemoteLibrary(std::string name, int id, MessageQueue* messageQueue);
            RemoteLibrary(const RemoteLibrary&) = delete;
            virtual ~RemoteLibrary();

            /* ILibrary */
            /** @brief Queues a serializable query for remote execution.
             *  @param query The query to run.
             *  @param cb Optional completion callback.
             *  @return The query id. */
            int Enqueue(QueryPtr query, Callback = Callback()) override;
            /** @brief Runs a query and blocks until the remote result arrives.
             *  @param query The query to run.
             *  @param timeoutMs Maximum wait; kWaitIndefinite to block forever.
             *  @param cb Optional completion callback.
             *  @return The query id. */
            int EnqueueAndWait(QueryPtr query, size_t timeoutMs = kWaitIndefinite, Callback = Callback()) override;
            /** @return nullptr (remote libraries have no local indexer). */
            musik::core::IIndexer *Indexer() override;
            /** @return The library id. */
            int Id() override;
            /** @return The library display name. */
            const std::string& Name() override;
            /** @brief Binds the message queue used for event delivery.
             *  @param queue The queue to use. */
            void SetMessageQueue(musik::core::runtime::IMessageQueue& queue) override;
            /** @return The bound message queue. */
            musik::core::runtime::IMessageQueue& GetMessageQueue() noexcept override { return *messageQueue; }
            /** @return This library itself (as the resource locator). */
            ILibrary::IResourceLocator& GetResourceLocator() noexcept override { return *this; }
            /** @return true if the library has been configured with a server address. */
            bool IsConfigured() override;
            /** @return The current connection state. */
            ConnectionState GetConnectionState() const override { return this->connectionState; }
            /** @return Type::Remote. */
            Type GetType() const noexcept override { return Type::Remote; }
            /** @brief Closes the library, stopping its worker thread and connection. */
            void Close() override;

            /* IMessageTarget */
            /** @brief Handles runtime messages sent to the library.
             *  @param message The incoming message. */
            void ProcessMessage(musik::core::runtime::IMessage &message) override;

            /* WebSocketClient::Listener */
            /** @brief Called when the server rejects the configured password.
             *  @param client The websocket client. */
            void OnClientInvalidPassword(Client* client) override;
            /** @brief Called when the websocket connection state changes.
             *  @param client The websocket client.
             *  @param newState The new state.
             *  @param oldState The previous state. */
            void OnClientStateChanged(Client* client, State newState, State oldState) override;
            /** @brief Called when a remote query succeeds.
             *  @param client The websocket client.
             *  @param messageId The id of the request message.
             *  @param query The query that was sent. */
            void OnClientQuerySucceeded(Client* client, const std::string& messageId, Query query) override;
            /** @brief Called when a remote query fails.
             *  @param client The websocket client.
             *  @param messageId The id of the request message.
             *  @param query The query that was sent.
             *  @param reason The failure reason. */
            void OnClientQueryFailed(Client* client, const std::string& messageId, Query query, Client::QueryError reason) override;

            /* IResourceLocator */
            /** @brief Resolves the playable URI for a remote track.
             *  @param track The track to resolve.
             *  @param defaultUri Fallback URI if the track has none.
             *  @return The resolved streamable URI. */
            std::string GetTrackUri(musik::core::sdk::ITrack* track, const std::string& defaultUri) override;

            /* RemoteLibrary */
            /** @brief Reloads the server address/password from preferences and reconnects. */
            void ReloadConnectionFromPreferences();
            /** @return A reference to the internal websocket client. */
            const musik::core::net::WebSocketClient& WebSocketClient() const;

        private:
            /** @brief Internal message posted when a remote query completes. */
            class QueryCompletedMessage;

            /** @brief A queued query plus its optional completion callback. */
            struct QueryContext {
                std::shared_ptr<musik::core::db::ISerializableQuery> query; /**< The query to run. */
                Callback callback; /**< Optional completion callback. */
            };

            using QueryContextPtr = std::shared_ptr<QueryContext>; /**< Shared context alias. */
            using QueryList = std::list<QueryContextPtr>; /**< Pending query queue. */

            /** @brief Executes a query context (dispatches to loopback or websocket).
             *  @param context The query to run. */
            void RunQuery(QueryContextPtr context);
            /** @brief Executes a query locally (when the remote is unreachable).
             *  @param context The query to run. */
            void RunQueryOnLoopback(QueryContextPtr context);
            /** @brief Sends a query over the websocket client.
             *  @param context The query to run. */
            void RunQueryOnWebSocketClient(QueryContextPtr context);

            /** @brief Handles a successful remote result.
             *  @param messageId The id of the request message.
             *  @param query The query that completed. */
            void OnQueryCompleted(const std::string& messageId, Query query);
            /** @brief Completes a query context.
             *  @param context The query that finished. */
            void OnQueryCompleted(QueryContextPtr context);
            /** @brief Notifies a context's callback and the QueryCompleted signal.
             *  @param context The finished query context. */
            void NotifyQueryCompleted(QueryContextPtr context);

            /** @return true if the query is currently awaiting a response.
             *  @param query The query to check. */
            bool IsQueryInFlight(Query query);

            /** @brief Worker thread entry point. */
            void ThreadProc();
            /** @brief Pops the next pending query, blocking until one is available.
             *  @return The next query context. */
            QueryContextPtr GetNextQuery();

            QueryList queryQueue; /**< Pending queries. */

            musik::core::runtime::IMessageQueue* messageQueue; /**< Event delivery queue. */
            musik::core::net::WebSocketClient wsc; /**< Underlying websocket client. */

            std::string identifier; /**< Stable identifier for the library. */
            int id;                 /**< Unique library id. */
            std::string name;       /**< Display name. */

            std::unordered_map<std::string, QueryContextPtr> queriesInFlight; /**< Queries awaiting responses. */

            std::unique_ptr<std::thread> thread; /**< Worker thread. */
            std::condition_variable_any queueCondition, syncQueryCondition; /**< Queue and sync signals. */
            std::recursive_mutex queueMutex; /**< Guards the query queue. */
            std::atomic<ConnectionState> connectionState{ ConnectionState::Disconnected }; /**< Connection state. */
            std::atomic<bool> exit; /**< Signals thread shutdown. */

    };

} } }
