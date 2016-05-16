#pragma once

#include "stdafx.h"
#include "IWindow.h"
#include <memory>

class IWindowMessage {
    public:
        virtual IWindow* Target() = 0;
        virtual int MessageType() = 0;
        virtual int64 UserData1() = 0;
        virtual int64 UserData2() = 0;
};

typedef std::shared_ptr<IWindowMessage> IWindowMessagePtr;