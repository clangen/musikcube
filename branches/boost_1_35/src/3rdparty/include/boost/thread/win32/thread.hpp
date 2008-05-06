#ifndef BOOST_THREAD_THREAD_WIN32_HPP
#define BOOST_THREAD_THREAD_WIN32_HPP
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
// (C) Copyright 2007 Anthony Williams

#include <exception>
#include <boost/thread/exceptions.hpp>
#include <ostream>
#include <boost/thread/detail/move.hpp>
#include <boost/intrusive_ptr.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/thread_time.hpp>
#include "thread_primitives.hpp"
#include "thread_heap_alloc.hpp"
#include <boost/utility.hpp>
#include <boost/assert.hpp>
#include <list>
#include <algorithm>
#include <boost/ref.hpp>
#include <boost/cstdint.hpp>
#include <stdlib.h>
#include <memory>

#ifdef BOOST_MSVC
#pragma warning(push)
#pragma warning(disable:4251)
#endif

namespace boost
{
    class thread_interrupted
    {};

    namespace detail
    {
        struct thread_exit_callback_node;
        struct tss_data_node;
        
        struct thread_data_base
        {
            long count;
            detail::win32::handle_manager thread_handle;
            detail::win32::handle_manager interruption_handle;
            boost::detail::thread_exit_callback_node* thread_exit_callbacks;
            boost::detail::tss_data_node* tss_data;
            bool interruption_enabled;
            unsigned id;

            thread_data_base():
                count(0),thread_handle(detail::win32::invalid_handle_value),
                interruption_handle(create_anonymous_event(detail::win32::manual_reset_event,detail::win32::event_initially_reset)),
                thread_exit_callbacks(0),tss_data(0),
                interruption_enabled(true),
                id(0)
            {}
            virtual ~thread_data_base()
            {}

            friend void intrusive_ptr_add_ref(thread_data_base * p)
            {
                BOOST_INTERLOCKED_INCREMENT(&p->count);
            }
            
            friend void intrusive_ptr_release(thread_data_base * p)
            {
                if(!BOOST_INTERLOCKED_DECREMENT(&p->count))
                {
                    detail::heap_delete(p);
                }
            }

            void interrupt()
            {
                BOOST_VERIFY(detail::win32::SetEvent(interruption_handle)!=0);
            }
            

            virtual void run()=0;
        };

        typedef boost::intrusive_ptr<detail::thread_data_base> thread_data_ptr;

        struct timeout
        {
            unsigned long start;
            uintmax_t milliseconds;
            bool relative;
            boost::system_time abs_time;

            static unsigned long const max_non_infinite_wait=0xfffffffe;

            timeout(uintmax_t milliseconds_):
                start(win32::GetTickCount()),
                milliseconds(milliseconds_),
                relative(true),
                abs_time(boost::get_system_time())
            {}

            timeout(boost::system_time const& abs_time_):
                start(win32::GetTickCount()),
                milliseconds(0),
                relative(false),
                abs_time(abs_time_)
            {}

            struct remaining_time
            {
                bool more;
                unsigned long milliseconds;

                remaining_time(uintmax_t remaining):
                    more(remaining>max_non_infinite_wait),
                    milliseconds(more?max_non_infinite_wait:(unsigned long)remaining)
                {}
            };

            remaining_time remaining_milliseconds() const
            {
                if(is_sentinel())
                {
                    return remaining_time(win32::infinite);
                }
                else if(relative)
                {
                    unsigned long const now=win32::GetTickCount();
                    unsigned long const elapsed=now-start;
                    return remaining_time((elapsed<milliseconds)?(milliseconds-elapsed):0);
                }
                else
                {
                    system_time const now=get_system_time();
                    if(abs_time<=now)
                    {
                        return remaining_time(0);
                    }
                    return remaining_time((abs_time-now).total_milliseconds()+1);
                }
            }

            bool is_sentinel() const
            {
                return milliseconds==~uintmax_t(0);
            }
            

            static timeout sentinel()
            {
                return timeout(sentinel_type());
            }
        private:
            struct sentinel_type
            {};
                
            explicit timeout(sentinel_type):
                start(0),milliseconds(~uintmax_t(0)),relative(true)
            {}
        };
    }

    class BOOST_THREAD_DECL thread
    {
    private:
        thread(thread&);
        thread& operator=(thread&);

        void release_handle();

        template<typename F>
        struct thread_data:
            detail::thread_data_base
        {
            F f;

            thread_data(F f_):
                f(f_)
            {}
            thread_data(detail::thread_move_t<F> f_):
                f(f_)
            {}
            
            void run()
            {
                f();
            }
        };
        
        mutable boost::mutex thread_info_mutex;
        detail::thread_data_ptr thread_info;

        static unsigned __stdcall thread_start_function(void* param);

        void start_thread();
        
        explicit thread(detail::thread_data_ptr data);

        detail::thread_data_ptr get_thread_info() const;
    public:
        thread();
        ~thread();

        template <class F>
        explicit thread(F f):
            thread_info(detail::heap_new<thread_data<F> >(f))
        {
            start_thread();
        }
        template <class F>
        thread(detail::thread_move_t<F> f):
            thread_info(detail::heap_new<thread_data<F> >(f))
        {
            start_thread();
        }

        thread(detail::thread_move_t<thread> x);
        thread& operator=(detail::thread_move_t<thread> x);
        operator detail::thread_move_t<thread>();
        detail::thread_move_t<thread> move();

        void swap(thread& x);

        class id;
        id get_id() const;


        bool joinable() const;
        void join();
        bool timed_join(const system_time& wait_until);

        template<typename TimeDuration>
        inline bool timed_join(TimeDuration const& rel_time)
        {
            return timed_join(get_system_time()+rel_time);
        }
        void detach();

        static unsigned hardware_concurrency();

        typedef detail::win32::handle native_handle_type;
        native_handle_type native_handle();

        // backwards compatibility
        bool operator==(const thread& other) const;
        bool operator!=(const thread& other) const;

        static void yield();
        static void sleep(const system_time& xt);

        // extensions
        void interrupt();
        bool interruption_requested() const;
    };

    inline detail::thread_move_t<thread> move(thread& x)
    {
        return x.move();
    }
    
    inline detail::thread_move_t<thread> move(detail::thread_move_t<thread> x)
    {
        return x;
    }

    template<typename F>
    struct thread::thread_data<boost::reference_wrapper<F> >:
        detail::thread_data_base
    {
        F& f;
        
        thread_data(boost::reference_wrapper<F> f_):
            f(f_)
        {}
        
        void run()
        {
            f();
        }
    };
    

    namespace this_thread
    {
        class BOOST_THREAD_DECL disable_interruption
        {
            disable_interruption(const disable_interruption&);
            disable_interruption& operator=(const disable_interruption&);
            
            bool interruption_was_enabled;
            friend class restore_interruption;
        public:
            disable_interruption();
            ~disable_interruption();
        };

        class BOOST_THREAD_DECL restore_interruption
        {
            restore_interruption(const restore_interruption&);
            restore_interruption& operator=(const restore_interruption&);
        public:
            explicit restore_interruption(disable_interruption& d);
            ~restore_interruption();
        };

        thread::id BOOST_THREAD_DECL get_id();

        bool BOOST_THREAD_DECL interruptible_wait(detail::win32::handle handle_to_wait_for,detail::timeout target_time);
        inline bool interruptible_wait(unsigned long milliseconds)
        {
            return interruptible_wait(detail::win32::invalid_handle_value,milliseconds);
        }

        void BOOST_THREAD_DECL interruption_point();
        bool BOOST_THREAD_DECL interruption_enabled();
        bool BOOST_THREAD_DECL interruption_requested();

        void BOOST_THREAD_DECL yield();
        template<typename TimeDuration>
        void sleep(TimeDuration const& rel_time)
        {
            interruptible_wait(static_cast<unsigned long>(rel_time.total_milliseconds()));
        }
    }

    class thread::id
    {
    private:
        detail::thread_data_ptr thread_data;
            
        id(detail::thread_data_ptr thread_data_):
            thread_data(thread_data_)
        {}
        friend class thread;
        friend id this_thread::get_id();
    public:
        id():
            thread_data(0)
        {}
            
        bool operator==(const id& y) const
        {
            return thread_data==y.thread_data;
        }
        
        bool operator!=(const id& y) const
        {
            return thread_data!=y.thread_data;
        }
        
        bool operator<(const id& y) const
        {
            return thread_data<y.thread_data;
        }
        
        bool operator>(const id& y) const
        {
            return y.thread_data<thread_data;
        }
        
        bool operator<=(const id& y) const
        {
            return !(y.thread_data<thread_data);
        }
        
        bool operator>=(const id& y) const
        {
            return !(thread_data<y.thread_data);
        }

        template<class charT, class traits>
        friend std::basic_ostream<charT, traits>& 
        operator<<(std::basic_ostream<charT, traits>& os, const id& x)
        {
            if(x.thread_data)
            {
                return os<<x.thread_data;
            }
            else
            {
                return os<<"{Not-any-thread}";
            }
        }

        void interrupt()
        {
            if(thread_data)
            {
                thread_data->interrupt();
            }
        }
        
    };

    inline bool thread::operator==(const thread& other) const
    {
        return get_id()==other.get_id();
    }
    
    inline bool thread::operator!=(const thread& other) const
    {
        return get_id()!=other.get_id();
    }
        
    namespace detail
    {
        struct thread_exit_function_base
        {
            virtual ~thread_exit_function_base()
            {}
            virtual void operator()() const=0;
        };
        
        template<typename F>
        struct thread_exit_function:
            thread_exit_function_base
        {
            F f;
            
            thread_exit_function(F f_):
                f(f_)
            {}
            
            void operator()() const
            {
                f();
            }
        };
        
        void add_thread_exit_function(thread_exit_function_base*);
    }
    
    namespace this_thread
    {
        template<typename F>
        void at_thread_exit(F f)
        {
            detail::thread_exit_function_base* const thread_exit_func=detail::heap_new<detail::thread_exit_function<F> >(f);
            detail::add_thread_exit_function(thread_exit_func);
        }
    }

    class thread_group:
        private noncopyable
    {
    public:
        ~thread_group()
        {
            for(std::list<thread*>::iterator it=threads.begin(),end=threads.end();
                it!=end;
                ++it)
            {
                delete *it;
            }
        }

        template<typename F>
        thread* create_thread(F threadfunc)
        {
            boost::lock_guard<mutex> guard(m);
            std::auto_ptr<thread> new_thread(new thread(threadfunc));
            threads.push_back(new_thread.get());
            return new_thread.release();
        }
        
        void add_thread(thread* thrd)
        {
            if(thrd)
            {
                boost::lock_guard<mutex> guard(m);
                threads.push_back(thrd);
            }
        }
            
        void remove_thread(thread* thrd)
        {
            boost::lock_guard<mutex> guard(m);
            std::list<thread*>::iterator const it=std::find(threads.begin(),threads.end(),thrd);
            if(it!=threads.end())
            {
                threads.erase(it);
            }
        }
        
        void join_all()
        {
            boost::lock_guard<mutex> guard(m);
            
            for(std::list<thread*>::iterator it=threads.begin(),end=threads.end();
                it!=end;
                ++it)
            {
                (*it)->join();
            }
        }
        
        void interrupt_all()
        {
            boost::lock_guard<mutex> guard(m);
            
            for(std::list<thread*>::iterator it=threads.begin(),end=threads.end();
                it!=end;
                ++it)
            {
                (*it)->interrupt();
            }
        }
        
        size_t size() const
        {
            boost::lock_guard<mutex> guard(m);
            return threads.size();
        }
        
    private:
        std::list<thread*> threads;
        mutable mutex m;
    };
}

#ifdef BOOST_MSVC
#pragma warning(pop)
#endif

#endif
