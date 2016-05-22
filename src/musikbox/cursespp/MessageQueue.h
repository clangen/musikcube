#pragma once

#include "IMessage.h"
#include "IMessageTarget.h"

#include <boost/thread/recursive_mutex.hpp>
#include <boost/chrono.hpp>

class MessageQueue {
    public:
        static MessageQueue& Instance();

        void Post(IMessagePtr message, int64 delayMs = 0);
        void Remove(IMessageTarget *target, int type = -1);

        void Dispatch();

    private:
        static MessageQueue instance;

        struct EnqueuedMessage {
            IMessagePtr message;
            boost::chrono::milliseconds time;
        };

        boost::recursive_mutex queueMutex;
        std::list<EnqueuedMessage*> queue;

        MessageQueue();
        void Dispatch(IMessagePtr message);

};