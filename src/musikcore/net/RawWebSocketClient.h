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

/** @file RawWebSocketClient.h
 *  @brief Thin wrapper around a websocketpp ASIO websocket client.
 *  @details Provides plain-text and TLS websocket clients with simple handler
 *      callbacks for open, fail, message, close and send errors. Higher-level
 *      clients build on this class. */

#include <musikcore/config.h>

#ifdef timeout
#undef timeout
#endif

#pragma warning(push, 0)
#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/config/asio_client.hpp>
#include <websocketpp/client.hpp>
#pragma warning(pop)

#include <atomic>
#include <memory>
#include <functional>
#include <system_error>

/** @namespace musik::core::net
 *  @brief Network clients: WebSocket connections to remote services. */
namespace musik { namespace core { namespace net {

    /** @brief Low-level websocketpp client wrapper.
     *  @details Selects between a plain-text and a TLS websocketpp client. Callers
     *      install handler callbacks and drive the event loop with Run(). */
    class RawWebSocketClient {
        public:
            using PlainTextClient = websocketpp::client<websocketpp::config::asio_client>; /**< Plain-text client type. */
            using PlainTextClientPtr = std::unique_ptr<PlainTextClient>; /**< Plain-text client alias. */
            using TlsClient = websocketpp::client<websocketpp::config::asio_tls_client>; /**< TLS client type. */
            using TlsClientPtr = std::unique_ptr<TlsClient>; /**< TLS client alias. */
            using SslContext = std::shared_ptr<asio::ssl::context>; /**< SSL context alias. */
            using Message = websocketpp::config::asio_client::message_type::ptr; /**< Incoming message alias. */
            using Connection = websocketpp::connection_hdl; /**< Connection handle alias. */

            using OpenHandler = std::function<void(Connection)>; /**< Called when a connection opens. */
            using FailHandler = std::function<void(Connection)>; /**< Called when a connection fails. */
            using MessageHandler = std::function<void(Connection, Message)>; /**< Called on incoming messages. */
            using CloseHandler = std::function<void(Connection)>; /**< Called when a connection closes. */
            using SendMessageErrorHandler = std::function<void(std::error_code)>; /**< Called on send errors. */

            /** @brief The transport mode of the client. */
            enum class Mode: int {
                PlainText = 0, /**< Unencrypted websocket. */
                TLS = 1        /**< TLS-encrypted websocket. */
            };

            /** @brief Creates a raw websocket client bound to an io_context.
             *  @param io The asio io_context the client runs on. */
            RawWebSocketClient(asio::io_context& io);
            RawWebSocketClient(const RawWebSocketClient&) = delete;
            ~RawWebSocketClient();

            /** @brief Selects the transport mode.
             *  @param mode PlainText or TLS. */
            void SetMode(Mode mode);
            /** @brief Installs the open handler.
             *  @param openHandler The handler. */
            void SetOpenHandler(OpenHandler openHandler);
            /** @brief Installs the fail handler.
             *  @param failHandler The handler. */
            void SetFailHandler(FailHandler failHandler);
            /** @brief Installs the message handler.
             *  @param messageHandler The handler. */
            void SetMessageHandler(MessageHandler messageHandler);
            /** @brief Installs the close handler.
             *  @param closeHandler The handler. */
            void SetCloseHandler(CloseHandler closeHandler);
            /** @brief Installs the send-error handler.
             *  @param errorHandler The handler. */
            void SetSendMessageErrorHandler(SendMessageErrorHandler errorHandler);
            /** @brief Sends a text message over a connection.
             *  @param connection The connection handle.
             *  @param message The message text. */
            void Send(Connection connection, const std::string& message);
            /** @brief Sets the pong timeout used for keepalives.
             *  @param timeoutMs Timeout in milliseconds. */
            void SetPongTimeout(long timeoutMs);
            /** @brief Opens a connection to the given URI.
             *  @param uri The websocket URI (ws:// or wss://). */
            void Connect(const std::string& uri);
            /** @brief Runs the asio event loop (blocks the calling thread). */
            void Run();

        private:

            Mode mode; /**< Transport mode. */
            TlsClientPtr tlsClient; /**< TLS client instance. */
            PlainTextClientPtr plainTextClient; /**< Plain-text client instance. */
            SendMessageErrorHandler sendMessageErrorHandler; /**< Send error callback. */
    };

} } }

