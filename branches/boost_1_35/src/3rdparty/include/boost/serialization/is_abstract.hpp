#ifndef BOOST_SERIALIZATION_IS_ABSTRACT_CLASS_HPP
#define BOOST_SERIALIZATION_IS_ABSTRACT_CLASS_HPP

// MS compatible compilers support #pragma once
#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma once
#endif

/////////1/////////2/////////3/////////4/////////5/////////6/////////7/////////8
// is_abstract_class.hpp:

// (C) Copyright 2002 Rani Sharoni (rani_sharoni@hotmail.com) and Robert Ramey
// Use, modification and distribution is subject to the Boost Software
// License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org for updates, documentation, and revision history.

#include <boost/config.hpp>
#include <boost/mpl/bool.hpp>
#include <boost/type_traits/is_abstract.hpp>

namespace boost {
namespace serialization {
    template<class T>
    struct is_abstract
#ifdef BOOST_NO_IS_ABSTRACT
      : mpl::false_
#else
      : boost::is_abstract<T>
#endif 
    {};
} // namespace serialization
} // namespace boost

// define a macro to make explicit designation of this more transparent
#define BOOST_IS_ABSTRACT(T)                          \
namespace boost {                                     \
namespace serialization {                             \
template<>                                            \
struct is_abstract< T > : mpl::true_ {};              \
}}                                                    \
/**/

#endif //BOOST_SERIALIZATION_IS_ABSTRACT_CLASS_HPP
