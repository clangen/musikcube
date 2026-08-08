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

/** @file IMessage.h
 *  @brief Abstract interface for runtime messages.
 *  @details A message targets an IMessageTarget and carries an int type plus two
 *      int64 user data values. Messages are posted to a queue and dispatched to
 *      their target. */

#include <musikcore/config.h>
#include <memory>

/** @namespace musik::core::runtime
 *  @brief Inter-thread message passing: messages, targets and queues. */
namespace musik { namespace core { namespace runtime {

    class IMessageTarget;

    /** @brief A unit of inter-thread communication.
     *  @details Delivered to its target by a message queue. The type identifies the
     *      kind of message; the two user-data slots carry payload. */
    class IMessage {
        public:
            virtual ~IMessage() { }
            /** @return The target that should process this message. */
            virtual IMessageTarget* Target() = 0;
            /** @return The message type identifier. */
            virtual int Type() = 0;
            /** @return The first user-data value. */
            virtual int64_t UserData1() = 0;
            /** @return The second user-data value. */
            virtual int64_t UserData2() = 0;
    };

    typedef std::shared_ptr<IMessage> IMessagePtr; /**< Shared message alias. */

} } }
