//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004-2019 musikcube team
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

#include "IMessageQueue.h"

#include <list>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <atomic>
#include <set>

namespace musik { namespace core { namespace runtime {
    class MessageQueue : public IMessageQueue {
        public:
            MessageQueue();
            virtual ~MessageQueue();

            virtual void Post(IMessagePtr message, int64_t delayMs = 0);
            virtual void Broadcast(IMessagePtr message, int64_t messageMs = 0);
            virtual int Remove(IMessageTarget *target, int type = -1);
            virtual bool Contains(IMessageTarget *target, int type = -1);
            virtual void Debounce(IMessagePtr message, int64_t delayMs = 0);
            virtual void RegisterForBroadcasts(IMessageTargetPtr target);
            virtual void UnregisterForBroadcasts(IMessageTarget *target);
            virtual void WaitAndDispatch(int64_t timeoutMillis = -1);
            virtual void Dispatch();

        protected:
            int64_t GetNextMessageTime() {
                return nextMessageTime.load();
            }

        private:
            typedef std::weak_ptr<IMessageTarget> IWeakMessageTarget;

            struct EnqueuedMessage {
                IMessagePtr message;
                std::chrono::milliseconds time;
            };

            struct WeakPtrLess { /* https://stackoverflow.com/a/12875729 */
                template <typename T>
                bool operator() (const std::weak_ptr<T>& l, const std::weak_ptr<T>& r) const {
                    return l.lock().get() < r.lock().get();
                }
            };

            std::mutex queueMutex;
            std::list<EnqueuedMessage*> queue;
            std::list<EnqueuedMessage*> dispatch;
            std::set<IWeakMessageTarget, WeakPtrLess> receivers;
            std::condition_variable_any waitForDispatch;
            std::atomic<int64_t> nextMessageTime;

            void Dispatch(IMessagePtr message);
    };
} } }