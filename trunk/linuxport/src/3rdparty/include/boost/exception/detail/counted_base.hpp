//Copyright (c) 2006-2008 Emil Dotchevski and Reverge Studios, Inc.

//Distributed under the Boost Software License, Version 1.0. (See accompanying
//file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef UUID_DBA0D90C930911DCBA7B675A56D89593
#define UUID_DBA0D90C930911DCBA7B675A56D89593

#include <boost/detail/workaround.hpp>

namespace
boost
    {
    namespace
    exception_detail
        {
        class
        counted_base
            {
            friend
            void
            intrusive_ptr_add_ref( counted_base const * c )
                {
                c->add_ref();
                }

            friend
            void
            intrusive_ptr_release( counted_base const * c )
                {
                c->release();
                }

            virtual void add_ref() const=0;
            virtual void release() const=0;

            protected:

#if BOOST_WORKAROUND( __GNUC__, BOOST_TESTED_AT(4) )
virtual //Disable bogus GCC warning.
#endif
            ~counted_base() throw()
                {
                }
            };
        }
    }

#endif
