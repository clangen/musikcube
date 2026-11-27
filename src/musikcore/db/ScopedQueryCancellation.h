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

#include <atomic>
#include <chrono>
#include <sqlite3.h>

namespace musik { namespace core { namespace db {

    /* bound SQLite lock waits and interrupt long queries when canceled. */
    class ScopedQueryCancellation {
    public:
        ScopedQueryCancellation(sqlite3* database, const std::atomic_bool& canceled)
        : database(database)
        , canceled(canceled) {
            sqlite3_busy_handler(database, WaitForLock, this);
            sqlite3_progress_handler(database, ProgressInterval, CheckProgress, this);
        }
        ~ScopedQueryCancellation() {
            sqlite3_progress_handler(this->database, 0, nullptr, nullptr);
            sqlite3_busy_handler(this->database, nullptr, nullptr);
        }
        ScopedQueryCancellation(const ScopedQueryCancellation&) = delete;
        ScopedQueryCancellation& operator=(const ScopedQueryCancellation&) = delete;

    private:
        static constexpr int BusyTimeoutMs = 10000;
        static constexpr int BusyRetryMs = 10;
        static constexpr int ProgressInterval = 1000;

        /* SQLite invokes this for SQLITE_BUSY; short retries let cancellation
        interrupt the wait instead of blocking for the full busy timeout. */
        static int WaitForLock(void* data, int attempts) noexcept {
            auto& state = *static_cast<ScopedQueryCancellation*>(data);
            const auto now = std::chrono::steady_clock::now();
            if (!attempts) {
                state.deadline = now + std::chrono::milliseconds(BusyTimeoutMs);
            }
            if (state.canceled.load() || now >= state.deadline) {
                return 0;
            }
            sqlite3_sleep(BusyRetryMs);
            return !state.canceled.load();
        }
        static int CheckProgress(void* data) noexcept {
            return static_cast<ScopedQueryCancellation*>(data)->canceled.load();
        }

        sqlite3* const database;
        const std::atomic_bool& canceled;
        std::chrono::steady_clock::time_point deadline {};
    };

} } }