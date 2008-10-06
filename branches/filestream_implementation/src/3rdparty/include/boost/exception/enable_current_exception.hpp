//Copyright (c) 2006-2008 Emil Dotchevski and Reverge Studios, Inc.

//Distributed under the Boost Software License, Version 1.0. (See accompanying
//file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef UUID_78CC85B2914F11DC8F47B48E55D89593
#define UUID_78CC85B2914F11DC8F47B48E55D89593

#include <boost/exception/exception.hpp>
#include <boost/exception/detail/cloning_base.hpp>
#include <boost/detail/atomic_count.hpp>
#include <boost/assert.hpp>
#include <new>

namespace
boost
    {
    namespace
    exception_detail
        {
        class
        clone_base:
            public counted_base
            {
            public:

            virtual void rethrow() const=0;
            };

        struct
        bad_alloc_impl:
            public clone_base,
            public std::bad_alloc
            {
            void
            add_ref() const
                {
                }

            void
            release() const
                {
                }

            void
            rethrow() const
                {
                throw *this;
                }
            };

        template <class T>
        clone_base * make_clone( T const & );

        template <class T>
        class
        clone_impl:
            public T,
            public cloning_base
            {
            public:

            explicit
            clone_impl( T const & x ):
                T(x)
                {
                if( boost::exception * be1=dynamic_cast<boost::exception *>(this) )
                    if( boost::exception const * be2=dynamic_cast<boost::exception const *>(&x) )
                        *be1 = *be2;
                }

            private:

            clone_base const *
            clone() const
                {
                return make_clone<T>(*this);
                }
            };

        template <class T>
        class
        exception_clone:
            public T,
            public clone_base
            {
            public:

            explicit
            exception_clone( T const & x ):
                T(x),
                count_(0)
                {
                if( boost::exception * be1=dynamic_cast<boost::exception *>(this) )
                    if( boost::exception const * be2=dynamic_cast<boost::exception const *>(&x) )
                        *be1 = *be2;
                }

            private:

            detail::atomic_count mutable count_;

            void
            add_ref() const
                {
                ++count_;
                }

            void
            release() const
                {
                if( !--count_ )
                    delete this;
                }

            void
            rethrow() const
                {
                throw clone_impl<T>(*this);
                }
            };

        template <class T>
        inline
        clone_base *
        make_clone( T const & x )
            {
            try
                {
                return new exception_clone<T>(x);
                }
            catch(
            std::bad_alloc & )
                {
                static bad_alloc_impl bad_alloc;
                return &bad_alloc;
                }
            catch(
            ... )
                {
                BOOST_ASSERT(0);
                return 0;
                }
            }
        }

    template <class T>
    inline
    exception_detail::clone_impl<T>
    enable_current_exception( T const & x )
        {
        return exception_detail::clone_impl<T>(x);
        }
    }

#endif
