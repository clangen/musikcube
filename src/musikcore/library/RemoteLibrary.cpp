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

#include <musikcore/library/RemoteLibrary.h>
#include <musikcore/config.h>
#include <musikcore/support/Common.h>
#include <musikcore/support/Preferences.h>
#include <musikcore/support/PreferenceKeys.h>
#include <musikcore/library/Indexer.h>
#include <musikcore/library/IQuery.h>
#include <musikcore/library/LibraryFactory.h>
#include <musikcore/library/QueryRegistry.h>
#include <musikcore/runtime/Message.h>
#include <musikcore/support/NarrowCast.h>
#include <musikcore/debug.h>

#include <chrono>

#pragma warning(push, 0)
#include <nlohmann/json.hpp>
#pragma warning(pop)

static const std::string TAG = "RemoteLibrary";

using namespace musik::core;
using namespace musik::core::db;
using namespace musik::core::library;
using namespace musik::core::net;
using namespace musik::core::runtime;
using namespace std::chrono;

#define MESSAGE_QUERY_COMPLETED 5000
#define MESSAGE_RECONNECT_SOCKET 5001
#define MESSAGE_UPDATE_CONNECTION_STATE 5002

class NullIndexer: public musik::core::IIndexer {
    public:
        void AddPath(const std::string& path) noexcept override { }
        void RemovePath(const std::string& path) noexcept override { }
        void GetPaths(std::vector<std::string>& paths) noexcept override { }
        void Schedule(SyncType type) noexcept override { }
        void Stop() noexcept override { }
        State GetState() noexcept override { return StateIdle; }
} kNullIndexer;

class RemoteLibrary::QueryCompletedMessage: public Message {
    public:
        using QueryContextPtr = RemoteLibrary::QueryContextPtr;

        QueryCompletedMessage(IMessageTarget* target, QueryContextPtr context)
        : Message(target, MESSAGE_QUERY_COMPLETED, 0, 0) {
            this->context = context;
        }

        QueryContextPtr GetContext() noexcept {
            return this->context;
        }

    private:
        QueryContextPtr context;
};

ILibraryPtr RemoteLibrary::Create(std::string name, int id, MessageQueue* messageQueue) {
    return std::make_shared<RemoteLibrary>(name, id, messageQueue);
}

RemoteLibrary::RemoteLibrary(std::string name, int id, MessageQueue* messageQueue)
: name(name)
, id(id)
, exit(false)
, messageQueue(messageQueue)
, wsc(messageQueue, this) {
    this->identifier = std::to_string(id);
    this->thread = std::make_unique<std::thread>(std::bind(&RemoteLibrary::ThreadProc, this));
    this->ReloadConnectionFromPreferences();
    if (this->messageQueue) {
        this->messageQueue->Register(this);
    }
}

RemoteLibrary::~RemoteLibrary() {
    this->Close();
    if (this->messageQueue) {
        this->messageQueue->Unregister(this);
    }
}

int RemoteLibrary::Id() {
    return this->id;
}

const std::string& RemoteLibrary::Name() {
    return this->name;
}

void RemoteLibrary::Close() {
    this->wsc.Disconnect();

    std::unique_ptr<std::thread> thread;

    {
        std::unique_lock<std::recursive_mutex> lock(this->queueMutex);
        if (this->thread) {
            thread = std::unique_ptr<std::thread>(std::move(this->thread));
            this->queryQueue.clear();
            this->exit = true;
        }
    }

    if (thread) {
        this->queueCondition.notify_all();
        this->syncQueryCondition.notify_all();
        thread->join();
    }
}

bool RemoteLibrary::IsConfigured() {
    auto prefs = Preferences::ForComponent(core::prefs::components::Settings);
    return prefs->GetBool(core::prefs::keys::RemoteLibraryViewed, false);
}

static inline bool isQueryDone(RemoteLibrary::Query query) {
    switch (query->GetStatus()) {
        case IQuery::Idle:
        case IQuery::Running:
            return false;
        default:
            return true;
    }
}

bool RemoteLibrary::IsQueryInFlight(Query query) {
    for (auto& kv : this->queriesInFlight) {
        if (query == kv.second->query) {
            return true;
        }
    }
    for (const auto& queryContext : this->queryQueue) {
        if (queryContext->query == query) {
            return true;
        }
    }
    return false;
}

int RemoteLibrary::Enqueue(QueryPtr query, Callback callback) {
    return this->EnqueueAndWait(query, 0LL, callback);
}

int RemoteLibrary::EnqueueAndWait(QueryPtr query, size_t timeoutMs, Callback callback) {
    if (QueryRegistry::IsLocalOnlyQuery(query->Name())) {
        auto defaultLocalLibrary = LibraryFactory::Instance().DefaultLocalLibrary();
        return defaultLocalLibrary->EnqueueAndWait(query, timeoutMs, callback);
    }

    auto serializableQuery = std::dynamic_pointer_cast<ISerializableQuery>(query);

    if (serializableQuery) {
        std::unique_lock<std::recursive_mutex> lock(this->queueMutex);

        if (this->exit) {
            return -1;
        }

        auto context = std::make_shared<QueryContext>();
        context->query = serializableQuery;
        context->callback = callback;

        queryQueue.push_back(context);
        queueCondition.notify_all();

        if (timeoutMs > 0) {
            while (
                !this->exit &&
                this->IsQueryInFlight(context->query) &&
                !isQueryDone(context->query))
            {
                if (timeoutMs == kWaitIndefinite) {
                    this->syncQueryCondition.wait(lock);
                    break;
                }
                else {
                    const auto result = this->syncQueryCondition.wait_for(lock, timeoutMs * milliseconds(1));
                    if (result == std::cv_status::timeout) {
                        break;
                    }
                }
            }
        }

        return query->GetId();
    }

    return -1;
}

RemoteLibrary::QueryContextPtr RemoteLibrary::GetNextQuery() {
    std::unique_lock<std::recursive_mutex> lock(this->queueMutex);
    while (this->queryQueue.empty() && !this->exit) {
        this->queueCondition.wait(lock);
    }
    if (this->exit) {
        return QueryContextPtr();
    }
    else {
        auto front = queryQueue.front();
        queryQueue.pop_front();
        return front;
    }
}

void RemoteLibrary::ThreadProc() {
    while (!this->exit) {
        auto query = GetNextQuery();
        if (query) {
            this->RunQuery(query);
        }
    }
}

void RemoteLibrary::NotifyQueryCompleted(QueryContextPtr context) {
    this->QueryCompleted(context->query.get());
    if (context->callback) {
        context->callback(context->query);
    }
}

void RemoteLibrary::OnQueryCompleted(QueryContextPtr context) {
    if (context) {
        if (this->messageQueue) {
            this->messageQueue->Post(std::make_shared<QueryCompletedMessage>(this, context));
        }
        else {
            this->NotifyQueryCompleted(context);
        }
    }
}

void RemoteLibrary::OnQueryCompleted(const std::string& messageId, Query query) {
    QueryContextPtr context;

    {
        std::unique_lock<std::recursive_mutex> lock(this->queueMutex);
        context = queriesInFlight[messageId];
        queriesInFlight.erase(messageId);
    }

    if (context) {
        this->OnQueryCompleted(context);
    }

    this->syncQueryCondition.notify_all();
}

void RemoteLibrary::RunQuery(QueryContextPtr context) {
    std::unique_lock<std::recursive_mutex> lock(this->queueMutex);
#if 0
    this->RunQueryOnLoopback(context);
#else
    this->RunQueryOnWebSocketClient(context);
#endif
}

void RemoteLibrary::RunQueryOnLoopback(QueryContextPtr context) {
    if (context) {
        /* do everything via loopback to the local library for testing. we do, however,
        go through the motions by serializing the inbound query to a string, then
        bouncing through the QueryRegistry to create a new instance, run the query
        locally, serialize the result, then deserialize it again to emulate the entire
        round trip. */

        auto localLibrary = LibraryFactory::Instance().DefaultLocalLibrary();
        localLibrary->SetMessageQueue(*this->messageQueue);

        auto localQuery = QueryRegistry::CreateLocalQueryFor(
            context->query->Name(), context->query->SerializeQuery(), localLibrary);

        if (!localQuery) {
            OnQueryCompleted(context);
            return;
        }

        localLibrary->EnqueueAndWait(
            localQuery,
            kWaitIndefinite,
            [this, context, localQuery](auto result) {
            if (localQuery->GetStatus() == IQuery::Finished) {
                context->query->DeserializeResult(localQuery->SerializeResult());
            }
            this->OnQueryCompleted(context);
        });
    }
}

void RemoteLibrary::RunQueryOnWebSocketClient(QueryContextPtr context) {
    if (context->query) {
        const std::string messageId = wsc.EnqueueQuery(context->query);
        if (!messageId.empty()) {
            queriesInFlight[messageId] = context;
        }
        else {
            context->query->Invalidate();
            this->OnQueryCompleted(context);
            this->syncQueryCondition.notify_all();
        }
    }
}

void RemoteLibrary::SetMessageQueue(musik::core::runtime::IMessageQueue& queue) {
    if (this->messageQueue && this->messageQueue != &queue) {
        this->messageQueue->Unregister(this);
    }
    this->messageQueue = &queue;
    this->messageQueue->Register(this);
    this->wsc.SetMessageQueue(messageQueue);
}

musik::core::IIndexer* RemoteLibrary::Indexer() {
    return &kNullIndexer;
}

/* IMessageTarget */

void RemoteLibrary::ProcessMessage(musik::core::runtime::IMessage &message) {
    if (message.Type() == MESSAGE_QUERY_COMPLETED) {
        auto context = dynamic_cast<QueryCompletedMessage*>(&message)->GetContext();
        if (context) {
            this->NotifyQueryCompleted(context);
        }
    }
    else if (message.Type() == MESSAGE_RECONNECT_SOCKET) {
        if (this->wsc.ConnectionState() == Client::State::Disconnected) {
            this->ReloadConnectionFromPreferences();
        }
    }
    else if (message.Type() == MESSAGE_UPDATE_CONNECTION_STATE) {
        const auto updatedState = static_cast<ConnectionState>(message.UserData1());
        this->connectionState = updatedState;
        this->ConnectionStateChanged(this->connectionState);
    }
}

/* WebSocketClient::Listener */

void RemoteLibrary::OnClientInvalidPassword(Client* client) {
    this->messageQueue->Post(Message::Create(this, MESSAGE_UPDATE_CONNECTION_STATE, (int) ConnectionState::AuthenticationFailure));
}

void RemoteLibrary::OnClientStateChanged(Client* client, State newState, State oldState) {
    static std::map<State, ConnectionState> kConnectionStateMap = {
        { State::Disconnected, ConnectionState::Disconnected },
        { State::Disconnecting, ConnectionState::Disconnected },
        { State::Connecting, ConnectionState::Connecting },
        { State::Connected, ConnectionState::Connected },
    };

    if (this->messageQueue) {
        const auto reason = this->wsc.LastConnectionError();
        const bool attemptReconnect =
            newState == State::Disconnected &&
            reason != WebSocketClient::ConnectionError::InvalidPassword &&
            reason != WebSocketClient::ConnectionError::IncompatibleVersion;
        if (attemptReconnect) {
            this->messageQueue->Remove(this, MESSAGE_RECONNECT_SOCKET);
            this->messageQueue->Post(Message::Create(this, MESSAGE_RECONNECT_SOCKET), 2500);
        }
        this->messageQueue->Post(Message::Create(
            this,
            MESSAGE_UPDATE_CONNECTION_STATE,
            (int) kConnectionStateMap[newState]));
    }
}

void RemoteLibrary::OnClientQuerySucceeded(Client* client, const std::string& messageId, Query query) {
    this->OnQueryCompleted(messageId, query);
}

void RemoteLibrary::OnClientQueryFailed(Client* client, const std::string& messageId, Query query, Client::QueryError result) {
    this->OnQueryCompleted(messageId, query);
}

/* RemoteLibrary::RemoteResourceLocator */

std::string RemoteLibrary::GetTrackUri(musik::core::sdk::ITrack* track, const std::string& defaultUri) {
    std::string type = ".mp3";

    char buffer[4096];
    buffer[0] = 0;
    const int size = track->Uri(buffer, sizeof(buffer));
    if (size) {
        std::string originalUri = buffer;
        const std::string::size_type lastDot = originalUri.find_last_of(".");
        if (lastDot != std::string::npos) {
            type = originalUri.substr(lastDot);
        }
    }

    auto prefs = Preferences::ForComponent(core::prefs::components::Settings);
    auto host = prefs->GetString(core::prefs::keys::RemoteLibraryHostname, "127.0.0.1");
    const auto port = narrow_cast<unsigned short>(prefs->GetInt(core::prefs::keys::RemoteLibraryHttpPort, 7905));
    auto password = prefs->GetString(core::prefs::keys::RemoteLibraryPassword, "");
    const auto useTls = prefs->GetBool(core::prefs::keys::RemoteLibraryHttpTls, false);

    const std::string scheme = useTls ? "https://" : "http://";

    std::string uri = scheme + host + ":" + std::to_string(port) + "/audio/id/" + std::to_string(track->GetId());

    if (prefs->GetBool(core::prefs::keys::RemoteLibraryTranscoderEnabled)) {
        auto const bitrate = prefs->GetInt(core::prefs::keys::RemoteLibraryTranscoderBitrate);
        auto const format = prefs->GetString(core::prefs::keys::RemoteLibraryTranscoderFormat);
        if (bitrate > 0 && bitrate < 9999 && !format.empty()) {
            uri += "?bitrate=" + std::to_string(bitrate) + "&format=" + format;
            type = "." + format;
        }
    }

    nlohmann::json path = {
        { "uri", uri },
        { "originalUri", std::string(buffer) },
        { "type", type },
        { "password", password }
    };

    return "musikcore://remote-track/" + path.dump();
}

/* RemoteLibrary */

const net::WebSocketClient& RemoteLibrary::WebSocketClient() const {
    return this->wsc;
}

void RemoteLibrary::ReloadConnectionFromPreferences() {
    auto prefs = Preferences::ForComponent(core::prefs::components::Settings);
    auto host = prefs->GetString(core::prefs::keys::RemoteLibraryHostname, "127.0.0.1");
    const auto port = narrow_cast<unsigned short>(prefs->GetInt(core::prefs::keys::RemoteLibraryWssPort, 7905));
    auto password = prefs->GetString(core::prefs::keys::RemoteLibraryPassword, "");
    const auto useTls = prefs->GetBool(core::prefs::keys::RemoteLibraryWssTls, false);
    this->wsc.Connect(host, port, password, useTls);
}