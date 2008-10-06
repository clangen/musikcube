//Copyright (c) 2006-2008 Emil Dotchevski and Reverge Studios, Inc.

//Distributed under the Boost Software License, Version 1.0. (See accompanying
//file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef UUID_274DA366004E11DCB1DDFE2E56D89593
#define UUID_274DA366004E11DCB1DDFE2E56D89593

#include <boost/config.hpp>
#include <boost/detail/workaround.hpp>
#include <boost/exception/detail/counted_base.hpp>
#include <boost/intrusive_ptr.hpp>
#include <typeinfo>

namespace
boost
    {
    template <class T>
    class shared_ptr;

    namespace
    exception_detail
        {
        class error_info_base;

        struct
        error_info_container:
            public exception_detail::counted_base
            {
            virtual char const * diagnostic_information( char const *, std::type_info const & ) const = 0;
            virtual shared_ptr<error_info_base const> get( std::type_info const & ) const = 0;
            virtual void set( shared_ptr<error_info_base const> const & ) = 0;
            };
        }

    template <class Tag,class T>
    class error_info;

    template <class E,class Tag,class T>
    E const & operator<<( E const &, error_info<Tag,T> const & );

    template <class ErrorInfo,class E>
    shared_ptr<typename ErrorInfo::value_type const> get_error_info( E const & );

    class
    exception
        {
        public:

        virtual
        char const *
        diagnostic_information() const throw()
            {
            return _diagnostic_information(0);
            }

        protected:

        exception()
            {
            }

        exception( exception const & e ):
            data_(e.data_)
            {
            }

        char const *
        _diagnostic_information( char const * std_what ) const throw()
            {
            if( data_ )
                try
                    {
                    char const * w = data_->diagnostic_information(std_what,typeid(*this));
                    BOOST_ASSERT(0!=w);
                    return w;
                    }
                catch(...)
                    {
                    }
            return std_what ? std_what : typeid(*this).name();
            }

#if BOOST_WORKAROUND( BOOST_MSVC, BOOST_TESTED_AT(1500) )
        //Force class exception to be abstract.
        //Otherwise, MSVC bug allows throw exception(), even though the copy constructor is protected.
        virtual ~exception() throw()=0;
#else
#if BOOST_WORKAROUND( __GNUC__, BOOST_TESTED_AT(4) )
        virtual //Disable bogus GCC warning.
#endif
        ~exception() throw()
            {
            }
#endif

        private:

        shared_ptr<exception_detail::error_info_base const> get( std::type_info const & ) const;
        void set( shared_ptr<exception_detail::error_info_base const> const & ) const;

        template <class E,class Tag,class T>
        friend E const & operator<<( E const &, error_info<Tag,T> const & );

        template <class ErrorInfo,class E>
        friend shared_ptr<typename ErrorInfo::value_type const> get_error_info( E const & );

        intrusive_ptr<exception_detail::error_info_container> mutable data_;
        };

#if BOOST_WORKAROUND( BOOST_MSVC, BOOST_TESTED_AT(1500) ) //See above.
    inline
    exception::
    ~exception() throw()
        {
        }
#endif
    }

#endif
