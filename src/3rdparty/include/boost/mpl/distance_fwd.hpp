
#ifndef BOOST_MPL_DISTANCE_FWD_HPP_INCLUDED
#define BOOST_MPL_DISTANCE_FWD_HPP_INCLUDED

// Copyright Aleksey Gurtovoy 2000-2004
//
// Distributed under the Boost Software License, Version 1.0. 
// (See accompanying file LICENSE_1_0.txt or copy at 
// http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/mpl for documentation.

// $Source$
// $Date: 2004-11-27 20:58:27 -0500 (Sat, 27 Nov 2004) $
// $Revision: 26325 $

#include <boost/mpl/aux_/common_name_wknd.hpp>

namespace boost { namespace mpl {

BOOST_MPL_AUX_COMMON_NAME_WKND(distance)

template< typename Tag > struct distance_impl;
template< typename First, typename Last > struct distance;

}}

#endif // BOOST_MPL_DISTANCE_FWD_HPP_INCLUDED
