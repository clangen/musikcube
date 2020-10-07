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

#include <core/net/WebSocketClient.h>
#include <core/library/query/AllCategoriesQuery.h>
#include <atomic>
#include <json.hpp>

using namespace musik::core;
using namespace musik::core::library;
using namespace musik::core::net;

using Client = WebSocketClient::Client;
using Message = WebSocketClient::Message;
using Connection = WebSocketClient::Connection;

static const std::string sUri = "ws://127.0.0.1:7905";

static std::atomic<int> nextMessageId(0);

static inline std::string generateMessageId() {
    return "integrated-websocket-client-" + std::to_string(nextMessageId.fetch_add(1));
}

static inline std::string createAuthenticateRequest(const std::string& password) {
    const nlohmann::json authRequestJson = {
        { "name", "authenticate" },
        { "type" , "request" },
        { "id", "this-is-a-random-auth-message-id-0" },
        { "device_id", "remote-random-device" },
        { "options", {
            { "password", password }
        }}
    };
    return authRequestJson.dump();
}

static inline std::string createSendRawQueryRequest(const std::string& rawQuery, const std::string& messageId) {
    nlohmann::json rawQueryJson = {
        { "name", "send_raw_query" },
        { "type" , "request" },
        { "id", messageId },
        { "device_id", "integrated-websocket-client" },
        { "options", {
            { "raw_query_data", rawQuery }
        }}
    };
    return rawQueryJson.dump();
}

static inline bool extractMessageIdAndQueryResult(
    nlohmann::json& responseJson, std::string& messageId, std::string& rawResult)
{
    if (responseJson["name"].get<std::string>() != "send_raw_query") {
        return false;
    }
    messageId = responseJson["id"].get<std::string>();
    rawResult = responseJson["options"]["raw_query_data"].get<std::string>();
    return true;
}

WebSocketClient::WebSocketClient(Listener* listener) {
    this->listener = listener;

    websocketpp::lib::error_code ec;
    client.init_asio(&io, ec);

    client.set_open_handler([this](Connection connection) {
        this->SetState(State::Authenticating);
        this->client.send(
            connection,
            createAuthenticateRequest(""),
            websocketpp::frame::opcode::text);
    });

    client.set_fail_handler([this](Connection connection) {
        this->SetState(State::Disconnected);
    });

    client.set_message_handler([this](Connection connection, Message message) {
        nlohmann::json responseJson = nlohmann::json::parse(message->get_payload());
        auto name = responseJson["name"].get<std::string>();
        if (name == "authenticate") {
            this->connection = connection;
            this->SetState(State::Connected);
        }
        else if (name == "send_raw_query") {
            std::string messageId, rawResult;
            if (extractMessageIdAndQueryResult(responseJson, messageId, rawResult)) {
                auto query = this->messageIdToQuery[messageId];
                if (query) {
                    query->DeserializeResult(rawResult);
                    this->listener->OnClientQuerySucceeded(this, messageId, query);
                }
                else {
                    this->listener->OnClientQueryFailed(
                        this, messageId, query, ErrorCode::QueryNotFound);
                }
            }
        }
    });

    client.set_close_handler([this](Connection connection) {
        if (this->state == State::Authenticating) {
            this->listener->OnClientInvalidPassword(this);
        }
        this->SetState(State::Disconnected);
    });

    this->Reconnect();
}

WebSocketClient::~WebSocketClient() {
    this->Disconnect();
}

void WebSocketClient::Disconnect() {
    std::shared_ptr<std::thread> oldThread;

    {
        std::unique_lock<decltype(this->mutex)> lock(this->mutex);
        oldThread = this->thread;
        this->thread.reset();
    }

    if (oldThread) {
        io.stop();
        oldThread->join();
    }
}

std::string WebSocketClient::EnqueueQuery(Query query) {
    std::unique_lock<decltype(this->mutex)> lock(this->mutex);
    if (this->state == State::Connected || this->state == State::Disconnected) {
        auto messageId = generateMessageId();
        messageIdToQuery[messageId] = query;
        if (this->state == State::Connected) {
            this->client.send(
                this->connection,
                createSendRawQueryRequest(query->SerializeQuery(), messageId),
                websocketpp::frame::opcode::text);
        }
        return messageId;
    }
    return "";
}

void WebSocketClient::Reconnect() {
    std::unique_lock<decltype(this->mutex)> lock(this->mutex);

    this->Disconnect();

    io.restart();

    this->thread.reset(new std::thread([&]() {
        websocketpp::lib::error_code ec;
        Client::connection_ptr connection = client.get_connection(sUri, ec);
        client.connect(connection);
        this->SetState(State::Connecting);
        client.run();
        this->SetState(State::Disconnected);
    }));
}

void WebSocketClient::InvalidatePendingQueries() {
    std::unique_lock<decltype(this->mutex)> lock(this->mutex);

    for (auto& kv : this->messageIdToQuery) {
        this->listener->OnClientQueryFailed(
            this, kv.first, kv.second, ErrorCode::Disconnected);
    }

    this->messageIdToQuery.clear();
}

void WebSocketClient::SendPendingQueries() {
    std::unique_lock<decltype(this->mutex)> lock(this->mutex);

    for (auto& kv : this->messageIdToQuery) {
        auto messageId = kv.first;
        auto query = kv.second;
        this->client.send(
            this->connection,
            createSendRawQueryRequest(query->SerializeQuery(), messageId),
            websocketpp::frame::opcode::text);
    }
}

void WebSocketClient::SetState(State state) {
    std::unique_lock<decltype(this->mutex)> lock(this->mutex);
    if (state != this->state) {
        auto oldState = this->state;

        switch (state) {
            case State::Disconnected:
                this->connection.reset();
                this->InvalidatePendingQueries();
                break;

            case State::Connected:
                this->SendPendingQueries();
                break;
        }

        this->state = state;
        this->listener->OnClientStateChanged(this, state, oldState);
    }
}
