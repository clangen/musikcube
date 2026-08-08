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

/* copied from boost's ThreadGroup.hpp */

#pragma once

/** @file ThreadGroup.h
 *  @brief A thread group that tracks and joins owned threads.
 *  @details Copied from Boost's ThreadGroup: manages a list of threads, supports
 *      membership queries, and joins all threads on destruction or join_all().
 *      Threads are deleted (not detached) by the group. */

#include <list>
#include <thread>
#include <shared_mutex>
#include <mutex>
#include <memory>

/** @namespace musik::core
 *  @brief Core application services: libraries, indexing, playback and utilities. */
namespace musik { namespace core {

    /** @brief Owns a collection of threads and joins them together.
     *  @details Threads added to the group are owned by it; the destructor deletes
     *      any not yet joined. All operations are guarded by a shared mutex. */
    class ThreadGroup
    {
    private:
        /** @brief Non-copyable.
         *  @param other The group to copy (deleted). */
        ThreadGroup(ThreadGroup const&);
        /** @brief Non-assignable.
         *  @param other The group to assign (deleted). */
        ThreadGroup& operator=(ThreadGroup const&);
    public:
        /** @brief Creates an empty thread group. */
        ThreadGroup() {}
        /** @brief Deletes any threads not joined by the destructor. */
        ~ThreadGroup()
        {
            for (std::list<std::thread*>::iterator it = threads.begin(), end = threads.end();
                it != end;
                ++it)
            {
                delete* it;
            }
        }

        /** @return true if the calling thread belongs to the group. */
        bool is_this_thread_in()
        {
            std::thread::id id = std::this_thread::get_id();
            std::shared_lock<std::shared_mutex> guard(m);
            for (std::list<std::thread*>::iterator it = threads.begin(), end = threads.end();
                it != end;
                ++it)
            {
                if ((*it)->get_id() == id)
                    return true;
            }
            return false;
        }

        /** @return true if the given thread belongs to the group.
         *  @param thrd The thread to check. */
        bool is_thread_in(std::thread* thrd)
        {
            if (thrd)
            {
                std::thread::id id = thrd->get_id();
                std::shared_lock<std::shared_mutex> guard(m);
                for (std::list<std::thread*>::iterator it = threads.begin(), end = threads.end();
                    it != end;
                    ++it)
                {
                    if ((*it)->get_id() == id)
                        return true;
                }
                return false;
            }
            else
            {
                return false;
            }
        }

        /** @brief Creates and adds a new thread to the group.
         *  @tparam F The callable type.
         *  @param threadfunc The thread function.
         *  @return The created thread (owned by the group). */
        template<typename F>
        std::thread* create_thread(F threadfunc)
        {
            std::lock_guard<std::shared_mutex> guard(m);
            std::unique_ptr<std::thread> new_thread(new std::thread(threadfunc));
            threads.push_back(new_thread.get());
            return new_thread.release();
        }

        /** @brief Adds an existing thread to the group.
         *  @param thrd The thread to add. */
        void add_thread(std::thread* thrd)
        {
            if (thrd)
            {
                std::lock_guard<std::shared_mutex> guard(m);
                threads.push_back(thrd);
            }
        }

        /** @brief Removes a thread from the group without joining it.
         *  @param thrd The thread to remove. */
        void remove_thread(std::thread* thrd)
        {
            std::lock_guard<std::shared_mutex> guard(m);
            std::list<std::thread*>::iterator const it = std::find(threads.begin(), threads.end(), thrd);
            if (it != threads.end())
            {
                threads.erase(it);
            }
        }

        /** @brief Joins every joinable thread in the group. */
        void join_all()
        {
            std::shared_lock<std::shared_mutex> guard(m);

            for (std::list<std::thread*>::iterator it = threads.begin(), end = threads.end();
                it != end;
                ++it)
            {
                if ((*it)->joinable())
                    (*it)->join();
            }
        }

        /** @return The number of threads in the group. */
        size_t size() const
        {
            std::shared_lock<std::shared_mutex> guard(m);
            return threads.size();
        }

    private:
        std::list<std::thread*> threads; /**< Owned threads. */
        mutable std::shared_mutex m; /**< Guards the thread list. */
    };

} }
