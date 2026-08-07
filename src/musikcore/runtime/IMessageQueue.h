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

/** @file IMessageQueue.h
 *  @brief Abstract interface for message queues.
 *  @details A message queue accepts posted messages, delivers them to their
 *      targets (optionally after a delay), supports debouncing and broadcasts,
 *      and tracks registered message targets. */

#include "IMessage.h"
#include "IMessageTarget.h"

/** @namespace musik::core::runtime
 *  @brief Inter-thread message passing: messages, targets and queues. */
namespace musik { namespace core { namespace runtime {

    /** @brief Message queue interface.
     *  @details Messages posted to the queue are delivered to their target when
     *      Dispatch() (or the dispatch thread) runs. Implementations may deliver
     *      on a dedicated thread. */
    class IMessageQueue {
        public:
            virtual ~IMessageQueue() { }
            /** @brief Posts a message for delivery, optionally after a delay.
             *  @param message The message to deliver.
             *  @param delayMs Delay before delivery, in milliseconds. */
            virtual void Post(IMessagePtr message, int64_t delayMs = 0) = 0;
            /** @brief Removes queued messages for a target.
             *  @param target The target to remove messages for.
             *  @param type Optional message type filter (-1 for all).
             *  @return The number of messages removed. */
            virtual int Remove(IMessageTarget *target, int type = -1) = 0;
            /** @brief Posts a message to every broadcast receiver.
             *  @param message The message to broadcast.
             *  @param delayMs Delay before delivery, in milliseconds. */
            virtual void Broadcast(IMessagePtr message, int64_t delayMs = 0) = 0;
            /** @return true if a message for the target (and type) is queued.
             *  @param target The target to check.
             *  @param type Optional message type filter (-1 for any). */
            virtual bool Contains(IMessageTarget *target, int type = -1) = 0;
            /** @brief Posts a message, first removing prior messages of the same type.
             *  @param message The message to deliver.
             *  @param delayMs Delay before delivery, in milliseconds. */
            virtual void Debounce(IMessagePtr message, int64_t delayMs = 0) = 0;
            /** @brief Registers a target that can receive messages.
             *  @param target The target to register. */
            virtual void Register(IMessageTarget* target) = 0;
            /** @brief Unregisters a target.
             *  @param target The target to unregister. */
            virtual void Unregister(IMessageTarget* target) = 0;
            /** @brief Registers a target for broadcast messages.
             *  @param target The target to register. */
            virtual void RegisterForBroadcasts(IMessageTargetPtr target) = 0;
            /** @brief Unregisters a target from broadcasts.
             *  @param target The target to unregister. */
            virtual void UnregisterForBroadcasts(IMessageTarget *target) = 0;
            /** @brief Blocks and dispatches messages until a timeout.
             *  @param timeoutMillis Maximum wait, in milliseconds (-1 waits forever). */
            virtual void WaitAndDispatch(int64_t timeoutMillis = -1) = 0;
            /** @brief Delivers all currently queued messages. */
            virtual void Dispatch() = 0;
    };

} } }