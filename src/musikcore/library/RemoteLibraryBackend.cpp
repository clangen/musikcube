//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004-2023 musikcube team
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

#include <musikcore/library/track/Track.h>
#include <musikcore/library/RemoteLibraryBackend.h>
#include <musikcore/config.h>
#include <musikcore/support/Common.h>
#include <musikcore/library/Indexer.h>
#include <musikcore/library/IQuery.h>
#include <musikcore/library/LibraryBackends.h>
#include <musikcore/library/QueryRegistry.h>
#include <common/runtime/Message.h>
#include <common/NarrowCast.h>
#include <musikcore/debug.h>
#include <common/HttpClient.h>
#include <websocketpp/base64/base64.hpp>

#include <chrono>
#include <charconv>
#include <cctype>
#include <optional>

#ifdef _MSC_VER
#pragma warning(push, 0)
#endif
#include <nlohmann/json.hpp>
#ifdef _MSC_VER
#pragma warning(pop)
#endif


using namespace musik::core;
using namespace musik::core::db;
using namespace musik::core::library;
using namespace musik::core::net;
using namespace musik::core::runtime;
using namespace std::chrono;

#define MESSAGE_QUERY_COMPLETED 5000
#define MESSAGE_RECONNECT_SOCKET 5001
#define MESSAGE_UPDATE_CONNECTION_STATE 5002

class RemoteLibraryBackend::QueryCompletedMessage: public Message {
    public:
        using QueryContextPtr = RemoteLibraryBackend::QueryContextPtr;

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

RemoteLibraryBackend::RemoteLibraryBackend(std::string name, int id, MessageQueue* messageQueue,
    OptionsProvider options, bool reconnect, uint64_t maxPendingQueries)
: options(std::move(options))
, reconnect(reconnect)
, maxPendingQueries(maxPendingQueries)
, messageQueue(messageQueue)
, wsc(messageQueue, this)
, id(id)
, name(std::move(name))
, exit(false)
{
    this->identifier = std::to_string(id);
    this->thread = std::make_unique<std::thread>(std::bind(&RemoteLibraryBackend::ThreadProc, this));
    if (this->messageQueue) {
        this->messageQueue->Register(this);
    }
}

RemoteLibraryBackend::~RemoteLibraryBackend() {
    this->Close();
    if (this->messageQueue) {
        this->messageQueue->Unregister(this);
    }
}

int RemoteLibraryBackend::Id() {
    return this->id;
}

const std::string& RemoteLibraryBackend::Name() {
    return this->name;
}

void RemoteLibraryBackend::Close() {
    this->Disconnect();

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

bool RemoteLibraryBackend::IsConfigured() {
    return this->options().configured;
}

static inline bool isQueryDone(RemoteLibraryBackend::Query query) {
    switch (query->GetStatus()) {
        case IQuery::Idle:
        case IQuery::Running:
            return false;
        default:
            return true;
    }
}

bool RemoteLibraryBackend::IsQueryInFlight(Query query) {
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

int RemoteLibraryBackend::Enqueue(QueryPtr query, Callback callback) {
    return this->EnqueueAndWait(query, 0LL, callback);
}

RemoteLibraryBackend::ConnectionQueryStatus RemoteLibraryBackend::EnqueueForConnection(
    Query query, uint64_t generation, Callback callback) {
    std::unique_lock<std::recursive_mutex> lock(this->queueMutex);
    if (this->exit || !generation || generation != this->wsc.ConnectionGeneration()) {
        return ConnectionQueryStatus::Unavailable;
    }

    /* ABI cancellation can finish before the server replies. Keep transport admission bounded. */

    const auto pending = std::count_if(this->queriesInFlight.begin(), this->queriesInFlight.end(),
        [](const auto& entry) { return !entry.second->notify; });
    if (static_cast<uint64_t>(pending) >= this->maxPendingQueries) {
        return ConnectionQueryStatus::Busy;
    }

    auto context = std::make_shared<QueryContext>();
    context->query = std::move(query);
    context->callback = std::move(callback);
    context->notify = false;
    const auto messageId = this->wsc.EnqueueQuery(context->query, generation);
    if (messageId.empty()) return ConnectionQueryStatus::Unavailable;
    this->queriesInFlight.emplace(messageId, std::move(context));
    return ConnectionQueryStatus::Accepted;
}

void RemoteLibraryBackend::DiscardConnectionQuery(const Query& query) {
    std::lock_guard lock(this->queueMutex);
    for (auto it = this->queriesInFlight.begin(); it != this->queriesInFlight.end(); ++it) {
        if (!it->second->notify && it->second->query == query) {
            /* a late network reply is ignored and no longer consumes a slot. */
            this->wsc.DiscardQuery(it->first);
            this->queriesInFlight.erase(it);
            return;
        }
    }
}

int RemoteLibraryBackend::EnqueueAndWait(QueryPtr query, size_t timeoutMs, Callback callback) {
    if (QueryRegistry::IsLocalOnlyQuery(query->Name())) {
        auto defaultLocalLibrary = musik::core::library::DefaultLocalBackend();
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

RemoteLibraryBackend::QueryContextPtr RemoteLibraryBackend::GetNextQuery() {
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

void RemoteLibraryBackend::ThreadProc() {
    while (!this->exit) {
        auto query = GetNextQuery();
        if (query) {
            this->RunQuery(query);
        }
    }
}

void RemoteLibraryBackend::NotifyQueryCompleted(QueryContextPtr context) {
    this->QueryCompleted.Publish(context->query.get());
    if (context->callback) {
        context->callback(context->query);
    }
}

void RemoteLibraryBackend::OnQueryCompleted(QueryContextPtr context) {
    if (context) {
        if (!context->notify) {
            /* ABI requests deliver through their own dispatcher. */
            context->callback(context->query);
        }
        else if (this->messageQueue) {
            this->messageQueue->Post(std::make_shared<QueryCompletedMessage>(this, context));
        }
        else {
            this->NotifyQueryCompleted(context);
        }
    }
}

void RemoteLibraryBackend::OnQueryCompleted(const std::string& messageId, Query query) {
    QueryContextPtr context;

    {
        std::unique_lock<std::recursive_mutex> lock(this->queueMutex);
        const auto found = this->queriesInFlight.find(messageId);
        if (found == this->queriesInFlight.end()) return;
        context = std::move(found->second);
        this->queriesInFlight.erase(found);
    }

    if (context) {
        this->OnQueryCompleted(context);
    }

    this->syncQueryCondition.notify_all();
}

void RemoteLibraryBackend::RunQuery(QueryContextPtr context) {
    std::unique_lock<std::recursive_mutex> lock(this->queueMutex);
#if 0
    this->RunQueryOnLoopback(context);
#else
    this->RunQueryOnWebSocketClient(context);
#endif
}

void RemoteLibraryBackend::RunQueryOnLoopback(QueryContextPtr context) {
    if (context) {
        /* do everything via loopback to the local library for testing. we do, however,
        go through the motions by serializing the inbound query to a string, then
        bouncing through the QueryRegistry to create a new instance, run the query
        locally, serialize the result, then deserialize it again to emulate the entire
        round trip. */

        auto localLibrary = musik::core::library::DefaultLocalBackend();
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

void RemoteLibraryBackend::RunQueryOnWebSocketClient(QueryContextPtr context) {
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

void RemoteLibraryBackend::SetMessageQueue(musik::core::runtime::IMessageQueue& queue) {
    if (this->messageQueue && this->messageQueue != &queue) {
        this->messageQueue->Unregister(this);
    }
    this->messageQueue = &queue;
    this->messageQueue->Register(this);
    this->wsc.SetMessageQueue(messageQueue);
}

musik::core::Indexer* RemoteLibraryBackend::Indexer() {
    return nullptr;
}

/* IMessageTarget */

void RemoteLibraryBackend::ProcessMessage(musik::core::runtime::IMessage &message) {
    if (message.Type() == MESSAGE_QUERY_COMPLETED) {
        auto context = dynamic_cast<QueryCompletedMessage*>(&message)->GetContext();
        if (context) {
            this->NotifyQueryCompleted(context);
        }
    }
    else if (message.Type() == MESSAGE_RECONNECT_SOCKET) {
        if (this->wsc.ConnectionState() == Client::State::Disconnected) {
            this->Connect();
        }
    }
    else if (message.Type() == MESSAGE_UPDATE_CONNECTION_STATE) {
        const auto updatedState = static_cast<ConnectionState>(message.UserData1());
        this->connectionState = updatedState;
        this->ConnectionStateChanged.Publish(this->connectionState);
    }
}

/* WebSocketClient::Listener */

void RemoteLibraryBackend::OnClientInvalidPassword(Client* client) {
    this->messageQueue->Post(Message::Create(this, MESSAGE_UPDATE_CONNECTION_STATE, (int) ConnectionState::AuthenticationFailure));
}

void RemoteLibraryBackend::OnClientStateChanged(Client* client, State newState, State oldState) {
    static std::map<State, ConnectionState> kConnectionStateMap = {
        { State::Disconnected, ConnectionState::Disconnected },
        { State::Disconnecting, ConnectionState::Disconnected },
        { State::Connecting, ConnectionState::Connecting },
        { State::Authenticating, ConnectionState::Connecting },
        { State::Connected, ConnectionState::Connected },
    };

    if (this->messageQueue) {
        const auto reason = this->wsc.LastConnectionError();
        const bool attemptReconnect =
            this->reconnect && newState == State::Disconnected &&
            reason != WebSocketClient::ConnectionError::InvalidPassword &&
            reason != WebSocketClient::ConnectionError::IncompatibleVersion;
        if (attemptReconnect) {
            this->messageQueue->Remove(this, MESSAGE_RECONNECT_SOCKET);
            this->messageQueue->Post(Message::Create(this, MESSAGE_RECONNECT_SOCKET), 2500);
        }
        const auto reported = newState == State::Disconnected &&
            reason == Client::ConnectionError::InvalidPassword
            ? ConnectionState::AuthenticationFailure
            : kConnectionStateMap.at(newState);
        this->messageQueue->Post(Message::Create(
            this,
            MESSAGE_UPDATE_CONNECTION_STATE,
            static_cast<int>(reported)));
    }
}

void RemoteLibraryBackend::OnClientQuerySucceeded(Client* client, const std::string& messageId, Query query) {
    this->OnQueryCompleted(messageId, query);
}

void RemoteLibraryBackend::OnClientQueryFailed(Client* client, const std::string& messageId, Query query, Client::QueryError result) {
    query->Invalidate();
    this->OnQueryCompleted(messageId, query);
}

/* RemoteLibraryBackend::RemoteResourceLocator */

std::string RemoteLibraryBackend::GetTrackUri(musik::core::Track* track, const std::string& defaultUri) {
    std::string type = ".mp3";

    const auto originalUri = track->Uri();
    if (!originalUri.empty()) {
        const std::string::size_type lastDot = originalUri.find_last_of(".");
        if (lastDot != std::string::npos) {
            type = originalUri.substr(lastDot);
        }
    }

    const auto config = this->options();
    const std::string scheme = config.httpTls ? "https://" : "http://";
    std::string uri = scheme + config.host + ":" + std::to_string(config.httpPort) +
        "/audio/id/" + std::to_string(track->GetId());

    if (config.transcoderBitrate > 0 && config.transcoderBitrate < 9999 && !config.transcoderFormat.empty()) {
        uri += "?bitrate=" + std::to_string(config.transcoderBitrate) + "&format=" + config.transcoderFormat;
        type = "." + config.transcoderFormat;
    }

    nlohmann::json path = {
        { "uri", uri },
        { "originalUri", originalUri },
        { "type", type },
        { "password", config.password },
        { "allowUntrustedCertificate", config.httpTls && config.httpTlsAllowUntrustedCertificate }
    };

    return "musikcore://remote-track/" + path.dump();
}

bool RemoteLibraryBackend::ReadThumbnail(int64_t id, std::vector<uint8_t>& bytes, size_t limit,
    std::stop_token canceled)
{
    struct Sink {
        std::vector<uint8_t>* bytes { nullptr };
        size_t limit { 0 };
        bool good { true };

        Sink& write(const char* data, size_t size) {
            if (this->bytes->size() > this->limit || size > this->limit - this->bytes->size()) {
                this->good = false;
                return *this;
            }
            const auto* begin = reinterpret_cast<const uint8_t*>(data);
            this->bytes->insert(this->bytes->end(), begin, begin + size);
            return *this;
        }

        explicit operator bool() const { return this->good; }
    };
    using Client = musik::core::sdk::HttpClient<Sink>;
    const auto config = this->options();
    const auto host = config.host.find(':') == std::string::npos ? config.host : "[" + config.host + "]";
    const auto uri = (config.httpTls ? "https://" : "http://") + host + ":" +
        std::to_string(config.httpPort) + "/thumbnail/" + std::to_string(id);
    auto client = Client::Create(Sink { &bytes, limit });
    std::optional<uint64_t> length;
    int response = 0;
    bool success = false;
    client->Url(uri).Mode(Client::Thread::Current).Cancellation(canceled)
        .VerifyTls(true).AllowUntrustedAuthority(config.httpTlsAllowUntrustedCertificate)
        .FollowRedirects(false)
        .Header("Authorization", "Basic " + websocketpp::base64_encode("default:" + config.password))
        .Headers([&](std::string key, std::string value) {
            std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c) {
                return static_cast<char>(std::tolower(c));
            });
            if (key == "content-length") {
                uint64_t parsed = 0;
                const auto result = std::from_chars(value.data(), value.data() + value.size(), parsed);
                if (result.ec == std::errc() && result.ptr == value.data() + value.size()) {
                    length = parsed;
                }
            }
        })
        .Run([&](Client*, int code, bool ok) { response = code; success = ok; });
    if (response == 404) {
        bytes.clear();
        return true;
    }
    return response == 200 && success && (!length || *length == bytes.size());
}

/* RemoteLibraryBackend */

const net::WebSocketClient& RemoteLibraryBackend::WebSocketClient() const {
    return this->wsc;
}

void RemoteLibraryBackend::Connect() {
    if (this->exit) {
        return;
    }

    const auto config = this->options();
    this->wsc.Connect(config.host, config.websocketPort, config.password,
        config.websocketTls, config.latencyTimeoutMs, config.ignoreVersionMismatch);
}

void RemoteLibraryBackend::Disconnect() {
    this->wsc.Disconnect();
}
