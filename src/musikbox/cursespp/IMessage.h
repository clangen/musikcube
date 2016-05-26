#pragma once

#include <stdafx.h>
#include <memory>

namespace cursespp {

    class IMessageTarget;

    class IMessage {
        public:
            virtual ~IMessage() { }
            virtual IMessageTarget* Target() = 0;
            virtual int Type() = 0;
            virtual int64 UserData1() = 0;
            virtual int64 UserData2() = 0;
    };

    typedef std::shared_ptr<IMessage> IMessagePtr;
}
