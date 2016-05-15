#include "stdafx.h"
#include "WindowMessageQueue.h"

using namespace boost::chrono;

WindowMessageQueue WindowMessageQueue::instance;

WindowMessageQueue::WindowMessageQueue() {

}

WindowMessageQueue& WindowMessageQueue::Instance() {
    return WindowMessageQueue::instance;
}

void WindowMessageQueue::Dispatch() {
    milliseconds now = duration_cast<milliseconds>(
        system_clock::now().time_since_epoch());

    boost::recursive_mutex::scoped_lock lock(this->queueMutex);

    bool done = false;
    while (!done && !this->queue.empty()) {
        /* messages are time-ordered. pop and dispatch one at a time
        until we hit one that shouldn't be sent yet. */

        EnqueuedMessage *m = this->queue.front();
        if (now >= m->time) {
            this->Dispatch(m->message);
            this->queue.pop_front();
            delete m;
        }
        else {
            done = true;
        }
    }
}

void WindowMessageQueue::Remove(IWindow *target, int type) {
    boost::recursive_mutex::scoped_lock lock(this->queueMutex);

    std::list<EnqueuedMessage*>::iterator it = this->queue.begin();
    while (it != this->queue.end()) {
        IWindowMessagePtr current = (*it)->message;

        if (current->Target().get() == target) {
            if (type == -1 || type == current->MessageType()) {
                delete (*it);
                it = this->queue.erase(it);
                continue;
            }
        }

        ++it;
    }
}

void WindowMessageQueue::Post(IWindowMessagePtr message, int64 delayMs) {
    boost::recursive_mutex::scoped_lock lock(this->queueMutex);

    delayMs = max(0, delayMs);

    EnqueuedMessage *m = new EnqueuedMessage();
    m->message = message;
    m->time = milliseconds::zero() + milliseconds(delayMs);

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

void WindowMessageQueue::Dispatch(IWindowMessagePtr message) {
    message->Target()->ProcessMessage(*message);
}
