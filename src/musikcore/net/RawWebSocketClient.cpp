//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004-2021 musikcube team
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

#include "pch.hpp"

#include <musikcore/net/RawWebSocketClient.h>

using namespace musik::core;
using namespace musik::core::net;

static inline RawWebSocketClient::SslContext createSslContext() {
    RawWebSocketClient::SslContext ctx =
        std::make_shared<boost::asio::ssl::context>(boost::asio::ssl::context::sslv23);

    try {
        ctx->set_options(boost::asio::ssl::context::default_workarounds |
            boost::asio::ssl::context::no_sslv2 |
            boost::asio::ssl::context::no_sslv3 |
            boost::asio::ssl::context::single_dh_use |
            boost::asio::ssl::context::verify_none);
    }
    catch (std::exception& e) {
        std::cerr << "Error in context pointer: " << e.what() << std::endl;
    }
    return ctx;
}

RawWebSocketClient::RawWebSocketClient(boost::asio::io_service& io) {
    websocketpp::lib::error_code ec;

    plainTextClient = std::make_unique<PlainTextClient>();
    plainTextClient->clear_access_channels(websocketpp::log::alevel::all);
    plainTextClient->init_asio(&io, ec);

    tlsClient = std::make_unique<TlsClient>();
    tlsClient->clear_access_channels(websocketpp::log::alevel::all);
    tlsClient->init_asio(&io, ec);
    tlsClient->set_tls_init_handler([](Connection connection) -> SslContext {
        return createSslContext();
    });
}

RawWebSocketClient::~RawWebSocketClient() {
}

void RawWebSocketClient::SetMode(Mode mode) {
    this->mode = mode;
}

void RawWebSocketClient::SetOpenHandler(OpenHandler openHandler) {
    this->plainTextClient->set_open_handler(openHandler);
    this->tlsClient->set_open_handler(openHandler);
}

void RawWebSocketClient::SetFailHandler(FailHandler failHandler) {
    this->plainTextClient->set_fail_handler(failHandler);
    this->tlsClient->set_fail_handler(failHandler);
}

void RawWebSocketClient::SetMessageHandler(MessageHandler messageHandler) {
    this->plainTextClient->set_message_handler(messageHandler);
    this->tlsClient->set_message_handler(messageHandler);
}

void RawWebSocketClient::SetCloseHandler(CloseHandler closeHandler) {
    this->plainTextClient->set_close_handler(closeHandler);
    this->tlsClient->set_close_handler(closeHandler);
}

void RawWebSocketClient::SetSendMessageErrorHandler(SendMessageErrorHandler errorHandler) {
    this->sendMessageErrorHandler = errorHandler;
}

void RawWebSocketClient::Send(Connection connection, const std::string& message) {
    std::error_code ec;
    if (mode == Mode::PlainText) {
        this->plainTextClient->send(connection, message, websocketpp::frame::opcode::text, ec);
    }
    else if (mode == Mode::TLS) {
        this->tlsClient->send(connection, message, websocketpp::frame::opcode::text, ec);
    }
    if (ec && sendMessageErrorHandler) {
        sendMessageErrorHandler(ec);
    }
}

void RawWebSocketClient::SetPongTimeout(long timeoutMs) {
    this->plainTextClient->set_pong_timeout(timeoutMs);
    this->tlsClient->set_pong_timeout(timeoutMs);
}

void RawWebSocketClient::Connect(const std::string& uri) {
    websocketpp::lib::error_code ec;
    if (mode == Mode::PlainText) {
        PlainTextClient::connection_ptr connection = plainTextClient->get_connection(uri, ec);
        if (!ec) {
            plainTextClient->connect(connection);
        }
    }
    else if (mode == Mode::TLS) {
        TlsClient::connection_ptr connection = tlsClient->get_connection(uri, ec);
        if (!ec) {
            tlsClient->connect(connection);
        }
    }
}

void RawWebSocketClient::Run() {
    if (mode == Mode::PlainText) {
        plainTextClient->run();
    }
    else if (mode == Mode::TLS) {
        tlsClient->run();
    }
}

