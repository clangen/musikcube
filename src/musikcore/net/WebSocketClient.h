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
#include <musikcore/net/RawWebSocketClient.h>
#include <musikcore/library/IQuery.h>
#include <musikcore/runtime/IMessageQueue.h>
#include <thread>
#include <unordered_map>
#include <atomic>
#include <memory>

namespace musik { namespace core { namespace net {

    class WebSocketClient: public musik::core::runtime::IMessageTarget {
        public:
            using ClientPtr = std::unique_ptr<RawWebSocketClient>;
            using ClientMessage = websocketpp::config::asio_client::message_type::ptr;
            using Connection = websocketpp::connection_hdl;
            using Query = std::shared_ptr<musik::core::db::ISerializableQuery>;

            enum class State: int {
                Disconnected = 0,
                Connecting = 1,
                Authenticating = 2,
                Connected = 3,
                Disconnecting = 4,
            };

            enum class QueryError: int {
                QueryFailed = 1,
                Disconnected = 2,
                AuthFailed = 3,
                QueryNotFound = 4,
                ParseFailed = 5,
            };

            enum class ConnectionError : int {
                None = 0,
                InvalidPassword = 1,
                IncompatibleVersion = 2,
                ConnectionFailed = 3,
                ClosedByServer = 4,
            };

            class Listener {
                public:
                    using Client = WebSocketClient;
                    using State = Client::State;
                    using QueryError = Client::QueryError;
                    virtual void OnClientInvalidPassword(Client* client) = 0;
                    virtual void OnClientStateChanged(Client* client, State newState, State oldState) = 0;
                    virtual void OnClientQuerySucceeded(Client* client, const std::string& messageId, Query query) = 0;
                    virtual void OnClientQueryFailed(Client* client, const std::string& messageId, Query query, QueryError result) = 0;
            };

            WebSocketClient(
                musik::core::runtime::IMessageQueue* messageQueue,
                Listener* listener);

            WebSocketClient(const WebSocketClient&) = delete;
            virtual ~WebSocketClient();

            void Connect(
                const std::string& host,
                unsigned short port,
                const std::string& password,
                bool useTls);

            void Reconnect();
            void Disconnect();

            State ConnectionState() const;
            ConnectionError LastConnectionError() const;
            std::string LastServerVersion() const;
            std::string Uri() const;

            std::string EnqueueQuery(Query query);

            void SetMessageQueue(musik::core::runtime::IMessageQueue* messageQueue);

            /* IMessageTarget */
            void ProcessMessage(musik::core::runtime::IMessage& message) override;

        private:
            void SetState(State state);
            void InvalidatePendingQueries();
            void SendPendingQueries();
            void SetDisconnected(ConnectionError errorCode);

            ClientPtr rawClient;
            Connection connection;
            boost::asio::io_service io;
            std::unique_ptr<std::thread> thread;
            mutable std::recursive_mutex mutex;
            bool useTls{ false };
            std::string uri, password;
            std::unordered_map<std::string, Query> messageIdToQuery;
            std::atomic<bool> quit{ false };
            ConnectionError connectionError{ ConnectionError::None };
            std::string serverVersion;
            State state{ State::Disconnected };
            Listener* listener{ nullptr };
            musik::core::runtime::IMessageQueue* messageQueue;
    };

} } }

