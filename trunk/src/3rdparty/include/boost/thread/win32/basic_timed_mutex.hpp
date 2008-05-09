#ifndef BOOST_BASIC_TIMED_MUTEX_WIN32_HPP
#define BOOST_BASIC_TIMED_MUTEX_WIN32_HPP

//  basic_timed_mutex_win32.hpp
//
//  (C) Copyright 2006 Anthony Williams 
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#include <boost/assert.hpp>
#include "thread_primitives.hpp"
#include "interlocked_read.hpp"
#include <boost/thread/thread_time.hpp>
#include <boost/detail/interlocked.hpp>

namespace boost
{
    namespace detail
    {
        struct basic_timed_mutex
        {
            BOOST_STATIC_CONSTANT(long,lock_flag_value=0x80000000);
            long active_count;
            void* event;

            void initialize()
            {
                active_count=0;
                event=0;
            }

            void destroy()
            {
#ifdef BOOST_MSVC
#pragma warning(push)
#pragma warning(disable:4312)
#endif
                void* const old_event=BOOST_INTERLOCKED_EXCHANGE_POINTER(&event,0);
#ifdef BOOST_MSVC
#pragma warning(pop)
#endif
                if(old_event)
                {
                    win32::CloseHandle(old_event);
                }
            }
            
          
            bool try_lock()
            {
                long old_count=active_count&~lock_flag_value;
                do
                {
                    long const current_count=BOOST_INTERLOCKED_COMPARE_EXCHANGE(&active_count,(old_count+1)|lock_flag_value,old_count);
                    if(current_count==old_count)
                    {
                        return true;
                    }
                    old_count=current_count;
                }
                while(!(old_count&lock_flag_value));
                return false;
            }
            
            void lock()
            {
                BOOST_VERIFY(timed_lock(::boost::detail::get_system_time_sentinel()));
            }
            bool timed_lock(::boost::system_time const& wait_until)
            {
                long old_count=active_count;
#ifdef BOOST_MSVC
#pragma warning(push)
#pragma warning(disable:4127)
#endif
                while(true)
#ifdef BOOST_MSVC
#pragma warning(pop)
#endif
                {
                    long const current_count=BOOST_INTERLOCKED_COMPARE_EXCHANGE(&active_count,(old_count+1)|lock_flag_value,old_count);
                    if(current_count==old_count)
                    {
                        break;
                    }
                    old_count=current_count;
                }

                if(old_count&lock_flag_value)
                {
                    bool lock_acquired=false;
                    void* const sem=get_event();
                    ++old_count; // we're waiting, too
                    do
                    {
                        old_count-=(lock_flag_value+1); // there will be one less active thread on this mutex when it gets unlocked
                        if(win32::WaitForSingleObject(sem,::boost::detail::get_milliseconds_until(wait_until))!=0)
                        {
                            BOOST_INTERLOCKED_DECREMENT(&active_count);
                            return false;
                        }
                        do
                        {
                            long const current_count=BOOST_INTERLOCKED_COMPARE_EXCHANGE(&active_count,old_count|lock_flag_value,old_count);
                            if(current_count==old_count)
                            {
                                break;
                            }
                            old_count=current_count;
                        }
                        while(!(old_count&lock_flag_value));
                        lock_acquired=!(old_count&lock_flag_value);
                    }
                    while(!lock_acquired);
                }
                return true;
            }

            template<typename Duration>
            bool timed_lock(Duration const& timeout)
            {
                return timed_lock(get_system_time()+timeout);
            }

            long get_active_count()
            {
                return ::boost::detail::interlocked_read_acquire(&active_count);
            }

            void unlock()
            {
                long const offset=lock_flag_value+1;
                long old_count=BOOST_INTERLOCKED_EXCHANGE_ADD(&active_count,(~offset)+1);
                
                if(old_count>offset)
                {
                    win32::SetEvent(get_event());
                }
            }

            bool locked()
            {
                return get_active_count()>=lock_flag_value;
            }
            
        private:
            void* get_event()
            {
                void* current_event=::boost::detail::interlocked_read_acquire(&event);
                
                if(!current_event)
                {
                    void* const new_event=win32::create_anonymous_event(win32::auto_reset_event,win32::event_initially_reset);
#ifdef BOOST_MSVC
#pragma warning(push)
#pragma warning(disable:4311)
#pragma warning(disable:4312)
#endif
                    void* const old_event=BOOST_INTERLOCKED_COMPARE_EXCHANGE_POINTER(&event,new_event,0);
#ifdef BOOST_MSVC
#pragma warning(pop)
#endif
                    if(old_event!=0)
                    {
                        win32::CloseHandle(new_event);
                        return old_event;
                    }
                    else
                    {
                        return new_event;
                    }
                }
                return current_event;
            }
            
        };
        
    }
}

#define BOOST_BASIC_TIMED_MUTEX_INITIALIZER {0}

#endif
