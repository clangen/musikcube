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

#include <core/library/RemoteLibrary.h>
#include <core/config.h>
#include <core/support/Common.h>
#include <core/support/Preferences.h>
#include <core/library/Indexer.h>
#include <core/library/IQuery.h>
#include <core/library/LibraryFactory.h>
#include <core/library/QueryRegistry.h>
#include <core/runtime/Message.h>
#include <core/debug.h>
#include <nlohmann/json.hpp>

static const std::string TAG = "RemoteLibrary";

using namespace musik::core;
using namespace musik::core::db;
using namespace musik::core::library;
using namespace musik::core::runtime;

#define MESSAGE_QUERY_COMPLETED 5000

static const std::string kServerHost = "127.0.0.1";
static const std::string kServerPassword = "";

class NullIndexer: public musik::core::IIndexer {
    public:
        virtual ~NullIndexer() override { }
        virtual void AddPath(const std::string& path) override { }
        virtual void RemovePath(const std::string& path) override { }
        virtual void GetPaths(std::vector<std::string>& paths) override { }
        virtual void Schedule(SyncType type) override { }
        virtual void Stop() override { }
        virtual State GetState() override { return StateIdle; }
} kNullIndexer;

class RemoteLibrary::QueryCompletedMessage: public Message {
    public:
        using QueryContextPtr = RemoteLibrary::QueryContextPtr;

        QueryCompletedMessage(IMessageTarget* target, QueryContextPtr context)
        : Message(target, MESSAGE_QUERY_COMPLETED, 0, 0) {
            this->context = context;
        }

        virtual ~QueryCompletedMessage() {
        }

        QueryContextPtr GetContext() { return this->context; }

    private:
        QueryContextPtr context;
};

ILibraryPtr RemoteLibrary::Create(std::string name, int id) {
    ILibraryPtr lib(new RemoteLibrary(name, id));
    return lib;
}

RemoteLibrary::RemoteLibrary(std::string name, int id)
: name(name)
, id(id)
, exit(false)
, messageQueue(nullptr)
, wsc(this) {
    this->wsc.Connect("ws://" + kServerHost + ":7905", kServerPassword);
    this->identifier = std::to_string(id);
    this->thread = new std::thread(std::bind(&RemoteLibrary::ThreadProc, this));
}

RemoteLibrary::~RemoteLibrary() {
    this->Close();
}

int RemoteLibrary::Id() {
    return this->id;
}

const std::string& RemoteLibrary::Name() {
    return this->name;
}

void RemoteLibrary::Close() {
    this->wsc.Disconnect();

    std::thread* thread = nullptr;

    {
        std::unique_lock<std::recursive_mutex> lock(this->queueMutex);
        if (this->thread) {
            thread = this->thread;
            this->thread = nullptr;
            this->queryQueue.clear();
            this->exit = true;
        }
    }

    if (thread) {
        this->queueCondition.notify_all();
        this->syncQueryCondition.notify_all();
        thread->join();
        delete thread;
    }
}

bool RemoteLibrary::IsConfigured() {
    return LibraryFactory::Instance().Default()->IsConfigured(); /* CAL TODO FIXME */
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
    return false;
}

int RemoteLibrary::Enqueue(QueryPtr query, unsigned int options, Callback callback) {
    if (QueryRegistry::IsLocalOnlyQuery(query->Name())) {
        auto defaultLocalLibrary = LibraryFactory::Instance().Default();
        return defaultLocalLibrary->Enqueue(query, options, callback);
    }
    
    auto serializableQuery = std::dynamic_pointer_cast<ISerializableQuery>(query);

    if (serializableQuery) {
        auto context = std::make_shared<QueryContext>();
        context->query = serializableQuery;
        context->callback = callback;

        if (options & ILibrary::QuerySynchronous) {
            this->RunQuery(context); /* false = do not notify via QueryCompleted */
            std::unique_lock<std::recursive_mutex> lock(this->queueMutex);
            while (
                !this->exit &&
                this->IsQueryInFlight(context->query) &&
                !isQueryDone(context->query))
            {
                this->syncQueryCondition.wait(lock);
            }
        }
        else {
            std::unique_lock<std::recursive_mutex> lock(this->queueMutex);
            if (this->exit) { return -1; }
            queryQueue.push_back(context);
            queueCondition.notify_all();
        }

        return query->GetId();
    }

    return -1;
}

RemoteLibrary::QueryContextPtr RemoteLibrary::GetNextQuery() {
    std::unique_lock<std::recursive_mutex> lock(this->queueMutex);
    while (!this->queryQueue.size() && !this->exit) {
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
        bouncing through the QueryRegister to create a new instance, run the query
        locally, serialize the result, then deserialize it again to emulate the entire
        flow. */

        auto localLibrary = LibraryFactory::Instance().Default();
        localLibrary->SetMessageQueue(*this->messageQueue);

        auto localQuery = QueryRegistry::CreateLocalQueryFor(
            context->query->Name(), context->query->SerializeQuery(), localLibrary);

        if (!localQuery) {
            OnQueryCompleted(context);
            return;
        }

        localLibrary->Enqueue(
            localQuery,
            ILibrary::QuerySynchronous, /* CAL TODO: make async! we have to make TrackList support async lookup first tho. */
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
        std::unique_lock<std::recursive_mutex> lock(this->queueMutex);
        const std::string messageId = wsc.EnqueueQuery(context->query);
        if (messageId.size()) {
            queriesInFlight[messageId] = context;
        }
    }
}

void RemoteLibrary::SetMessageQueue(musik::core::runtime::IMessageQueue& queue) {
    this->messageQueue = &queue;
}

musik::core::IIndexer* RemoteLibrary::Indexer() {
    return &kNullIndexer;
}

/* IMessageTarget */

void RemoteLibrary::ProcessMessage(musik::core::runtime::IMessage &message) {
    if (message.Type() == MESSAGE_QUERY_COMPLETED) {
        auto context = static_cast<QueryCompletedMessage*>(&message)->GetContext();
        this->NotifyQueryCompleted(context);
    }
}

/* WebSocketClient::Listener */

void RemoteLibrary::OnClientInvalidPassword(Client* client) {

}

void RemoteLibrary::OnClientStateChanged(Client* client, State newState, State oldState) {

}

void RemoteLibrary::OnClientQuerySucceeded(Client* client, const std::string& messageId, Query query) {
    this->OnQueryCompleted(messageId, query);
}

void RemoteLibrary::OnClientQueryFailed(Client* client, const std::string& messageId, Query query, Client::ErrorCode result) {
    this->OnQueryCompleted(messageId, query);
}

/* RemoteLibrary::RemoteResourceLocator */

std::string RemoteLibrary::GetTrackUri(musik::core::sdk::ITrack* track, const std::string& defaultUri) {
    std::string type = ".mp3";

    char buffer[4096];
    int size = track->Uri(buffer, sizeof(buffer));
    if (size) {
        std::string originalUri = buffer;
        std::string::size_type lastDot = originalUri.find_last_of(".");
        if (lastDot != std::string::npos) {
            type = originalUri.substr(lastDot).c_str();
        }
    }

    const std::string uri = "http://" + kServerHost + ":7906/audio/id/" + std::to_string(track->GetId());
    nlohmann::json path = {
        { "uri", uri },
        { "type", type },
        { "password", kServerPassword }
    };

    return "musikcore://remote-track/" + path.dump();
}
