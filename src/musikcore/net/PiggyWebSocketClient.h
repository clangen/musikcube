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

#pragma once

#include <musikcore/config.h>
#include <musikcore/net/RawWebSocketClient.h>
#include <musikcore/runtime/IMessageQueue.h>

#include <thread>
#include <unordered_map>
#include <atomic>
#include <memory>

#pragma warning(push, 0)
#include <nlohmann/json.hpp>
#pragma warning(pop)

namespace musik { namespace core { namespace net {

    class PiggyWebSocketClient: public musik::core::runtime::IMessageTarget {
        public:
            using ClientPtr = std::unique_ptr<RawWebSocketClient>;
            using ClientMessage = websocketpp::config::asio_client::message_type::ptr;
            using Connection = websocketpp::connection_hdl;
            using Message = std::shared_ptr<nlohmann::json>;

            enum class State: int {
                Disconnected = 0,
                Connecting = 1,
                Authenticating = 2,
                Connected = 3,
                Disconnecting = 4,
            };

            enum class ConnectionError : int {
                None = 0,
                InvalidPassword = 1,
                IncompatibleVersion = 2,
                ConnectionFailed = 3,
                ClosedByServer = 4,
            };

            class Listener {
                public:
                    using Client = PiggyWebSocketClient;
                    using State = Client::State;
                    virtual void OnClientStateChanged(Client* client, State newState, State oldState) = 0;
            };

            PiggyWebSocketClient(
                musik::core::runtime::IMessageQueue* messageQueue,
                Listener* listener);

            PiggyWebSocketClient(const PiggyWebSocketClient&) = delete;
            virtual ~PiggyWebSocketClient();

            void Connect(
                const std::string& host,
                unsigned short port,
                const std::string& password,
                bool useTls);

            void Reconnect();
            void Disconnect();

            std::string EnqueueMessage(Message message);

            State ConnectionState() const;
            ConnectionError LastConnectionError() const;
            std::string Uri() const;

            void SetMessageQueue(musik::core::runtime::IMessageQueue* messageQueue);
            void ProcessMessage(musik::core::runtime::IMessage& message) override;

        private:
            void SetState(State state);
            void SetDisconnected(ConnectionError errorCode);
            void InvalidatePendingMessages();
            void SendPendingMessages();

            ClientPtr rawClient;
            Connection connection;
            boost::asio::io_service io;
            std::unique_ptr<std::thread> thread;
            mutable std::recursive_mutex mutex;
            bool useTls{ false };
            std::string uri;
            std::unordered_map<std::string, Message> messageIdToMessage;
            std::atomic<bool> quit{ false };
            ConnectionError connectionError{ ConnectionError::None };
            State state{ State::Disconnected };
            Listener* listener{ nullptr };
            musik::core::runtime::IMessageQueue* messageQueue;
    };

} } }

