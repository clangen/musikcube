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

/** @file WebSocketClient.h
 *  @brief WebSocket client that runs serializable library queries remotely.
 *  @details Connects to a musikcube server, authenticates with a password, and
 *      sends serialized ISerializableQuery objects. Responses are matched back to
 *      the query by message id and reported to a Listener. */

#include <musikcore/config.h>
#include <musikcore/net/RawWebSocketClient.h>
#include <musikcore/library/IQuery.h>
#include <musikcore/runtime/IMessageQueue.h>
#include <thread>
#include <unordered_map>
#include <atomic>
#include <memory>

/** @namespace musik::core::net
 *  @brief Network clients: WebSocket connections to remote services. */
namespace musik { namespace core { namespace net {

    /** @brief Executes library queries against a remote server over WebSocket.
     *  @details Maintains a background thread, authenticates with the configured
     *      password, and tracks in-flight queries by message id. All listener
     *      callbacks are marshaled onto the bound message queue's thread. */
    class WebSocketClient: public musik::core::runtime::IMessageTarget {
        public:
            using ClientPtr = std::unique_ptr<RawWebSocketClient>; /**< Raw client alias. */
            using ClientMessage = websocketpp::config::asio_client::message_type::ptr; /**< Incoming message alias. */
            using Connection = websocketpp::connection_hdl; /**< Connection handle alias. */
            using Query = std::shared_ptr<musik::core::db::ISerializableQuery>; /**< Serializable query alias. */

            /** @brief Connection lifecycle state. */
            enum class State: int {
                Disconnected = 0, /**< Not connected. */
                Connecting = 1,   /**< Connecting. */
                Authenticating = 2, /**< Authenticating with the server. */
                Connected = 3,    /**< Connected and ready. */
                Disconnecting = 4,/**< Disconnecting. */
            };

            /** @brief Reasons a remote query may fail. */
            enum class QueryError: int {
                QueryFailed = 1,   /**< Server reported a query failure. */
                Disconnected = 2,  /**< Disconnected before a response. */
                AuthFailed = 3,    /**< Authentication failed. */
                QueryNotFound = 4, /**< Server could not find the query. */
                ParseFailed = 5,   /**< Response could not be parsed. */
            };

            /** @brief Reasons the connection may have failed. */
            enum class ConnectionError : int {
                None = 0,             /**< No error. */
                InvalidPassword = 1,  /**< Password was rejected. */
                IncompatibleVersion = 2, /**< Server version mismatch. */
                ConnectionFailed = 3, /**< Could not connect. */
                ClosedByServer = 4,   /**< Closed by the remote end. */
            };

            /** @brief Callback interface for websocket client events. */
            class Listener {
                public:
                    using Client = WebSocketClient; /**< Client alias. */
                    using State = Client::State; /**< State alias. */
                    using QueryError = Client::QueryError; /**< Query error alias. */
                    /** @brief Called when the server rejects the password.
                     *  @param client The client. */
                    virtual void OnClientInvalidPassword(Client* client) = 0;
                    /** @brief Called when the connection state changes.
                     *  @param client The client.
                     *  @param newState The new state.
                     *  @param oldState The previous state. */
                    virtual void OnClientStateChanged(Client* client, State newState, State oldState) = 0;
                    /** @brief Called when a remote query succeeds.
                     *  @param client The client.
                     *  @param messageId The request message id.
                     *  @param query The query that completed. */
                    virtual void OnClientQuerySucceeded(Client* client, const std::string& messageId, Query query) = 0;
                    /** @brief Called when a remote query fails.
                     *  @param client The client.
                     *  @param messageId The request message id.
                     *  @param query The query that failed.
                     *  @param result The failure reason. */
                    virtual void OnClientQueryFailed(Client* client, const std::string& messageId, Query query, QueryError result) = 0;
            };

            /** @brief Creates a websocket client.
             *  @param messageQueue The queue used for listener callbacks.
             *  @param listener The listener for client events. */
            WebSocketClient(
                musik::core::runtime::IMessageQueue* messageQueue,
                Listener* listener);

            WebSocketClient(const WebSocketClient&) = delete;
            virtual ~WebSocketClient();

            /** @brief Connects to a server and authenticates.
             *  @param host The server host.
             *  @param port The server port.
             *  @param password The authentication password.
             *  @param useTls Whether to use TLS. */
            void Connect(
                const std::string& host,
                unsigned short port,
                const std::string& password,
                bool useTls);

            /** @brief Reconnects using the last connection parameters. */
            void Reconnect();
            /** @brief Disconnects from the server. */
            void Disconnect();

            /** @return The current connection state. */
            State ConnectionState() const;
            /** @return The last connection error, if any. */
            ConnectionError LastConnectionError() const;
            /** @return The version string reported by the last connected server. */
            std::string LastServerVersion() const;
            /** @return The URI of the current/last connection. */
            std::string Uri() const;

            /** @brief Serializes and sends a query to the server.
             *  @param query The query to run.
             *  @return The message id assigned to the query. */
            std::string EnqueueQuery(Query query);

            /** @brief Binds the message queue used for listener callbacks.
             *  @param messageQueue The queue to use. */
            void SetMessageQueue(musik::core::runtime::IMessageQueue* messageQueue);

            /* IMessageTarget */
            /** @brief Handles runtime messages.
             *  @param message The incoming message. */
            void ProcessMessage(musik::core::runtime::IMessage& message) override;

        private:
            /** @brief Updates and broadcasts the connection state.
             *  @param state The new state. */
            void SetState(State state);
            /** @brief Fails all queries awaiting a response. */
            void InvalidatePendingQueries();
            /** @brief Flushes queued queries over the connection. */
            void SendPendingQueries();
            /** @brief Records a disconnect and its cause.
             *  @param errorCode The failure reason. */
            void SetDisconnected(ConnectionError errorCode);

            ClientPtr rawClient; /**< Underlying raw websocket client. */
            Connection connection; /**< Active connection handle. */
            asio::io_context io; /**< Connection io_context. */
            std::unique_ptr<std::thread> thread; /**< Connection thread. */
            mutable std::recursive_mutex mutex; /**< Guards connection state. */
            bool useTls{ false }; /**< Whether TLS is in use. */
            std::string uri, password; /**< Connection URI and password. */
            std::unordered_map<std::string, Query> messageIdToQuery; /**< In-flight queries by message id. */
            std::atomic<bool> quit{ false }; /**< Signals thread shutdown. */
            ConnectionError connectionError{ ConnectionError::None }; /**< Last connection error. */
            std::string serverVersion; /**< Server version string. */
            State state{ State::Disconnected }; /**< Current state. */
            Listener* listener{ nullptr }; /**< Event listener. */
            musik::core::runtime::IMessageQueue* messageQueue; /**< Queue used for callbacks. */
    };

} } }

