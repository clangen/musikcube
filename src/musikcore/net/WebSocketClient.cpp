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

#include "pch.hpp"

#include <musikcore/net/WebSocketClient.h>
#include <musikcore/version.h>
#include <nlohmann/json.hpp>

using namespace musik::core;
using namespace musik::core::net;

using Client = WebSocketClient::Client;
using ClientPtr = WebSocketClient::ClientPtr;
using Message = WebSocketClient::Message;
using Connection = WebSocketClient::Connection;

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

static inline bool extractRawQueryResult(
    nlohmann::json& responseJson, std::string& rawResult)
{
    if (responseJson["name"].get<std::string>() != "send_raw_query") {
        return false;
    }
    rawResult = responseJson["options"]["raw_query_data"].get<std::string>();
    return true;
}

WebSocketClient::WebSocketClient(Listener* listener) {
    client = std::make_unique<Client>();

#if 1
    client->clear_access_channels(websocketpp::log::alevel::all);
#endif

    this->listener = listener;

    websocketpp::lib::error_code ec;
    client->init_asio(&io, ec);

    client->set_open_handler([this](Connection connection) {
        this->SetState(State::Authenticating);
        this->client->send(
            connection,
            createAuthenticateRequest(this->password),
            websocketpp::frame::opcode::text);
    });

    client->set_fail_handler([this](Connection connection) {
        this->SetDisconnected(ConnectionError::ConnectionFailed);
    });

    client->set_message_handler([this](Connection connection, Message message) {
        nlohmann::json responseJson = nlohmann::json::parse(message->get_payload());
        auto name = responseJson["name"].get<std::string>();
        auto messageId = responseJson["id"].get<std::string>();
        if (name == "authenticate") {
            this->connection = connection;
            auto appVersion = responseJson["options"]["environment"]["app_version"];
            if (!appVersion.is_string() || appVersion.get<std::string>() != VERSION) {
                this->SetDisconnected(ConnectionError::IncompatibleVersion);
            }
            else {
                this->SetState(State::Connected);
            }
        }
        else if (name == "send_raw_query") {
            auto query = this->messageIdToQuery[messageId];
            if (query) {
                this->messageIdToQuery.erase(messageId);
                auto& options = responseJson["options"];
                if (options.find("success") != options.end() && options["success"] == false) {
                    this->listener->OnClientQueryFailed(
                        this, messageId, query, QueryError::QueryFailed);
                }
                else {
                    std::string rawResult;
                    if (extractRawQueryResult(responseJson, rawResult)) {
                        if (query) {
                            query->DeserializeResult(rawResult);
                            this->listener->OnClientQuerySucceeded(this, messageId, query);
                        }
                        else {
                            this->listener->OnClientQueryFailed(
                                this, messageId, query, QueryError::QueryNotFound);
                        }
                    }
                }
            }
        }
    });

    client->set_close_handler([this](Connection connection) {
        if (this->state == State::Authenticating) {
            this->SetDisconnected(ConnectionError::InvalidPassword);
            this->listener->OnClientInvalidPassword(this);
        }
        else {
            this->SetDisconnected(ConnectionError::ClosedByServer);
        }
    });
}

WebSocketClient::~WebSocketClient() {
    this->Disconnect();
    /* need to ensure this is destroyed before the io_service, hence
    wrapping it in a unique_ptr and explicitly resetting it here. */
    this->client.reset();
}

WebSocketClient::ConnectionError WebSocketClient::LastConnectionError() const {
    std::unique_lock<decltype(this->mutex)> lock(this->mutex);
    return this->connectionError;
}

WebSocketClient::State WebSocketClient::ConnectionState() const {
    std::unique_lock<decltype(this->mutex)> lock(this->mutex);
    return this->state;
}

std::string WebSocketClient::Uri() const {
    std::unique_lock<decltype(this->mutex)> lock(this->mutex);
    return this->uri;
}

void WebSocketClient::SetDisconnected(ConnectionError errorCode) {
    std::unique_lock<decltype(this->mutex)> lock(this->mutex);
    this->connectionError = errorCode;
    this->SetState(State::Disconnected);
}

std::string WebSocketClient::EnqueueQuery(Query query) {
    std::unique_lock<decltype(this->mutex)> lock(this->mutex);
    auto messageId = generateMessageId();
    messageIdToQuery[messageId] = query;
    if (this->state == State::Connected) {
        this->client->send(
            this->connection,
            createSendRawQueryRequest(query->SerializeQuery(), messageId),
            websocketpp::frame::opcode::text);
    }
    return messageId;
}

void WebSocketClient::Connect(const std::string& host, short port, const std::string& password) {
    auto newUri = "ws://" + host + ":" + std::to_string(port);
    if (newUri != this->uri ||
        password != this->password ||
        this->state != State::Connected)
    {
        this->Disconnect();
        this->uri = newUri;
        this->password = password;
        if (this->uri.size()) {
            this->Reconnect();
        }
    }
}

void WebSocketClient::Reconnect() {
    std::unique_lock<decltype(this->mutex)> lock(this->mutex);

    this->Disconnect();

    io.restart();

    this->SetState(State::Connecting);
    this->thread = std::make_shared<std::thread>([&]() {
        std::string uri;

        {
            std::unique_lock<decltype(this->mutex)> lock(this->mutex);
            uri = this->uri;
        }

        if (uri.size()) {
            websocketpp::lib::error_code ec;
            Client::connection_ptr connection = client->get_connection(this->uri, ec);
            client->connect(connection);
            client->run();
        }

        this->SetState(State::Disconnected);
    });
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

void WebSocketClient::InvalidatePendingQueries() {
    std::unique_lock<decltype(this->mutex)> lock(this->mutex);

    for (auto& kv : this->messageIdToQuery) {
        this->listener->OnClientQueryFailed(
            this, kv.first, kv.second, QueryError::Disconnected);
    }

    this->messageIdToQuery.clear();
}

void WebSocketClient::SendPendingQueries() {
    std::unique_lock<decltype(this->mutex)> lock(this->mutex);

    for (auto& kv : this->messageIdToQuery) {
        auto messageId = kv.first;
        auto query = kv.second;
        this->client->send(
            this->connection,
            createSendRawQueryRequest(query->SerializeQuery(), messageId),
            websocketpp::frame::opcode::text);
    }
}

void WebSocketClient::SetState(State state) {
    std::unique_lock<decltype(this->mutex)> lock(this->mutex);
    if (state != this->state) {
        auto const oldState = this->state;

        switch (state) {
            case State::Disconnected:
                this->connection.reset();
                this->InvalidatePendingQueries();
                break;
            case State::Connected:
                this->connectionError = ConnectionError::None;
                this->SendPendingQueries();
                break;
            default:
                break;
        }

        this->state = state;
        this->listener->OnClientStateChanged(this, state, oldState);
    }
}
