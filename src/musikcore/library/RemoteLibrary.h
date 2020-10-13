//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004-2020 musikcube team
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

namespace musik { namespace core { namespace library {

    class RemoteLibrary :
        public ILibrary,
        public musik::core::runtime::IMessageTarget,
        public std::enable_shared_from_this<RemoteLibrary>,
        public musik::core::net::WebSocketClient::Listener,
        public ILibrary::IResourceLocator
    {
        public:
            using Client = musik::core::net::WebSocketClient;
            using Query = std::shared_ptr<musik::core::db::ISerializableQuery>;
            static ILibraryPtr Create(std::string name, int id);

            RemoteLibrary(const RemoteLibrary&) = delete;
            virtual ~RemoteLibrary();

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
            virtual ILibrary::IResourceLocator& GetResourceLocator() override { return *this; }
            virtual bool IsConfigured() override;
            virtual ConnectionState GetConnectionState() const override { return this->connectionState; }
            virtual Type GetType() const override { return Type::Remote; }
            virtual void Close() override;

            /* IMessageTarget */
            virtual void ProcessMessage(musik::core::runtime::IMessage &message) override;

            /* WebSocketClient::Listener */
            virtual void OnClientInvalidPassword(Client* client) override;
            virtual void OnClientStateChanged(Client* client, State newState, State oldState) override;
            virtual void OnClientQuerySucceeded(Client* client, const std::string& messageId, Query query) override;
            virtual void OnClientQueryFailed(Client* client, const std::string& messageId, Query query, Client::QueryError reason) override;

            /* IResourceLocator */
            virtual std::string GetTrackUri(musik::core::sdk::ITrack* track, const std::string& defaultUri) override;

            /* RemoteLibrary */
            void ReloadConnectionFromPreferences();
            const musik::core::net::WebSocketClient& WebSocketClient() const;

        private:
            class QueryCompletedMessage;

            struct QueryContext {
                std::shared_ptr<musik::core::db::ISerializableQuery> query;
                Callback callback;
            };

            using QueryContextPtr = std::shared_ptr<QueryContext>;
            using QueryList = std::list<QueryContextPtr>;

            RemoteLibrary(std::string name, int id); /* ctor */

            void RunQuery(QueryContextPtr context);
            void RunQueryOnLoopback(QueryContextPtr context);
            void RunQueryOnWebSocketClient(QueryContextPtr context);

            void OnQueryCompleted(const std::string& messageId, Query query);
            void OnQueryCompleted(QueryContextPtr context);
            void NotifyQueryCompleted(QueryContextPtr context);

            bool IsQueryInFlight(Query query);

            void ThreadProc();
            QueryContextPtr GetNextQuery();

            QueryList queryQueue;

            musik::core::runtime::IMessageQueue* messageQueue;
            musik::core::net::WebSocketClient wsc;

            std::string identifier;
            int id;
            std::string name;

            std::unordered_map<std::string, QueryContextPtr> queriesInFlight;

            std::thread* thread;
            std::condition_variable_any queueCondition, syncQueryCondition;
            std::recursive_mutex queueMutex;
            std::atomic<ConnectionState> connectionState{ ConnectionState::Disconnected };
            std::atomic<bool> exit;

    };

} } }
