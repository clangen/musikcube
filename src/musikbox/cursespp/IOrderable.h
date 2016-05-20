#pragma once

class IOrderable {
    public:
        virtual ~IOrderable() = 0 { }
        virtual void BringToTop() = 0;
        virtual void SendToBottom() = 0;
};