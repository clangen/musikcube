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
#include <core/library/QueryRegistry.h>
#include <core/runtime/Message.h>
#include <core/debug.h>

#include <core/library/LibraryFactory.h> /* CAL TODO: remove this */

static const std::string TAG = "RemoteLibrary";

using namespace musik::core;
using namespace musik::core::db;
using namespace musik::core::library;
using namespace musik::core::runtime;

#define MESSAGE_QUERY_COMPLETED 5000

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

RemoteLibrary::RemoteLibrary(std::string name,int id)
: name(name)
, id(id)
, exit(false)
, messageQueue(nullptr) {
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
    std::thread* thread = nullptr;

    {
        std::unique_lock<std::recursive_mutex> lock(this->mutex);
        if (this->thread) {
            thread = this->thread;
            this->thread = nullptr;
            this->queryQueue.clear();
            this->exit = true;
        }
    }

    if (thread) {
        this->queueCondition.notify_all();
        thread->join();
        delete thread;
    }
}

int RemoteLibrary::Enqueue(QueryPtr query, unsigned int options, Callback callback) {
    auto serializableQuery = std::dynamic_pointer_cast<ISerializableQuery>(query);

    if (serializableQuery) {
        std::unique_lock<std::recursive_mutex> lock(this->mutex);

        if (this->exit) { /* closed */
            return -1;
        }

        auto context = std::make_shared<QueryContext>();
        context->query = serializableQuery;
        context->callback = callback;

        if (options & ILibrary::QuerySynchronous) {
            this->RunQuery(context, false); /* false = do not notify via QueryCompleted */
        }
        else {
            queryQueue.push_back(context);
            queueCondition.notify_all();
        }

        return query->GetId();
    }

    return -1;
}

RemoteLibrary::QueryContextPtr RemoteLibrary::GetNextQuery() {
    std::unique_lock<std::recursive_mutex> lock(this->mutex);
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

void RemoteLibrary::RunQuery(QueryContextPtr context, bool notify) {
    if (context) {
        /* CAL TODO: eventually make the request over a websocket. right now we just
        do everything via loopback to the local library for testing. we do, however,
        go through the motions by serializing the inbound query to a string, then
        bouncing through the QueryRegister to create a new instance, run the query
        locally, serialize the result, then deserialize it again to emulate the entire
        flow. */

        auto localLibrary = LibraryFactory::Default();

        auto localQuery = QueryRegistry::CreateLocalQueryFor(
            context->query->Name(), context->query->SerializeQuery(), localLibrary);

        auto onComplete = [this, notify, context, localQuery]() {
            if (notify) {
                if (this->messageQueue) {
                    this->messageQueue->Post(std::make_shared<QueryCompletedMessage>(this, context));
                }
                else {
                    this->QueryCompleted(localQuery.get());
                }
            }
            else if (context->callback) {
                context->callback(context->query);
            }
        };

        if (!localQuery) {
            onComplete();
            return;
        }

        localLibrary->Enqueue(
            localQuery, 
            0, 
            [context, onComplete, localQuery](auto result) {
                if (localQuery->GetStatus() == IQuery::Finished) {
                    context->query->DeserializeResult(localQuery->SerializeResult());
                }
                onComplete();
            });
    }
}

void RemoteLibrary::SetMessageQueue(musik::core::runtime::IMessageQueue& queue) {
    this->messageQueue = &queue;
}

void RemoteLibrary::ProcessMessage(musik::core::runtime::IMessage &message) {
    if (message.Type() == MESSAGE_QUERY_COMPLETED) {
        auto context = static_cast<QueryCompletedMessage*>(&message)->GetContext();
        auto query = context->query;

        this->QueryCompleted(context->query.get());

        if (context->callback) {
            context->callback(query);
        }
    }
}

musik::core::IIndexer* RemoteLibrary::Indexer() {
    return nullptr;
}
