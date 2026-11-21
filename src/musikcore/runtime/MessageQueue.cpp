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

#include "MessageQueue.h"

#include <algorithm>
#include <chrono>
#include <iterator>
#include <stdexcept>

using namespace std::chrono;
using namespace musik::core::runtime;

using LockT = std::unique_lock<std::mutex>;

MessageQueue::MessageQueue() noexcept {
    this->nextMessageTime.store(1);
}

MessageQueue::~MessageQueue() {
    /* Payload destructors can call back into this queue. Detach entries under
       the lock, then destroy them unlocked while queue members still exist.
       Producers and dispatch must already be stopped before destruction. */
    std::list<EnqueuedMessage*> pending;
    {
        LockT lock(this->queueMutex);
        this->stopping = true;
        pending.splice(pending.end(), this->queue);
        pending.splice(pending.end(), this->dispatch);
        this->targets.clear();
        this->receivers.clear();
        this->nextMessageTime.store(-1);
    }

    for (auto entry : pending) {
        delete entry;
    }
}

void MessageQueue::WaitAndDispatch(int64_t timeoutMillis) {
    {
        LockT lock(this->queueMutex);

        if (this->queue.size()) {
            auto waitTime = duration_cast<milliseconds>(
                this->queue.front()->time -
                system_clock::now().time_since_epoch());

            if (timeoutMillis >= 0) {
                const auto timeoutDuration = milliseconds(timeoutMillis);
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

bool MessageQueue::WaitForReady(const std::atomic<bool>& stop) {
    LockT lock(this->queueMutex);
    while (!stop.load()) {
        if (this->queue.empty()) {
            this->waitForDispatch.wait(lock);
        }
        else {
            const auto deadline = system_clock::time_point(this->queue.front()->time);
            if (system_clock::now() >= deadline) {
                return true;
            }
            this->waitForDispatch.wait_until(lock, deadline);
        }
    }
    return false;
}

void MessageQueue::WakeWaiters() {
    /* Synchronize with the stop check before the waiter unlocks and parks. */
    LockT lock(this->queueMutex);
    this->waitForDispatch.notify_all();
}

void MessageQueue::Dispatch() {
    const milliseconds now = duration_cast<milliseconds>(
        system_clock::now().time_since_epoch());

    const int64_t nextTime = nextMessageTime.load();

    if (nextTime > now.count() || nextTime < 0) {
        return; /* short circuit before any iteration. */
    }

    using Iterator = std::list<EnqueuedMessage*>::iterator;
    std::list<EnqueuedMessage*> discarded;

    {
        LockT lock(this->queueMutex);

        this->nextMessageTime.store(-1);

        Iterator it = this->queue.begin();

        bool done = false;
        while (!done && it != this->queue.end()) {
            /* messages are time-ordered. pop and dispatch one at a time
            until we get to a message that should be delivered in the future,
            or the queue has been exhausted. */

            EnqueuedMessage* m = (*it);

            if (now >= m->time) {
                /* it's possible the target (receiver) has been unregistered;
                if that's the case, just discard it. otherwise, add it to the
                output set to be dispatched outside of the critical section */
                auto const target = (*it)->message->Target();
                auto entry = it++;
                if (target == nullptr || this->targets.find(target) != this->targets.end()) {
                    this->dispatch.splice(this->dispatch.end(), this->queue, entry);
                }
                else {
                    discarded.splice(discarded.end(), this->queue, entry);
                }
            }
            else {
                done = true;
            }
        }
    }

    /* dispatch outside of the critical section */

    for (auto entry : discarded) {
        delete entry;
    }
    while (true) {
        EnqueuedMessage* entry;
        {
            LockT lock(this->queueMutex);
            if (this->dispatch.empty()) {
                break;
            }
            entry = this->dispatch.front();
            this->dispatch.pop_front();
        }
        this->Dispatch(entry->message.get());
        delete entry;
    }

    {
        LockT lock(this->queueMutex);
        this->nextMessageTime.store(this->queue.empty() ? -1 : this->queue.front()->time.count());
    }
}

void MessageQueue::Register(IMessageTarget* target) {
    LockT lock(this->queueMutex);
    if (this->stopping) {
        return;
    }
    this->targets.insert(target);
}

void MessageQueue::Unregister(IMessageTarget* target) {
    std::list<EnqueuedMessage*> removed;
    {
        LockT lock(this->queueMutex);
        if (!this->targets.erase(target)) {
            return;
        }

        /* Clear both lists before the address can be registered again. */
        this->DetachMessages(removed, this->queue, target, -1);
        this->DetachMessages(removed, this->dispatch, target, -1);
        this->nextMessageTime.store(this->queue.empty() ? -1 : this->queue.front()->time.count());
        this->waitForDispatch.notify_all();
    }

    /* Payload destruction can reenter the queue. */
    for (auto entry : removed) {
        delete entry;
    }
}

void MessageQueue::RegisterForBroadcasts(IMessageTargetPtr target) {
    LockT lock(this->queueMutex);
    if (this->stopping) {
        return;
    }
    this->receivers.insert(target);
}

void MessageQueue::UnregisterForBroadcasts(IMessageTarget* target) {
    LockT lock(this->queueMutex);
    for (auto it : this->receivers) {
        auto shared = it.lock();
        if (shared && shared.get() == target) {
            this->receivers.erase(it);
            return;
        }
    }
}

void MessageQueue::DetachMessages(std::list<EnqueuedMessage*>& removed,
    std::list<EnqueuedMessage*>& source, IMessageTarget* target, int type)
{
    for (auto it = source.begin(); it != source.end();) {
        auto entry = it++;
        const auto& current = (*entry)->message;
        if (current->Target() == target && (type == -1 || type == current->Type())) {
            removed.splice(removed.end(), source, entry);
        }
    }
}

int MessageQueue::Remove(IMessageTarget* target, int type) {
    std::list<EnqueuedMessage*> removed;
    {
        LockT lock(this->queueMutex);
        this->DetachMessages(removed, this->queue, target, type);
        this->nextMessageTime.store(this->queue.empty() ? -1 : this->queue.front()->time.count());
        this->waitForDispatch.notify_all();
    }

    /* Payload destruction can release libraries/targets that reenter this queue. */
    const auto count = static_cast<int>(removed.size());
    for (auto entry : removed) {
        delete entry;
    }
    return count;
}

bool MessageQueue::Contains(IMessageTarget* target, int type) {
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
    LockT lock(this->queueMutex);
    if (this->stopping) {
        return;
    }

    if (message->Target()) {
        throw new std::runtime_error("broadcasts cannot have a target!");
    }

    this->Enqueue(message, delayMs);
}

void MessageQueue::Post(IMessagePtr message, int64_t delayMs) {
    LockT lock(this->queueMutex);
    if (this->stopping) {
        return;
    }

    if (this->targets.find(message->Target()) == this->targets.end()) {
        return;
    }

    this->Enqueue(message, delayMs);
}

void MessageQueue::Enqueue(IMessagePtr message, int64_t delayMs) {
    delayMs = std::max((int64_t) 0, delayMs);

    const milliseconds now = duration_cast<milliseconds>(
        system_clock::now().time_since_epoch());

    EnqueuedMessage* m = new EnqueuedMessage();
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

void MessageQueue::Dispatch(IMessage* message) {
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
