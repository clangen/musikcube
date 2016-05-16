#pragma once

#include "IWindowMessage.h"

class WindowMessage : public IWindowMessage {
    private:
        WindowMessage(
            IWindow* target, 
            int messageType,
            int64 data1, 
            int64 data2);
        
    public:
        static IWindowMessagePtr Create(
            IWindow* target,
            int messageType,
            int64 data1,
            int64 data2);
        
        virtual IWindow* Target();
        virtual int MessageType();
        virtual int64 UserData1();
        virtual int64 UserData2();

    private:
        IWindow* target;
        int messageType;
        int64 data1, data2;
};