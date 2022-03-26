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

#include <musikcore/net/PiggyWebSocketClient.h>
#include <musikcore/support/Common.h>
#include <musikcore/support/PreferenceKeys.h>
#include <musikcore/support/Preferences.h>
#include <musikcore/runtime/Message.h>
#include <musikcore/version.h>

using namespace musik::core;
using namespace musik::core::net;
using namespace musik::core::runtime;

using ClientPtr = PiggyWebSocketClient::ClientPtr;
using ClientMessage = PiggyWebSocketClient::ClientMessage;
using Connection = PiggyWebSocketClient::Connection;
using Message = PiggyWebSocketClient::Message;

static const bool kDisableOfflineQueue = true;
static std::atomic<int> nextMessageId(0);

static inline std::string generateMessageId() {
    return "integrated-websocket-client-" + std::to_string(nextMessageId.fetch_add(1));
}

// static inline std::string createPingRequest() {
//     const nlohmann::json authRequestJson = {
//         { "name", "ping" },
//         { "type" , "request" },
//         { "id", generateMessageId() },
//         { "device_id", "remote-random-device" },
//         { "options", nlohmann::json() }
//     };
//     return authRequestJson.dump();
// }

// static inline bool extractRawQueryResult(
//     nlohmann::json& responseJson, std::string& rawResult)
// {
//     if (responseJson["name"].get<std::string>() != "send_raw_query") {
//         return false;
//     }
//     rawResult = responseJson["options"]["raw_query_data"].get<std::string>();
//     return true;
// }

PiggyWebSocketClient::PiggyWebSocketClient(IMessageQueue* messageQueue, Listener* listener)
: messageQueue(nullptr) {
    this->SetMessageQueue(messageQueue);

    rawClient = std::make_unique<RawWebSocketClient>(io);

    this->listener = listener;

    rawClient->SetMode(RawWebSocketClient::Mode::TLS);

    rawClient->SetOpenHandler([this](Connection connection) {
        this->SetState(State::Authenticating);
        /* initialize session */
    });

    rawClient->SetFailHandler([this](Connection connection) {
        this->SetDisconnected(ConnectionError::ConnectionFailed);
    });

    rawClient->SetMessageHandler([this](Connection connection, ClientMessage message) {
        nlohmann::json responseJson = nlohmann::json::parse(message->get_payload());
        /* process response message */
    });

    rawClient->SetCloseHandler([this](Connection connection) {
        this->SetDisconnected(ConnectionError::ClosedByServer);
    });
}

PiggyWebSocketClient::~PiggyWebSocketClient() {
    this->Disconnect();
    /* need to ensure this is destroyed before the io_service, hence
    wrapping it in a unique_ptr and explicitly resetting it here. */
    this->rawClient.reset();
    if (this->messageQueue) {
        this->messageQueue->Unregister(this);
    }
}

PiggyWebSocketClient::ConnectionError PiggyWebSocketClient::LastConnectionError() const {
    std::unique_lock<decltype(this->mutex)> lock(this->mutex);
    return this->connectionError;
}

PiggyWebSocketClient::State PiggyWebSocketClient::ConnectionState() const {
    std::unique_lock<decltype(this->mutex)> lock(this->mutex);
    return this->state;
}

std::string PiggyWebSocketClient::Uri() const {
    std::unique_lock<decltype(this->mutex)> lock(this->mutex);
    return this->uri;
}

void PiggyWebSocketClient::SetDisconnected(ConnectionError errorCode) {
    std::unique_lock<decltype(this->mutex)> lock(this->mutex);
    this->connectionError = errorCode;
    this->SetState(State::Disconnected);
}

std::string PiggyWebSocketClient::EnqueueMessage(Message message) {
    std::unique_lock<decltype(this->mutex)> lock(this->mutex);
    if (kDisableOfflineQueue && this->state != State::Connected) {
        return "";
    }
    if (!message) {
        return "";
    }
    auto messageId = generateMessageId();
    messageIdToMessage[messageId] = message;
    if (this->state == State::Connected) {
        this->rawClient->Send(this->connection, message->dump());
    }
    return messageId;
}

void PiggyWebSocketClient::Connect(
    const std::string& host,
    unsigned short port,
    const std::string& password,
    bool useTls)
{
    auto newUri = "ws://" + host + ":" + std::to_string(port);
    if (newUri != this->uri ||
        useTls != this->useTls ||
        this->state != State::Connected)
    {
        this->Disconnect();
        this->useTls = useTls;
        this->uri = newUri;
        if (this->uri.size()) {
            this->Reconnect();
        }
    }
}

void PiggyWebSocketClient::Reconnect() {
    std::unique_lock<decltype(this->mutex)> lock(this->mutex);

    this->Disconnect();

#if BOOST_VERSION < 106600
    io.reset();
#else
    io.restart();
#endif

    auto const prefs = Preferences::ForComponent(core::prefs::components::Settings);
    auto const timeout = prefs->GetInt(core::prefs::keys::RemoteLibraryLatencyTimeoutMs, 5000);

    this->SetState(State::Connecting);

    this->thread = std::make_unique<std::thread>([&, timeout]() {
        std::string uri;

        {
            std::unique_lock<decltype(this->mutex)> lock(this->mutex);
            uri = this->uri;
        }

        if (uri.size()) {
            const auto mode = this->useTls
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

void PiggyWebSocketClient::Disconnect() {
    std::unique_ptr<std::thread> oldThread;

    {
        std::unique_lock<decltype(this->mutex)> lock(this->mutex);
        oldThread = std::unique_ptr<std::thread>(std::move(this->thread));
    }

    if (oldThread) {
        io.stop();
        oldThread->join();
    }
}

void PiggyWebSocketClient::InvalidatePendingMessages() {
    std::unique_lock<decltype(this->mutex)> lock(this->mutex);

    // for (auto& kv : this->messageIdToMessage) {
    //     this->listener->OnClientQueryFailed(
    //         this, kv.first, kv.second, QueryError::Disconnected);
    // }

    this->messageIdToMessage.clear();
}

void PiggyWebSocketClient::SendPendingMessages() {
    std::unique_lock<decltype(this->mutex)> lock(this->mutex);

    // for (auto& kv : this->messageIdToMessage) {
    //     auto messageId = kv.first;
    //     auto query = kv.second;
    //     if (query) {
    //         this->rawClient->Send(
    //             this->connection,
    //             createSendRawQueryRequest(query->SerializeQuery(), messageId));
    //     }
    // }
}

void PiggyWebSocketClient::SetState(State state) {
    std::unique_lock<decltype(this->mutex)> lock(this->mutex);
    if (state != this->state) {
        auto const oldState = this->state;

        switch (state) {
            case State::Disconnected:
                this->connection.reset();
                this->InvalidatePendingMessages();
                break;
            case State::Connected:
                this->connectionError = ConnectionError::None;
                this->SendPendingMessages();
                break;
            default:
                break;
        }

        this->state = state;
        this->listener->OnClientStateChanged(this, state, oldState);
    }
}

void PiggyWebSocketClient::SetMessageQueue(IMessageQueue* messageQueue) {
    if (messageQueue == this->messageQueue) {
        return;
    }
    if (this->messageQueue) {
        this->messageQueue->Unregister(this);
    }
    this->messageQueue = messageQueue;
    if (this->messageQueue) {
        this->messageQueue->Register(this);
        // this->messageQueue->Post(Message::Create(this, kPingMessage), kPingIntervalMs);
    }
}

/* IMessageTarget */
void PiggyWebSocketClient::ProcessMessage(IMessage& message) {
    // if (message.Type() == kPingMessage) {
    //     std::unique_lock<decltype(this->mutex)> lock(this->mutex);
    //     if (this->state == State::Connected) {
    //         this->rawClient->Send(this->connection, createPingRequest());
    //     }
    //     this->messageQueue->Post(Message::Create(this, kPingMessage), kPingIntervalMs);
    // }
}
