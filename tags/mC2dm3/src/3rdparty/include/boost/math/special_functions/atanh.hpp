//    boost atanh.hpp header file

//  (C) Copyright Hubert Holin 2001.
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

// See http://www.boost.org for updates, documentation, and revision history.

#ifndef BOOST_ATANH_HPP
#define BOOST_ATANH_HPP

#ifdef _MSC_VER
#pragma once
#endif


#include <cmath>
#include <boost/config.hpp>
#include <boost/math/tools/precision.hpp>
#include <boost/math/policies/error_handling.hpp>
#include <boost/math/special_functions/math_fwd.hpp>

// This is the inverse of the hyperbolic tangent function.

namespace boost
{
    namespace math
    {
       namespace detail
       {
#if defined(__GNUC__) && (__GNUC__ < 3)
        // gcc 2.x ignores function scope using declarations,
        // put them in the scope of the enclosing namespace instead:
        
        using    ::std::abs;
        using    ::std::sqrt;
        using    ::std::log;
        
        using    ::std::numeric_limits;
#endif
        
        // This is the main fare
        
        template<typename T, typename Policy>
        inline T    atanh_imp(const T x, const Policy& pol)
        {
            using    ::std::abs;
            using    ::std::sqrt;
            using    ::std::log;
            
            using    ::std::numeric_limits;
            
            T const            one = static_cast<T>(1);
            T const            two = static_cast<T>(2);
            
            static T const    taylor_2_bound = sqrt(tools::epsilon<T>());
            static T const    taylor_n_bound = sqrt(taylor_2_bound);

            static const char* function = "boost::math::atanh<%1%>(%1%)";
            
            if        (x < -one)
            {
               return policies::raise_domain_error<T>(
                  function,
                  "atanh requires x >= -1, but got x = %1%.", x, pol);
            }
            else if    (x < -one + tools::epsilon<T>())
            {
               // -Infinity:
               return -policies::raise_overflow_error<T>(function, 0, pol);
            }
            else if    (x > one - tools::epsilon<T>())
            {
               // Infinity:
               return -policies::raise_overflow_error<T>(function, 0, pol);
            }
            else if    (x > +one)
            {
               return policies::raise_domain_error<T>(
                  function,
                  "atanh requires x <= 1, but got x = %1%.", x, pol);
            }
            else if    (abs(x) >= taylor_n_bound)
            {
                return(log( (one + x) / (one - x) ) / two);
            }
            else
            {
                // approximation by taylor series in x at 0 up to order 2
                T    result = x;
                
                if    (abs(x) >= taylor_2_bound)
                {
                    T    x3 = x*x*x;
                    
                    // approximation by taylor series in x at 0 up to order 4
                    result += x3/static_cast<T>(3);
                }
                
                return(result);
            }
        }
       }

        template<typename T, typename Policy>
        inline typename tools::promote_args<T>::type atanh(const T x, const Policy& pol)
        {
           typedef typename tools::promote_args<T>::type result_type;
           return detail::atanh_imp(
              static_cast<result_type>(x), pol);
        }
        template<typename T>
        inline typename tools::promote_args<T>::type atanh(const T x)
        {
           typedef typename tools::promote_args<T>::type result_type;
           return detail::atanh_imp(
              static_cast<result_type>(x), policies::policy<>());
        }

    }
}

#endif /* BOOST_ATANH_HPP */



