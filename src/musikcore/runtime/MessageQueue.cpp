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
#include "MessageQueue.h"

#include <algorithm>
#include <chrono>

using namespace std::chrono;
using namespace musik::core::runtime;

using LockT = std::unique_lock<std::mutex>;

MessageQueue::MessageQueue() {
    this->nextMessageTime.store(1);
}

void MessageQueue::WaitAndDispatch(int64_t timeoutMillis) {
    {
        LockT lock(this->queueMutex);

        if (this->queue.size()) {
            auto waitTime = duration_cast<milliseconds>(
                this->queue.front()->time -
                system_clock::now().time_since_epoch());

            if (timeoutMillis >= 0) {
                auto timeoutDuration = milliseconds(timeoutMillis);
                if (waitTime > timeoutDuration) {
                    waitTime = timeoutDuration;
                }
            }

            if (waitTime.count() > 0) {
                waitForDispatch.wait_for(lock, waitTime);
            }
       }
        else {
            if (timeoutMillis >= 0) {
                waitForDispatch.wait_for(lock, milliseconds(timeoutMillis));
            }
            else {
                waitForDispatch.wait(lock);
            }
        }
    }

    this->Dispatch();
}

MessageQueue::~MessageQueue() {

}

void MessageQueue::Dispatch() {
    milliseconds now = duration_cast<milliseconds>(
        system_clock::now().time_since_epoch());

    int64_t nextTime = nextMessageTime.load();

    if (nextTime > now.count() || nextTime < 0) {
        return; /* short circuit before any iteration. */
    }

    using Iterator = std::list<EnqueuedMessage*>::iterator;

    {
        LockT lock(this->queueMutex);

        this->nextMessageTime.store(-1);

        Iterator it = this->queue.begin();

        bool done = false;
        while (!done && it != this->queue.end()) {
            /* messages are time-ordered. pop and dispatch one at a time
            until we get to a message that should be delivered in the future,
            or the queue has been exhausted. */

            EnqueuedMessage *m = (*it);

            if (now >= m->time) {
                this->dispatch.push_back(m);
                it = this->queue.erase(it);
            }
            else {
                done = true;
            }
        }
    }

    /* dispatch outside of the critical section */

    Iterator it = this->dispatch.begin();
    while (it != this->dispatch.end()) {
        this->Dispatch((*it)->message);
        delete *it;
        it++;
    }

    this->dispatch.clear();

    if (this->queue.size()) {
        this->nextMessageTime.store((*this->queue.begin())->time.count());
    }
}

void MessageQueue::RegisterForBroadcasts(IMessageTargetPtr target) {
    LockT lock(this->queueMutex);
    this->receivers.insert(target);
}

void MessageQueue::UnregisterForBroadcasts(IMessageTarget *target) {
    LockT lock(this->queueMutex);
    for (auto it : this->receivers) {
        auto shared = it.lock();
        if (shared && shared.get() == target) {
            this->receivers.erase(it);
            return;
        }
    }
}

int MessageQueue::Remove(IMessageTarget *target, int type) {
    LockT lock(this->queueMutex);

    int count = 0;
    std::list<EnqueuedMessage*>::iterator it = this->queue.begin();
    while (it != this->queue.end()) {
        IMessagePtr current = (*it)->message;

        if (current->Target() == target) {
            if (type == -1 || type == current->Type()) {
                delete (*it);
                it = this->queue.erase(it);
                ++count;
                continue;
            }
        }

        ++it;
    }

    if (this->queue.size()) {
        this->nextMessageTime.store((*this->queue.begin())->time.count());
    }

    return count;
}

bool MessageQueue::Contains(IMessageTarget *target, int type) {
    LockT lock(this->queueMutex);

    std::list<EnqueuedMessage*>::iterator it = this->queue.begin();
    while (it != this->queue.end()) {
        IMessagePtr current = (*it)->message;

        if (current->Target() == target) {
            if (type == -1 || type == current->Type()) {
                return true;
            }
        }

        ++it;
    }
    return false;
}

void MessageQueue::Broadcast(IMessagePtr message, int64_t delayMs) {
    if (message->Target()) {
        throw new std::runtime_error("broadcasts cannot have a target!");
    }

    this->Post(message, delayMs);
}

void MessageQueue::Post(IMessagePtr message, int64_t delayMs) {
    LockT lock(this->queueMutex);

    delayMs = std::max((int64_t) 0, delayMs);

    milliseconds now = duration_cast<milliseconds>(
        system_clock::now().time_since_epoch());

    EnqueuedMessage *m = new EnqueuedMessage();
    m->message = message;
    m->time = now + milliseconds(delayMs);

    /* the queue is time ordered. start from the front of the queue, and
    work our way back until we find the correct place to insert the new one */

    typedef std::list<EnqueuedMessage*>::iterator Iterator;
    Iterator curr = this->queue.begin();

    while (curr != this->queue.end()) {
        if ((*curr)->time <= m->time) {
            curr++;
        }
        else {
            break;
        }
    }

    bool first = (curr == this->queue.begin());

    this->queue.insert(curr, m);

    if (this->queue.size()) {
        this->nextMessageTime.store((*this->queue.begin())->time.count());
    }

    if (first) {
        this->waitForDispatch.notify_all();
    }
}

void MessageQueue::Debounce(IMessagePtr message, int64_t delayMs) {
    Remove(message->Target(), message->Type());
    Post(message, delayMs);
}

void MessageQueue::Dispatch(IMessagePtr message) {
    if (message->Target()) {
        message->Target()->ProcessMessage(*message);
    }
    else {
        std::set<IWeakMessageTarget, WeakPtrLess> copy;

        /* copy to dispatch outside of a lock */
        {
            LockT lock(this->queueMutex);
            std::copy(
                receivers.begin(),
                receivers.end(),
                std::inserter(copy, copy.begin()));
        }

        /* dispatch */
        bool prune = false;
        for (auto receiver : copy) {
            auto shared = receiver.lock();
            if (shared) {
                shared->ProcessMessage(*message);
            }
            else {
                prune = true;
            }
        }

        if (prune) { /* at least one of our weak_ptrs is dead. */
            LockT lock(this->queueMutex);
            auto it = this->receivers.begin();
            while (it != this->receivers.end()) {
                if (it->expired()) {
                    it = this->receivers.erase(it);
                }
                else {
                    ++it;
                }
            }
        }
    }
}
