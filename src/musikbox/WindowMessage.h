#pragma once

#include "IWindowMessage.h"

class WindowMessage : public IWindowMessage {
    public:
        WindowMessage(
            IWindowPtr target, 
            int messageType,
            int64 data1, 
            int64 data2);

        virtual IWindowPtr Target();
        virtual int MessageType();
        virtual int64 UserData1();
        virtual int64 UserData2();

    private:
        IWindowPtr target;
        int messageType;
        int64 data1, data2;
};