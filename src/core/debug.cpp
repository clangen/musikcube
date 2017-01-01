//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2007-2016 musikcube team
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

#include "pch.hpp"

#include <core/debug.h>

#include <string>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <memory>

using namespace musik;

sigslot::signal3<debug::log_level, std::string, std::string> debug::string_logged;

namespace musik {
    /*
    * Basically ripped and simplified from message_queue. Log statements get
    * enqueued and processed on a background thread.
    */
    class log_queue {
    public:
        struct log_entry {
            log_entry(debug::log_level l, const std::string& t, const std::string& m) {
                level_ = l;
                tag_ = t;
                message_ = m;
            }

            debug::log_level level_;
            std::string tag_;
            std::string message_;
        };

        class stopped_exception {
        };

    private:
        std::queue<log_entry*> queue_;
        std::condition_variable wait_for_next_item_condition_;
        std::mutex queue_mutex_;
        volatile bool active_;

    public:
        log_queue() {
            active_ = true;
        }

        log_entry* pop_top() {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            while ((queue_.size() == 0) && (active_ == true)) {
                wait_for_next_item_condition_.wait(lock);
            }

            if (!active_) {
                return NULL;
            }

            log_entry* top = queue_.front();
            queue_.pop();
            return top;
        }

        bool push(log_entry* f) {
            std::unique_lock<std::mutex> lock(queue_mutex_);

            if (active_) {
                bool was_empty = (queue_.size() == 0);
                queue_.push(f);

                if (was_empty) {
                    wait_for_next_item_condition_.notify_one();
                }

                return true;
            }

            return false;
        }

        void stop() {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            active_ = false;

            while (queue_.size() > 0) {
                log_entry* top = queue_.front();
                queue_.pop();
                delete top;
            }

            wait_for_next_item_condition_.notify_all();
        }
    };
} // namespace autom8

static std::thread* thread_ = NULL;
static log_queue* queue_ = NULL;
static std::recursive_mutex system_mutex_;
static volatile bool cancel_ = true;

static void thread_proc() {
    std::string s;
    try {
        while (!cancel_) {
            log_queue::log_entry* entry = queue_->pop_top();
            if (entry) {
                debug::string_logged(entry->level_, entry->tag_, entry->message_);
                delete entry;
            }
        }
    }
    catch (log_queue::stopped_exception&) {
    }
}

void debug::init() {
    std::unique_lock<std::recursive_mutex> lock(system_mutex_);

    if (queue_ || thread_) {
        return;
    }

    deinit();

    cancel_ = false;
    queue_ = new log_queue();
    thread_ = new std::thread(std::bind(&thread_proc));
}

void debug::deinit() {
    std::unique_lock<std::recursive_mutex> lock(system_mutex_);

    cancel_ = true;

    if (thread_ && queue_) {
        /* ordering is important here... stop the queue, then
        join the thread. stop will trigger a stopped_exception the
        thread may be blocking on, waiting for the next message */
        queue_->stop();
        thread_->join();

        /* don't delete anything until the join has completed */
        delete thread_;
        thread_ = NULL;
        delete queue_;
        queue_ = NULL;
    }
}

void debug::info(const std::string& tag, const std::string& string) {
    log(debug::level_info, tag, string);
}

void debug::warn(const std::string& tag, const std::string& string) {
    log(debug::level_warning, tag, string);
}

void debug::err(const std::string& tag, const std::string& string) {
    log(debug::level_error, tag, string);
}

void debug::log(log_level level, const std::string& tag, const std::string& string) {
    std::unique_lock<std::recursive_mutex> lock(system_mutex_);

    if (queue_) {
        queue_->push(new log_queue::log_entry(level, tag, string));
    }
}

