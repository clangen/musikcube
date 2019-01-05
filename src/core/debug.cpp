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

#include <pch.hpp>
#include <core/debug.h>
#include <core/support/Common.h>
#include <functional>
#include <string>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <iostream>
#include <time.h>

using namespace musik::core;

class log_queue;

static std::vector<std::unique_ptr<musik::debug::IBackend>> backends;
static std::thread* thread = nullptr;
static log_queue* queue = nullptr;
static std::recursive_mutex mutex;
static volatile bool cancel = true;

enum class debug_level {
    verbose = 0,
    info = 1,
    warning = 2,
    error = 3
};

class log_queue {
    public:
        struct log_entry {
            log_entry(debug_level l, const std::string& t, const std::string& m) {
                level = l;
                tag = t;
                message = m;
            }

            debug_level level;
            std::string tag;
            std::string message;
        };

        class stopped_exception {
        };

        log_queue() {
            active = true;
        }

        log_entry* pop_top() {
            std::unique_lock<std::mutex> lock(queue_mutex);
            while ((queue.size() == 0) && (active == true)) {
                wait_for_next_item_condition.wait(lock);
            }

            if (!active) {
                return nullptr;
            }

            log_entry* top = queue.front();
            queue.pop();
            return top;
        }

        bool push(log_entry* f) {
            std::unique_lock<std::mutex> lock(queue_mutex);

            if (active) {
                bool was_empty = (queue.size() == 0);
                queue.push(f);

                if (was_empty) {
                    wait_for_next_item_condition.notify_one();
                }

                return true;
            }

            return false;
        }

        void stop() {
            std::unique_lock<std::mutex> lock(queue_mutex);
            active = false;

            while (queue.size() > 0) {
                log_entry* top = queue.front();
                queue.pop();
                delete top;
            }

            wait_for_next_item_condition.notify_all();
        }

    private:
        std::queue<log_entry*> queue;
        std::condition_variable wait_for_next_item_condition;
        std::mutex queue_mutex;
        volatile bool active;
};

static void thread_proc() {
    try {
        while (!cancel) {
            log_queue::log_entry* entry = queue->pop_top();
            if (entry) {
                for (auto& backend : backends) {
                    switch (entry->level) {
                        case debug_level::verbose:
                            backend->verbose(entry->tag, entry->message);
                            break;
                        case debug_level::info:
                            backend->info(entry->tag, entry->message);
                            break;
                        case debug_level::warning:
                            backend->warning(entry->tag, entry->message);
                            break;
                        case debug_level::error:
                            backend->error(entry->tag, entry->message);
                            break;
                    }
                }
                delete entry;
            }
        }
    }
    catch (log_queue::stopped_exception&) {
    }
}

void musik::debug::Start(std::vector<musik::debug::IBackend*> backends) {
    std::unique_lock<std::recursive_mutex> lock(mutex);

    if (queue || thread) {
        return;
    }

    for (auto backend : backends) {
        ::backends.push_back(std::unique_ptr<musik::debug::IBackend>(backend));
    }

    cancel = false;
    queue = new log_queue();
    thread = new std::thread(std::bind(&thread_proc));

    info("LOG SESSION", "---------- START ----------");
}

void musik::debug::Stop() {
    std::unique_lock<std::recursive_mutex> lock(mutex);

    cancel = true;

    if (thread && queue) {
        queue->stop();
        thread->join();

        delete thread;
        thread = nullptr;
        delete queue;
        queue = nullptr;
    }
}

static void enqueue(debug_level level, const std::string& tag, const std::string& string) {
    std::unique_lock<std::recursive_mutex> lock(mutex);

    if (queue) {
        queue->push(new log_queue::log_entry(level, tag, string));
    }
}

void musik::debug::verbose(const std::string& tag, const std::string& string) {
    enqueue(debug_level::verbose, tag, string);
}

void musik::debug::v(const std::string& tag, const std::string& string) {
    enqueue(debug_level::verbose, tag, string);
}

void musik::debug::info(const std::string& tag, const std::string& string) {
    enqueue(debug_level::info, tag, string);
}

void musik::debug::i(const std::string& tag, const std::string& string) {
    enqueue(debug_level::info, tag, string);
}

void musik::debug::warning(const std::string& tag, const std::string& string) {
    enqueue(debug_level::warning, tag, string);
}

void musik::debug::w(const std::string& tag, const std::string& string) {
    enqueue(debug_level::warning, tag, string);
}

void musik::debug::error(const std::string& tag, const std::string& string) {
    enqueue(debug_level::error, tag, string);
}

void musik::debug::e(const std::string& tag, const std::string& string) {
    enqueue(debug_level::error, tag, string);
}

////////// backend utils //////////

namespace musik {

    static std::string timestamp() {
        time_t rawtime = { 0 };
        struct tm * timeinfo;
        char buffer [64];
        time(&rawtime);
        timeinfo = localtime (&rawtime);
        strftime (buffer, sizeof(buffer), "%T", timeinfo);
        return std::string(buffer);
    }

    static void writeTo(
        std::ostream& out,
        const std::string& level,
        const std::string& tag,
        const std::string& message)
    {
        out << timestamp() << " [" << level << "] [" << tag << "] " << message << "\n";
        out.flush();
    }

}

////////// FileBackend //////////

namespace musik {

    debug::FileBackend::FileBackend(const std::string& fn)
    : out(fn.c_str()) {

    }

    debug::FileBackend::FileBackend(FileBackend&& fn) {
        this->out.swap(fn.out);
    }

    debug::FileBackend::~FileBackend() {
    }

    void debug::FileBackend::verbose(const std::string& tag, const std::string& string) {
        writeTo(this->out, "verbose", tag, string);
    }

    void debug::FileBackend::info(const std::string& tag, const std::string& string) {
        writeTo(this->out, "info", tag, string);
    }

    void debug::FileBackend::warning(const std::string& tag, const std::string& string) {
        writeTo(this->out, "warning", tag, string);
    }

    void debug::FileBackend::error(const std::string& tag, const std::string& string) {
        writeTo(this->out, "error", tag, string);
    }

}

////////// SimpleFileBackend //////////

namespace musik {

    debug::SimpleFileBackend::SimpleFileBackend()
    : FileBackend(GetDataDirectory() + "log.txt") {
    }

}

////////// ConsoleBackend //////////

namespace musik {

    debug::ConsoleBackend::ConsoleBackend() {
    }

    debug::ConsoleBackend::~ConsoleBackend() {
    }

    void debug::ConsoleBackend::verbose(const std::string& tag, const std::string& string) {
        writeTo(std::cout, "verbose", tag, string);
    }

    void debug::ConsoleBackend::info(const std::string& tag, const std::string& string) {
        writeTo(std::cout, "info", tag, string);
    }

    void debug::ConsoleBackend::warning(const std::string& tag, const std::string& string) {
        writeTo(std::cout, "warning", tag, string);
    }

    void debug::ConsoleBackend::error(const std::string& tag, const std::string& string) {
        writeTo(std::cerr, "error", tag, string);
    }

}
