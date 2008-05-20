// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
// (C) Copyright 2007 Anthony Williams
#ifndef THREAD_HEAP_ALLOC_HPP
#define THREAD_HEAP_ALLOC_HPP
#include <new>
#include "thread_primitives.hpp"
#include <stdexcept>
#include <boost/assert.hpp>

#if defined( BOOST_USE_WINDOWS_H )
# include <windows.h>

namespace boost
{
    namespace detail
    {
        namespace win32
        {
            using ::GetProcessHeap;
            using ::HeapAlloc;
            using ::HeapFree;
        }
    }
}

#else

# ifdef HeapAlloc
# undef HeapAlloc
# endif

namespace boost
{
    namespace detail
    {
        namespace win32
        {
            extern "C"
            {
                __declspec(dllimport) handle __stdcall GetProcessHeap();
                __declspec(dllimport) void* __stdcall HeapAlloc(handle,unsigned long,ulong_ptr);
                __declspec(dllimport) int __stdcall HeapFree(handle,unsigned long,void*);
            }
        }
    }
}

#endif

namespace boost
{
    namespace detail
    {
        inline BOOST_THREAD_DECL void* allocate_raw_heap_memory(unsigned size)
        {
            void* const heap_memory=detail::win32::HeapAlloc(detail::win32::GetProcessHeap(),0,size);
            if(!heap_memory)
            {
                throw std::bad_alloc();
            }
            return heap_memory;
        }

        inline BOOST_THREAD_DECL void free_raw_heap_memory(void* heap_memory)
        {
            BOOST_VERIFY(detail::win32::HeapFree(detail::win32::GetProcessHeap(),0,heap_memory)!=0);
        }
            
        template<typename T>
        T* heap_new()
        {
            void* const heap_memory=allocate_raw_heap_memory(sizeof(T));
            try
            {
                T* const data=new (heap_memory) T();
                return data;
            }
            catch(...)
            {
                free_raw_heap_memory(heap_memory);
                throw;
            }
        }

        template<typename T,typename A1>
        T* heap_new(A1 a1)
        {
            void* const heap_memory=allocate_raw_heap_memory(sizeof(T));
            try
            {
                T* const data=new (heap_memory) T(a1);
                return data;
            }
            catch(...)
            {
                free_raw_heap_memory(heap_memory);
                throw;
            }
        }
        
        template<typename T,typename A1,typename A2>
        T* heap_new(A1 a1,A2 a2)
        {
            void* const heap_memory=allocate_raw_heap_memory(sizeof(T));
            try
            {
                T* const data=new (heap_memory) T(a1,a2);
                return data;
            }
            catch(...)
            {
                free_raw_heap_memory(heap_memory);
                throw;
            }
        }

        template<typename T,typename A1,typename A2,typename A3>
        T* heap_new(A1 a1,A2 a2,A3 a3)
        {
            void* const heap_memory=allocate_raw_heap_memory(sizeof(T));
            try
            {
                T* const data=new (heap_memory) T(a1,a2,a3);
                return data;
            }
            catch(...)
            {
                free_raw_heap_memory(heap_memory);
                throw;
            }
        }
        
        template<typename T,typename A1,typename A2,typename A3,typename A4>
        T* heap_new(A1 a1,A2 a2,A3 a3,A4 a4)
        {
            void* const heap_memory=allocate_raw_heap_memory(sizeof(T));
            try
            {
                T* const data=new (heap_memory) T(a1,a2,a3,a4);
                return data;
            }
            catch(...)
            {
                free_raw_heap_memory(heap_memory);
                throw;
            }
        }
        
        template<typename T>
        void heap_delete(T* data)
        {
            data->~T();
            free_raw_heap_memory(data);
        }

        template<typename T>
        struct do_heap_delete
        {
            void operator()(T* data) const
            {
                detail::heap_delete(data);
            }
        };
    }
}


#endif
