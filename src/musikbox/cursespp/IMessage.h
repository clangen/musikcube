#pragma once

#include <stdafx.h>
#include <memory>

class IMessageTarget;

class IMessage {
    public:
        virtual ~IMessage() = 0 { }
        virtual IMessageTarget* Target() = 0;
        virtual int MessageType() = 0;
        virtual int64 UserData1() = 0;
        virtual int64 UserData2() = 0;
};

typedef std::shared_ptr<IMessage> IMessagePtr;