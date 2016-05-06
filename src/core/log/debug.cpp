#include "pch.hpp"

#include <core/log/debug.h>

#include <string>
#include <queue>
#include <boost/thread.hpp>
#include <boost/shared_ptr.hpp>

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
        boost::condition_variable wait_for_next_item_condition_;
        boost::mutex queue_mutex_;
        volatile bool active_;

    public:
        log_queue() {
            active_ = true;
        }

        log_entry* pop_top() {
            boost::mutex::scoped_lock lock(queue_mutex_);
            while ((queue_.size() == 0) && (active_ == true)) {
                wait_for_next_item_condition_.wait(lock);
            }

            if (!active_) {
                throw stopped_exception();
            }

            log_entry* top = queue_.front();
            queue_.pop();
            return top;
        }

        bool push(log_entry* f) {
            boost::mutex::scoped_lock lock(queue_mutex_);

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
            boost::mutex::scoped_lock lock(queue_mutex_);
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

static boost::thread* thread_ = NULL;
static log_queue* queue_ = NULL;
static boost::recursive_mutex system_mutex_;
static volatile bool cancel_ = true;

static void thread_proc() {
    std::string s;
    try {
        while (!cancel_) {
            log_queue::log_entry* entry = queue_->pop_top();
            debug::string_logged(entry->level_, entry->tag_, entry->message_);
            delete entry;
        }
    }
    catch (log_queue::stopped_exception&) {
    }
}

void debug::init() {
    boost::recursive_mutex::scoped_lock lock(system_mutex_);

    if (queue_ || thread_) {
        return;
    }

    deinit();

    cancel_ = false;
    queue_ = new log_queue();
    thread_ = new boost::thread(boost::bind(&thread_proc));
}

void debug::deinit() {
    boost::recursive_mutex::scoped_lock lock(system_mutex_);

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

void debug::log(const std::string& tag, const std::string& string) {
    log(debug::info, tag, string);
}

void debug::log(log_level level, const std::string& tag, const std::string& string) {
    boost::recursive_mutex::scoped_lock lock(system_mutex_);

    if (queue_) {
        queue_->push(new log_queue::log_entry(level, tag, string));
    }
}

