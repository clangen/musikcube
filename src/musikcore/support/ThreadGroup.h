//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004-2021 musikcube team
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

#include <list>
#include <thread>
#include <shared_mutex>
#include <mutex>
#include <memory>

namespace musik { namespace core {

    class ThreadGroup
    {
    private:
        ThreadGroup(ThreadGroup const&);
        ThreadGroup& operator=(ThreadGroup const&);
    public:
        ThreadGroup() {}
        ~ThreadGroup()
        {
            for (std::list<std::thread*>::iterator it = threads.begin(), end = threads.end();
                it != end;
                ++it)
            {
                delete* it;
            }
        }

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

        template<typename F>
        std::thread* create_thread(F threadfunc)
        {
            std::lock_guard<std::shared_mutex> guard(m);
            std::unique_ptr<std::thread> new_thread(new std::thread(threadfunc));
            threads.push_back(new_thread.get());
            return new_thread.release();
        }

        void add_thread(std::thread* thrd)
        {
            if (thrd)
            {
                std::lock_guard<std::shared_mutex> guard(m);
                threads.push_back(thrd);
            }
        }

        void remove_thread(std::thread* thrd)
        {
            std::lock_guard<std::shared_mutex> guard(m);
            std::list<std::thread*>::iterator const it = std::find(threads.begin(), threads.end(), thrd);
            if (it != threads.end())
            {
                threads.erase(it);
            }
        }

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

        size_t size() const
        {
            std::shared_lock<std::shared_mutex> guard(m);
            return threads.size();
        }

    private:
        std::list<std::thread*> threads;
        mutable std::shared_mutex m;
    };

} }
