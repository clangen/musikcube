#pragma once

#include "IMessage.h"

class IMessageTarget {
    public:
        virtual bool IsAcceptingMessages() = 0;
        virtual void ProcessMessage(IMessage &message) = 0;
};