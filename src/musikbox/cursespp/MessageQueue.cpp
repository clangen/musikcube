//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2007-2016 musikcube team
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

#include <stdafx.h>
#include <algorithm>
#include "MessageQueue.h"

using namespace boost::chrono;
using namespace cursespp;

MessageQueue MessageQueue::instance;

MessageQueue::MessageQueue() {

}

MessageQueue& MessageQueue::Instance() {
    return MessageQueue::instance;
}

void MessageQueue::Dispatch() {
    milliseconds now = duration_cast<milliseconds>(
        system_clock::now().time_since_epoch());

    typedef std::list<EnqueuedMessage*>::iterator Iterator;
    std::list<EnqueuedMessage*> toDispatch;

    {
        boost::recursive_mutex::scoped_lock lock(this->queueMutex);

        Iterator it = this->queue.begin();

        bool done = false;
        while (!done && it != this->queue.end()) {
            /* messages are time-ordered. pop and dispatch one at a time
            until we get to a message that should be delivered in the future,
            or the queue has been exhausted. */

            EnqueuedMessage *m = (*it);

            if (now >= m->time) {
                toDispatch.push_back(m);
                it = this->queue.erase(it);
            }
            else {
                done = true;
            }
        }
    }

    /* dispatch outside of the critical section */

    Iterator it = toDispatch.begin();
    while (it != toDispatch.end()) {
        this->Dispatch((*it)->message);
        delete *it;
        it++;
    }
}

void MessageQueue::Remove(IMessageTarget *target, int type) {
    boost::recursive_mutex::scoped_lock lock(this->queueMutex);

    std::list<EnqueuedMessage*>::iterator it = this->queue.begin();
    while (it != this->queue.end()) {
        IMessagePtr current = (*it)->message;

        if (current->Target() == target) {
            if (type == -1 || type == current->Type()) {
                delete (*it);
                it = this->queue.erase(it);
                continue;
            }
        }

        ++it;
    }
}

void MessageQueue::Post(IMessagePtr message, int64 delayMs) {
    boost::recursive_mutex::scoped_lock lock(this->queueMutex);

    delayMs = std::max((int64) 0, delayMs);

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

    this->queue.insert(curr, m);
}

void MessageQueue::Dispatch(IMessagePtr message) {
    message->Target()->ProcessMessage(*message);
}
