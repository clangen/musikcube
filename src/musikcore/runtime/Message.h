//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004-2023 musikcube team
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//    * Redistributions of source code must retain the above copyright notice,
//      this list of conditions and the following disclaimer.
//
//    * Redistributions in binary form must reproduce the above copyright
//      notice, this list of conditions and the following disclaimer in the
//      documentation and/or other materials provided with the distribution.
//
//    * Neither the name of the author nor the names of other contributors may
//      be used to endorse or promote products derived from this software
//      without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
//////////////////////////////////////////////////////////////////////////////

#pragma once

/** @file Message.h
 *  @brief Concrete IMessage implementation.
 *  @details Stores the target, type and two user-data values. Use Create() to
 *      allocate messages; the destructor releases them. */

#include "IMessage.h"

/** @namespace musik::core::runtime
 *  @brief Inter-thread message passing: messages, targets and queues. */
namespace musik { namespace core { namespace runtime {

    /** @brief A concrete runtime message.
     *  @details Created via the static Create() factory. Holds a target pointer
     *      (not owned), a type code and two int64 user-data values. */
    class Message : public IMessage {
        protected:
            /** @brief Constructs a message (use Create()).
             *  @param target The destination target.
             *  @param messageType The message type.
             *  @param data1 First user-data value.
             *  @param data2 Second user-data value. */
            Message(
                IMessageTarget* target,
                int messageType,
                int64_t data1,
                int64_t data2);

        public:
            /** @brief Allocates a new message.
             *  @param target The destination target.
             *  @param messageType The message type.
             *  @param data1 First user-data value.
             *  @param data2 Second user-data value.
             *  @return A shared message. */
            static IMessagePtr Create(
                IMessageTarget* target,
                int messageType,
                int64_t data1 = 0LL,
                int64_t data2 = 0LL);

            virtual ~Message() {
            }

            /** @return The destination target. */
            virtual IMessageTarget* Target();
            /** @return The message type. */
            virtual int Type();
            /** @return The first user-data value. */
            virtual int64_t UserData1();
            /** @return The second user-data value. */
            virtual int64_t UserData2();

        private:
            IMessageTarget* target; /**< Destination target. */
            int messageType;        /**< Message type. */
            int64_t data1, data2;   /**< User-data values. */
    };

} } }