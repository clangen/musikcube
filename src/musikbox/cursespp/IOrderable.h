#pragma once

namespace cursespp {
    class IOrderable {
        public:
            virtual ~IOrderable() { }
            virtual void BringToTop() = 0;
            virtual void SendToBottom() = 0;
    };
}
