#pragma once

#include "IMessage.h"

namespace cursespp {
    class Message : public IMessage {
        private:
            Message(
                IMessageTarget* target,
                int messageType,
                int64 data1,
                int64 data2);

        public:
            static IMessagePtr Create(
                IMessageTarget* target,
                int messageType,
                int64 data1,
                int64 data2);

            virtual IMessageTarget* Target();
            virtual int Type();
            virtual int64 UserData1();
            virtual int64 UserData2();

        private:
            IMessageTarget* target;
            int messageType;
            int64 data1, data2;
    };
}