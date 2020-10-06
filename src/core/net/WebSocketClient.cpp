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

using namespace musik::core::net;

using Client = WebSocketClient::Client;
using Message = WebSocketClient::Message;
using Connection = WebSocketClient::Connection;
using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;

void on_open(Client* c, Connection hdl) {
    std::string msg = "Hello";
    c->send(hdl, msg, websocketpp::frame::opcode::text);
    c->get_alog().write(websocketpp::log::alevel::app, "Sent Message: " + msg);
}

void on_fail(Client* c, Connection hdl) {
    c->get_alog().write(websocketpp::log::alevel::app, "Connection Failed");
}

void on_message(Client* c, Connection hdl, Message msg) {
    c->get_alog().write(websocketpp::log::alevel::app, "Received Reply: " + msg->get_payload());
    c->close(hdl, websocketpp::close::status::normal, "");
}

void on_close(Client* c, Connection hdl) {
    c->get_alog().write(websocketpp::log::alevel::app, "Connection Closed");
}

WebSocketClient::WebSocketClient() {
    //new std::thread([]() {
    //    Client client;
    //    client.set_access_channels(websocketpp::log::alevel::all);
    //    client.clear_access_channels(websocketpp::log::alevel::frame_payload);
    //    client.init_asio();
    //    websocketpp::lib::error_code ec;
    //    Client::connection_ptr connection = client.get_connection("ws://192.168.1.221:7905", ec);
    //    client.set_open_handler(bind(&on_open, &client, ::_1));
    //    client.set_fail_handler(bind(&on_fail, &client, ::_1));
    //    client.set_message_handler(bind(&on_message, &client, ::_1, ::_2));
    //    client.set_close_handler(bind(&on_close, &client, ::_1));
    //    client.connect(connection);
    //    client.run();
    //});
}

WebSocketClient::~WebSocketClient() {
}