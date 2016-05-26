#pragma once

#include "IMessage.h"

namespace cursespp {
    class IMessageTarget {
        public:
            virtual ~IMessageTarget() { }
            virtual bool IsAcceptingMessages() = 0;
            virtual void ProcessMessage(IMessage &message) = 0;
    };
}
