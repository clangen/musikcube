#include <stdafx.h>
#include "Message.h"

IMessagePtr Message::Create(
    IMessageTarget* target,
    int messageType,
    int64 data1,
    int64 data2)
{
    return IMessagePtr(new Message(target, messageType, data1, data2));
}

Message::Message(
    IMessageTarget* target,
    int messageType,
    int64 data1,
    int64 data2)
{
    this->target = target;
    this->messageType = messageType;
    this->data1 = data1;
    this->data2 = data2;
}

IMessageTarget* Message::Target() {
    return this->target;
}

int Message::MessageType() {
    return this->messageType;
}

int64 Message::UserData1() {
    return this->data1;
}

int64 Message::UserData2() {
    return this->data2;
}