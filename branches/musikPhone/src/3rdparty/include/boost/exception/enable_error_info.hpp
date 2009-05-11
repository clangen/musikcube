//Copyright (c) 2006-2008 Emil Dotchevski and Reverge Studios, Inc.

//Distributed under the Boost Software License, Version 1.0. (See accompanying
//file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef UUID_0C5D492E909711DCB658AD4556D89593
#define UUID_0C5D492E909711DCB658AD4556D89593

#include <boost/exception/exception.hpp>
#include <boost/detail/workaround.hpp>
#include <boost/config.hpp>
#include <stddef.h>

namespace
boost
    {
    namespace
    exception_detail
        {
        template <class T>
        struct
        error_info_injector:
            public T,
            public exception
            {
            explicit
            error_info_injector( T const & x ):
                T(x)
                {
                }

            ~error_info_injector() throw()
                {
                }

            char const *
            diagnostic_information() const throw()
                {
                return boost::exception::_diagnostic_information(T::what());
                }
            };

        struct large_size { char c[256]; };
        large_size dispatch( exception * );

        struct small_size { };
        small_size dispatch( void * );

        template <class,size_t>
        struct enable_error_info_helper;

        template <class T>
        struct
        enable_error_info_helper<T,sizeof(large_size)>
            {
            typedef T type;
            };

        template <class T>
        struct
        enable_error_info_helper<T,sizeof(small_size)>
            {
            typedef error_info_injector<T> type;
            };

#if BOOST_WORKAROUND(__BORLANDC__,BOOST_TESTED_AT(0x582))
        template <class T>
        struct
        sizeof_dispatch
            {
            BOOST_STATIC_CONSTANT(int, value = sizeof(dispatch((T*)0)) );
            };

        template <class T>
        struct
        enable_error_info_return_type
            {
            typedef typename enable_error_info_helper<T,sizeof_dispatch<T>::value>::type type;
            };
#else
        template <class T>
        struct
        enable_error_info_return_type
            {
            typedef typename enable_error_info_helper<T,sizeof(dispatch((T*)0))>::type type;
            };
#endif
        }

    template <class T>
    inline
#if !BOOST_WORKAROUND(__BORLANDC__,BOOST_TESTED_AT(0x582))
    typename
#endif
    exception_detail::enable_error_info_return_type<T>::type
    enable_error_info( T const & x )
        {
        return
#if !BOOST_WORKAROUND(__BORLANDC__,BOOST_TESTED_AT(0x582))
        typename
#endif
        exception_detail::enable_error_info_return_type<T>::type(x);
        }
    }

#endif
