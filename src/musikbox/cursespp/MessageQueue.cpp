#include <stdafx.h>
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
                if (m->message.get()->Target()->IsAcceptingMessages()) {
                    toDispatch.push_back(m);
                    it = this->queue.erase(it);
                }
                else {
                    it++;
                }
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

    delayMs = max(0, delayMs);

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
