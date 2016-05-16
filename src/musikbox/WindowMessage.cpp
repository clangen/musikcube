#include "stdafx.h"

#include "WindowMessage.h"

IWindowMessagePtr WindowMessage::Create(
    IWindow* target,
    int messageType,
    int64 data1,
    int64 data2)
{
    return IWindowMessagePtr(new WindowMessage(target, messageType, data1, data2));
}

WindowMessage::WindowMessage(
    IWindow* target,
    int messageType,
    int64 data1,
    int64 data2)
{
    this->target = target;
    this->messageType = messageType;
    this->data1 = data1;
    this->data2 = data2;
}

IWindow* WindowMessage::Target() {
    return this->target;
}

int WindowMessage::MessageType() {
    return this->messageType;
}

int64 WindowMessage::UserData1() {
    return this->data1;
}

int64 WindowMessage::UserData2() {
    return this->data2;
}