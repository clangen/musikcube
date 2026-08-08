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

/** @file PiggyWebSocketClient.h
 *  @brief WebSocket client for the Piggy (lyrics/metadata) service.
 *  @details Singleton client that maintains a background connection to the Piggy
 *      server, queues outgoing JSON messages, and marshals incoming messages onto
 *      a runtime message queue. */

#include <musikcore/config.h>
#include <musikcore/net/RawWebSocketClient.h>
#include <musikcore/runtime/IMessageQueue.h>

#include <sigslot/sigslot.h>

#include <thread>
#include <deque>
#include <atomic>
#include <memory>

#pragma warning(push, 0)
#include <nlohmann/json.hpp>
#pragma warning(pop)

/** @namespace musik::core::net
 *  @brief Network clients: WebSocket connections to remote services. */
namespace musik { namespace core { namespace net {

    /** @brief Manages a WebSocket connection to the Piggy service.
     *  @details Runs its own thread and asio io_context. Messages enqueued via
     *      EnqueueMessage() are flushed once connected; incoming messages are
     *      delivered on the bound message queue's thread. */
    class PiggyWebSocketClient: public musik::core::runtime::IMessageTarget {
        public:
            using ClientPtr = std::unique_ptr<RawWebSocketClient>; /**< Raw client alias. */
            using ClientMessage = websocketpp::config::asio_client::message_type::ptr; /**< Incoming message alias. */
            using Connection = websocketpp::connection_hdl; /**< Connection handle alias. */
            using Message = std::shared_ptr<nlohmann::json>; /**< JSON message alias. */
            using MessageQueue = musik::core::runtime::IMessageQueue; /**< Queue alias. */

            /** @brief Connection lifecycle state. */
            enum class State: int {
                Disconnected = 0, /**< Not connected. */
                Connecting = 1,   /**< Connecting. */
                Connected = 3,    /**< Connected. */
                Disconnecting = 4,/**< Disconnecting. */
            };

            /** @brief Reasons the connection may have failed. */
            enum class ConnectionError : int {
                None = 0,             /**< No error. */
                ConnectionFailed = 1, /**< Could not connect. */
                ClosedByServer = 2,   /**< Closed by the remote end. */
            };

            /** @brief Emitted when the connection state changes.
             *  @details Arguments: the client, the new state and the old state. */
            sigslot::signal3<PiggyWebSocketClient*, State, State> StateChanged;

            /** @return The process-wide Piggy client singleton.
             *  @param messageQueue The queue used for message delivery. */
            static std::shared_ptr<PiggyWebSocketClient> Instance(MessageQueue* messageQueue);
            /** @brief Shuts down and releases the singleton. */
            static void Shutdown();

            PiggyWebSocketClient(const PiggyWebSocketClient&) = delete;
            virtual ~PiggyWebSocketClient();

            /** @brief Connects to the Piggy service.
             *  @param host The host name or address.
             *  @param port The port (default 8347).
             *  @param useTls Whether to use a TLS connection. */
            void Connect(const std::string& host, unsigned short port = 8347, bool useTls = false);
            /** @brief Reconnects using the last connection parameters. */
            void Reconnect();
            /** @brief Disconnects from the service. */
            void Disconnect();

            /** @brief Queues a JSON message to send once connected.
             *  @param message The message to send. */
            void EnqueueMessage(Message message);

            /** @return The current connection state. */
            State ConnectionState() const;
            /** @return The last connection error, if any. */
            ConnectionError LastConnectionError() const;
            /** @return The URI of the current/last connection. */
            std::string Uri() const;

            /** @brief Binds the message queue used for delivery.
             *  @param messageQueue The queue to use. */
            void SetMessageQueue(MessageQueue* messageQueue);
            /** @brief Handles runtime messages.
             *  @param message The incoming message. */
            void ProcessMessage(musik::core::runtime::IMessage& message) override;

        protected:
            /** @brief Constructs the client (use Instance()).
             *  @param messageQueue The queue used for delivery. */
            PiggyWebSocketClient(MessageQueue* messageQueue);

        private:
            /** @brief Updates and broadcasts the connection state.
             *  @param state The new state. */
            void SetState(State state);
            /** @brief Records a disconnect and its cause.
             *  @param errorCode The failure reason. */
            void SetDisconnected(ConnectionError errorCode);
            /** @brief Flushes queued messages over the connection. */
            void SendPendingMessages();

            ClientPtr rawClient; /**< Underlying raw websocket client. */
            Connection connection; /**< Active connection handle. */
            std::string sessionId; /**< Server-assigned session id. */
            asio::io_context io; /**< Connection io_context. */
            std::unique_ptr<std::thread> thread; /**< Connection thread. */
            mutable std::recursive_mutex mutex; /**< Guards connection state. */
            bool useTls{ false }; /**< Whether TLS is in use. */
            std::string uri; /**< Connection URI. */
            std::deque<Message> pendingMessages; /**< Messages awaiting a connection. */
            std::atomic<bool> quit{ false }; /**< Signals thread shutdown. */
            ConnectionError connectionError{ ConnectionError::None }; /**< Last connection error. */
            State state{ State::Disconnected }; /**< Current state. */
            MessageQueue* messageQueue; /**< Queue used for delivery. */
    };

} } }

