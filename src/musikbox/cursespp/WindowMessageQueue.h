#pragma once

#include "IWindowMessage.h"

#include <boost/thread/recursive_mutex.hpp>
#include <boost/chrono.hpp>

class WindowMessageQueue {
    public:
        static WindowMessageQueue& Instance();

        void Post(IWindowMessagePtr message, int64 delayMs = 0);
        void Remove(IWindow *target, int type = -1);

        void Dispatch();

    private:
        static WindowMessageQueue instance;

        struct EnqueuedMessage {
            IWindowMessagePtr message;
            boost::chrono::milliseconds time;
        };

        boost::recursive_mutex queueMutex;
        std::list<EnqueuedMessage*> queue;

        WindowMessageQueue();
        void Dispatch(IWindowMessagePtr message);

};