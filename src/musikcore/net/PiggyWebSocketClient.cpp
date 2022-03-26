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
#include <musikcore/runtime/Message.h>
#include <musikcore/version.h>

using namespace musik::core;
using namespace musik::core::net;
using namespace musik::core::runtime;

using ClientPtr = PiggyWebSocketClient::ClientPtr;
using ClientMessage = PiggyWebSocketClient::ClientMessage;
using Connection = PiggyWebSocketClient::Connection;
using Message = PiggyWebSocketClient::Message;

static const int64_t kLatencyTimeoutMs = 30000;
static const int64_t kPingIntervalMs = 10000;
static const int kPingMessage = 6000;
static const bool kDisableOfflineQueue = true;
static std::atomic<int> nextMessageId(0);

static inline std::string generateSessionId() {
    return "musikcube-" + std::to_string(nextMessageId.fetch_add(1));
}

static inline std::string createPingJson(const std::string& sessionId) {
    const nlohmann::json authRequestJson = {
        { "name", "ping" },
        { "sessionId", sessionId },
        { "data", nlohmann::json() }
    };
    return authRequestJson.dump();
}

PiggyWebSocketClient::PiggyWebSocketClient(IMessageQueue* messageQueue)
: messageQueue(nullptr)
, sessionId(generateSessionId()) {
    this->SetMessageQueue(messageQueue);

    rawClient = std::make_unique<RawWebSocketClient>(io);

    rawClient->SetMode(RawWebSocketClient::Mode::PlainText);

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

void PiggyWebSocketClient::EnqueueMessage(Message message) {
    std::unique_lock<decltype(this->mutex)> lock(this->mutex);
    if (!message) {
        return;
    }
    (*message)["sessionId"] = this->sessionId;
    if (this->state != State::Connected) {
        if (!kDisableOfflineQueue) {
            this->pendingMessages.push_back(message);
        }
        return;
    }
    if (this->state == State::Connected) {
        this->rawClient->Send(this->connection, message->dump());
    }
}

void PiggyWebSocketClient::Connect(const std::string& host, unsigned short port, bool useTls) {
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

    this->SetState(State::Connecting);

    this->thread = std::make_unique<std::thread>([&]() {
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
            rawClient->SetPongTimeout(kLatencyTimeoutMs);
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
    this->pendingMessages.clear();
}

void PiggyWebSocketClient::SendPendingMessages() {
    std::unique_lock<decltype(this->mutex)> lock(this->mutex);

    for (auto& message : this->pendingMessages) {
        this->rawClient->Send(this->connection, message->dump());
    }

    this->pendingMessages.clear();
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
        this->StateChanged(this, state, oldState);
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
        this->messageQueue->Post(runtime::Message::Create(this, kPingMessage), kPingIntervalMs);
    }
}

/* IMessageTarget */
void PiggyWebSocketClient::ProcessMessage(IMessage& message) {
    if (message.Type() == kPingMessage) {
        std::unique_lock<decltype(this->mutex)> lock(this->mutex);
        if (this->state == State::Connected) {
            this->rawClient->Send(this->connection, createPingJson(this->sessionId));
        }
        this->messageQueue->Post(runtime::Message::Create(this, kPingMessage), kPingIntervalMs);
    }
}
