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

namespace musik { namespace core { namespace net {

    class RawWebSocketClient {
        public:
            using PlainTextClient = websocketpp::client<websocketpp::config::asio_client>;
            using PlainTextClientPtr = std::unique_ptr<PlainTextClient>;
            using TlsClient = websocketpp::client<websocketpp::config::asio_tls_client>;
            using TlsClientPtr = std::unique_ptr<TlsClient>;
            using SslContext = std::shared_ptr<asio::ssl::context>;
            using Message = websocketpp::config::asio_client::message_type::ptr;
            using Connection = websocketpp::connection_hdl;

            using OpenHandler = std::function<void(Connection)>;
            using FailHandler = std::function<void(Connection)>;
            using MessageHandler = std::function<void(Connection, Message)>;
            using CloseHandler = std::function<void(Connection)>;
            using SendMessageErrorHandler = std::function<void(std::error_code)>;

            enum class Mode: int {
                PlainText = 0,
                TLS = 1
            };

            RawWebSocketClient(asio::io_context& io);
            RawWebSocketClient(const RawWebSocketClient&) = delete;
            ~RawWebSocketClient();

            void SetMode(Mode mode);
            void SetOpenHandler(OpenHandler openHandler);
            void SetFailHandler(FailHandler failHandler);
            void SetMessageHandler(MessageHandler messageHandler);
            void SetCloseHandler(CloseHandler closeHandler);
            void SetSendMessageErrorHandler(SendMessageErrorHandler errorHandler);
            void Send(Connection connection, const std::string& message);
            void SetPongTimeout(long timeoutMs);
            void Connect(const std::string& uri);
            void Run();

        private:

            Mode mode;
            TlsClientPtr tlsClient;
            PlainTextClientPtr plainTextClient;
            SendMessageErrorHandler sendMessageErrorHandler;
    };

} } }

