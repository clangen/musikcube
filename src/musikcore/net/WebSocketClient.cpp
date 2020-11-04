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
#include <musikcore/support/PreferenceKeys.h>
#include <musikcore/support/Preferences.h>
#include <musikcore/runtime/Message.h>
#include <musikcore/version.h>
#include <nlohmann/json.hpp>

using namespace musik::core;
using namespace musik::core::net;
using namespace musik::core::runtime;

using ClientPtr = WebSocketClient::ClientPtr;
using ClientMessage = WebSocketClient::ClientMessage;
using Connection = WebSocketClient::Connection;

static const int64_t kPingIntervalMs = 3500;
static const int kPingMessage = 6000;

static const bool kDisableOfflineQueue = true;

static std::atomic<int> nextMessageId(0);

static inline std::string generateMessageId() {
    return "integrated-websocket-client-" + std::to_string(nextMessageId.fetch_add(1));
}

static inline std::string createPingRequest() {
    const nlohmann::json authRequestJson = {
        { "name", "ping" },
        { "type" , "request" },
        { "id", generateMessageId() },
        { "device_id", "remote-random-device" },
        { "options", nlohmann::json() }
    };
    return authRequestJson.dump();
}

static inline std::string createAuthenticateRequest(const std::string& password) {
    const nlohmann::json authRequestJson = {
        { "name", "authenticate" },
        { "type" , "request" },
        { "id", generateMessageId() },
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

WebSocketClient::WebSocketClient(IMessageQueue* messageQueue, Listener* listener)
: messageQueue(nullptr) {
    this->SetMessageQueue(messageQueue);

    rawClient = std::make_unique<RawWebSocketClient>(io);

    this->listener = listener;

    rawClient->SetMode(RawWebSocketClient::Mode::TLS);

    rawClient->SetOpenHandler([this](Connection connection) {
        this->SetState(State::Authenticating);
        this->rawClient->Send(
            connection, createAuthenticateRequest(this->password));
    });

    rawClient->SetFailHandler([this](Connection connection) {
        this->SetDisconnected(ConnectionError::ConnectionFailed);
    });

    rawClient->SetMessageHandler([this](Connection connection, ClientMessage message) {
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

    rawClient->SetCloseHandler([this](Connection connection) {
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
    this->rawClient.reset();
    if (this->messageQueue) {
        this->messageQueue->Unregister(this);
    }
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
    if (kDisableOfflineQueue && this->state != State::Connected) {
        query->Invalidate(); /* mark it as failed */
        return "";
    }
    if (!query) {
        return "";
    }
    auto messageId = generateMessageId();
    messageIdToQuery[messageId] = query;
    if (this->state == State::Connected) {
        this->rawClient->Send(
            this->connection,
            createSendRawQueryRequest(query->SerializeQuery(), messageId));
    }
    return messageId;
}

void WebSocketClient::Connect(
    const std::string& host,
    short port,
    const std::string& password,
    bool useTls)
{
    auto newUri = "ws://" + host + ":" + std::to_string(port);
    if (newUri != this->uri ||
        password != this->password ||
        useTls != this->useTls ||
        this->state != State::Connected)
    {
        this->Disconnect();
        this->useTls = useTls;
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

#if BOOST_VERSION < 106600
    io.reset();
#else
    io.restart();
#endif

    auto prefs = Preferences::ForComponent(core::prefs::components::Settings);
    auto timeout = prefs->GetInt(core::prefs::keys::RemoteLibraryLatencyTimeoutMs, 5000);

    this->SetState(State::Connecting);
    this->thread = std::make_shared<std::thread>([&, timeout]() {
        std::string uri;

        {
            std::unique_lock<decltype(this->mutex)> lock(this->mutex);
            uri = this->uri;
        }

        if (uri.size()) {
            auto mode = this->useTls
                ? RawWebSocketClient::Mode::TLS
                : RawWebSocketClient::Mode::PlainText;
            rawClient->SetMode(mode);
            rawClient->SetPongTimeout(timeout);
            rawClient->Connect(uri);
            rawClient->Run();
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
        if (query) {
            this->rawClient->Send(
                this->connection,
                createSendRawQueryRequest(query->SerializeQuery(), messageId));
        }
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

void WebSocketClient::SetMessageQueue(IMessageQueue* messageQueue) {
    if (messageQueue == this->messageQueue) {
        return;
    }
    if (this->messageQueue) {
        this->messageQueue->Unregister(this);
    }
    this->messageQueue = messageQueue;
    if (this->messageQueue) {
        this->messageQueue->Register(this);
        this->messageQueue->Post(Message::Create(this, kPingMessage), kPingIntervalMs);
    }
}

/* IMessageTarget */
void WebSocketClient::ProcessMessage(IMessage& message) {
    if (message.Type() == kPingMessage) {
        std::unique_lock<decltype(this->mutex)> lock(this->mutex);
        if (this->state == State::Connected) {
            this->rawClient->Send(this->connection, createPingRequest());
        }
        this->messageQueue->Post(Message::Create(this, kPingMessage), kPingIntervalMs);
    }
}
