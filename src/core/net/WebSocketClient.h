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

#pragma once

#include <core/config.h>
#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/client.hpp>

namespace musik { namespace core { namespace net {

    class WebSocketClient {
        public:
            using Client = websocketpp::client<websocketpp::config::asio_client>;
            using Message = websocketpp::config::asio_client::message_type::ptr;
            using Connection = websocketpp::connection_hdl;

            enum class State: int {
                Disconnected = 0,
                Connecting = 1,
                Connected = 2
            };

            enum class ErrorCode: int {
                QueryFailed = 1,
                Disconnected = 2
            };

            class Listener {
                public:
                    using Client = WebSocketClient;
                    using State = Client::State;
                    using ErrorCode = Client::ErrorCode;
                    virtual void OnClientInvalidPassword(Client* client) = 0;
                    virtual void OnClientStateChanged(
                        Client* client, State newState, State oldState) = 0;
                    virtual void OnClientQuerySucceeded(
                        Client* client, int64_t queryId, const std::string& result) = 0;
                    virtual void OnClientQueryFailed(
                        Client* client, int64_t queryId, ErrorCode result) = 0;
            };

            WebSocketClient();
            WebSocketClient(const WebSocketClient&) = delete;
            virtual ~WebSocketClient();

            void Connect(
                const std::string& hostname,
                const short port,
                const std::string& password);

            int64_t EnqueueQuery(const std::string& data);

        private:
            Client client;
    };

} } }

