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

#include "pch.hpp"

#include <core/net/WebSocketclient.h>

#include <json.hpp>

using namespace musik::core::net;

using Client = WebSocketClient::Client;
using Message = WebSocketClient::Message;
using Connection = WebSocketClient::Connection;

static const std::string sUri = "ws://192.168.1.221:7905";

static const nlohmann::json sAuthRequest = {
    { "name", "authenticate" },
    { "type" , "request" },
    { "id", "this-is-a-random-auth-message-id" },
    { "device_id", "remote-random-device" },
    { "options", {
        { "password", "" }
    }}
};

WebSocketClient::WebSocketClient() {
    io.restart();

    websocketpp::lib::error_code ec;
    client.init_asio(&io, ec);

    client.set_open_handler([this](auto connection) {
        this->client.send(connection, sAuthRequest.dump(), websocketpp::frame::opcode::text);
    });
    client.set_fail_handler([this](auto connection) {
        /* todo fail handler */
        auto failed = "failed";
    });
    client.set_message_handler([this](auto connection, Message message) {
        auto response = message->get_payload();
    });
    client.set_close_handler([this](auto connection) {
        /* todo connection closed */
        auto closed = "closed";
    });

    this->Reconnect();
}

WebSocketClient::~WebSocketClient() {
    std::unique_lock<decltype(this->mutex)> lock(this->mutex);

    if (this->thread) {
        io.stop();
        this->thread->join();
        this->thread.reset();
    }
}

void WebSocketClient::Reconnect() {
    std::unique_lock<decltype(this->mutex)> lock(this->mutex);

    if (this->thread) {
        io.stop();
        this->thread->join();
    }

    this->thread.reset(new std::thread([&]() {
        websocketpp::lib::error_code ec;
        Client::connection_ptr connection = client.get_connection(sUri, ec);
        client.connect(connection);
        client.run();
    }));
}