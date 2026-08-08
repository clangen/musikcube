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

/** @file MessageQueue.h
 *  @brief Concrete IMessageQueue implementation with delayed delivery.
 *  @details Maintains a queue of enqueued messages with their delivery times.
 *      Dispatch() delivers all messages whose time has arrived; WaitAndDispatch()
 *      blocks until the next message is due. Supports broadcasts, debouncing and
 *      removal of pending messages. */

#include "IMessageQueue.h"

#include <list>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <atomic>
#include <set>

/** @namespace musik::core::runtime
 *  @brief Inter-thread message passing: messages, targets and queues. */
namespace musik { namespace core { namespace runtime {

    /** @brief Default message queue implementation.
     *  @details Messages are stored with a delivery time. Registered targets
     *      receive directed messages; broadcast receivers get a copy of every
     *      broadcast. The queue can be driven from a dedicated thread. */
    class MessageQueue : public IMessageQueue {
        public:
            MessageQueue(const MessageQueue&) = delete;
            MessageQueue(const MessageQueue&&) = delete;
            MessageQueue& operator=(const MessageQueue&) = delete;
            MessageQueue& operator=(const MessageQueue&&) = delete;

            /** @brief Creates an empty message queue. */
            MessageQueue() noexcept;

            /** @brief Posts a message for delivery, optionally after a delay.
             *  @param message The message to deliver.
             *  @param delayMs Delay before delivery, in milliseconds. */
            void Post(IMessagePtr message, int64_t delayMs = 0) override;
            /** @brief Posts a message to every broadcast receiver.
             *  @param message The message to broadcast.
             *  @param messageMs Delay before delivery, in milliseconds. */
            void Broadcast(IMessagePtr message, int64_t messageMs = 0) override;
            /** @brief Removes queued messages for a target.
             *  @param target The target to remove messages for.
             *  @param type Optional message type filter (-1 for all).
             *  @return The number of messages removed. */
            int Remove(IMessageTarget *target, int type = -1) override;
            /** @return true if a message for the target (and type) is queued.
             *  @param target The target to check.
             *  @param type Optional message type filter (-1 for any). */
            bool Contains(IMessageTarget *target, int type = -1) override;
            /** @brief Posts a message, first removing prior messages of the same type.
             *  @param message The message to deliver.
             *  @param delayMs Delay before delivery, in milliseconds. */
            void Debounce(IMessagePtr message, int64_t delayMs = 0) override;
            /** @brief Registers a target that can receive messages.
             *  @param target The target to register. */
            void Register(IMessageTarget* target) override;
            /** @brief Unregisters a target.
             *  @param target The target to unregister. */
            void Unregister(IMessageTarget* target) override;
            /** @brief Registers a target for broadcast messages.
             *  @param target The target to register. */
            void RegisterForBroadcasts(IMessageTargetPtr target) override;
            /** @brief Unregisters a target from broadcasts.
             *  @param target The target to unregister. */
            void UnregisterForBroadcasts(IMessageTarget *target) override;
            /** @brief Blocks and dispatches messages until the next is due or a timeout.
             *  @param timeoutMillis Maximum wait, in milliseconds (-1 waits forever). */
            void WaitAndDispatch(int64_t timeoutMillis = -1) override;
            /** @brief Delivers all currently due messages. */
            void Dispatch() override;

        protected:
            /** @return The delivery time of the next pending message. */
            int64_t GetNextMessageTime() noexcept {
                return nextMessageTime.load();
            }

        private:
            typedef std::weak_ptr<IMessageTarget> IWeakMessageTarget; /**< Weak broadcast receiver alias. */

            /** @brief Adds a message to the queue with a delivery delay.
             *  @param message The message to enqueue.
             *  @param delayMs Delay before delivery, in milliseconds. */
            void Enqueue(IMessagePtr message, int64_t delayMs);
            /** @brief Delivers a single message to its target(s).
             *  @param message The message to dispatch. */
            void Dispatch(IMessage* message);

            /** @brief A queued message and its delivery time. */
            struct EnqueuedMessage {
                IMessagePtr message; /**< The message. */
                std::chrono::milliseconds time; /**< Delivery time. */
            };

            /** @brief Orders weak pointers by their locked raw pointer. */
            struct WeakPtrLess { /* https://stackoverflow.com/a/12875729 */
                template <typename T>
                bool operator() (const std::weak_ptr<T>& l, const std::weak_ptr<T>& r) const {
                    return l.lock().get() < r.lock().get();
                }
            };

            std::mutex queueMutex; /**< Guards the queues and receivers. */
            std::list<EnqueuedMessage*> queue; /**< Pending messages. */
            std::list<EnqueuedMessage*> dispatch; /**< Messages currently being dispatched. */
            std::set<IWeakMessageTarget, WeakPtrLess> receivers; /**< Broadcast receivers. */
            std::set<IMessageTarget*> targets; /**< Directed message targets. */
            std::condition_variable_any waitForDispatch; /**< Signals the dispatch thread. */
            std::atomic<int64_t> nextMessageTime; /**< Next message delivery time. */
    };

} } }