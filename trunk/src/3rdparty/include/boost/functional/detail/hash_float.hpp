
// Copyright 2005-2007 Daniel James.
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

//  Based on Peter Dimov's proposal
//  http://www.open-std.org/JTC1/SC22/WG21/docs/papers/2005/n1756.pdf
//  issue 6.18. 

#if !defined(BOOST_FUNCTIONAL_DETAIL_HASH_FLOAT_HEADER)
#define BOOST_FUNCTIONAL_DETAIL_HASH_FLOAT_HEADER

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma once
#endif

#if defined(BOOST_MSVC)
#pragma warning(push)
#if BOOST_MSVC >= 1400
#pragma warning(disable:6294) // Ill-defined for-loop: initial condition does
                              // not satisfy test. Loop body not executed 
#endif
#endif

#include <boost/functional/detail/float_functions.hpp>
#include <boost/integer/static_log2.hpp>
#include <boost/cstdint.hpp>
#include <boost/limits.hpp>
#include <boost/assert.hpp>

// Select implementation for the current platform.

// Cygwn
#if defined(__CYGWIN__)
#  if defined(__i386__) || defined(_M_IX86)
#    define BOOST_HASH_USE_x86_BINARY_HASH
#  endif

// STLport
#elif defined(__SGI_STL_PORT) || defined(_STLPORT_VERSION)
// _fpclass and fpclassify aren't good enough on STLport.

// GNU libstdc++ 3
#elif defined(__GLIBCPP__) || defined(__GLIBCXX__)
#  if (defined(__USE_ISOC99) || defined(_GLIBCXX_USE_C99_MATH)) && \
      !(defined(macintosh) || defined(__APPLE__) || defined(__APPLE_CC__))
#    define BOOST_HASH_USE_FPCLASSIFY
#  endif

// Dinkumware Library, on Visual C++ 
#elif (defined(_YVALS) && !defined(__IBMCPP__)) || defined(_CPPLIB_VER)

// Not using _fpclass because it causes a warning about a conversion
// from 'long double' to 'double'. Pity.
// 
//#  if defined(BOOST_MSVC)
//#    define BOOST_HASH_USE_FPCLASS
//#  endif

#endif

namespace boost
{
    namespace hash_detail
    {
        inline void hash_float_combine(std::size_t& seed, std::size_t value)
        {
            seed ^= value + (seed<<6) + (seed>>2);
        }

// A simple, non-portable hash algorithm for x86.
#if defined(BOOST_HASH_USE_x86_BINARY_HASH)
        inline std::size_t float_hash_impl(float v)
        {
            boost::uint32_t* ptr = (boost::uint32_t*)&v;
            std::size_t seed = *ptr;
            return seed;
        }

        inline std::size_t float_hash_impl(double v)
        {
            boost::uint32_t* ptr = (boost::uint32_t*)&v;
            std::size_t seed = *ptr++;
            hash_float_combine(seed, *ptr);
            return seed;
        }

        inline std::size_t float_hash_impl(long double v)
        {
            boost::uint32_t* ptr = (boost::uint32_t*)&v;
            std::size_t seed = *ptr++;
            hash_float_combine(seed, *ptr++);
            hash_float_combine(seed, *(boost::uint16_t*)ptr);
            return seed;
        }

#else

        template <class T>
        inline std::size_t float_hash_impl(T v)
        {
            int exp = 0;

            v = boost::hash_detail::call_frexp(v, &exp);

            // A postive value is easier to hash, so combine the
            // sign with the exponent.
            if(v < 0) {
                v = -v;
                exp += std::numeric_limits<T>::max_exponent -
                    std::numeric_limits<T>::min_exponent;
            }

            // The result of frexp is always between 0.5 and 1, so its
            // top bit will always be 1. Subtract by 0.5 to remove that.
            v -= T(0.5);
            v = boost::hash_detail::call_ldexp(v,
                    std::numeric_limits<std::size_t>::digits + 1);
            std::size_t seed = static_cast<std::size_t>(v);
            v -= seed;

            // ceiling(digits(T) * log2(radix(T))/ digits(size_t)) - 1;
            std::size_t const length
                = (std::numeric_limits<T>::digits *
                        boost::static_log2<std::numeric_limits<T>::radix>::value
                        - 1)
                / std::numeric_limits<std::size_t>::digits;

            for(std::size_t i = 0; i != length; ++i)
            {
                v = boost::hash_detail::call_ldexp(v,
                        std::numeric_limits<std::size_t>::digits);
                std::size_t part = static_cast<std::size_t>(v);
                v -= part;
                hash_float_combine(seed, part);
            }

            hash_float_combine(seed, exp);

            return seed;
        }
#endif

        template <class T>
        inline std::size_t float_hash_value(T v)
        {
#if defined(BOOST_HASH_USE_FPCLASSIFY)
            using namespace std;
            switch (fpclassify(v)) {
            case FP_ZERO:
                return 0;
            case FP_INFINITE:
                return (std::size_t)(v > 0 ? -1 : -2);
            case FP_NAN:
                return (std::size_t)(-3);
            case FP_NORMAL:
            case FP_SUBNORMAL:
                return float_hash_impl(v);
            default:
                BOOST_ASSERT(0);
                return 0;
            }
#elif defined(BOOST_HASH_USE_FPCLASS)
            switch(_fpclass(v)) {
            case _FPCLASS_NZ:
            case _FPCLASS_PZ:
                return 0;
            case _FPCLASS_PINF:
                return (std::size_t)(-1);
            case _FPCLASS_NINF:
                return (std::size_t)(-2);
            case _FPCLASS_SNAN:
            case _FPCLASS_QNAN:
                return (std::size_t)(-3);
            case _FPCLASS_NN:
            case _FPCLASS_ND:
                return float_hash_impl(v);
            case _FPCLASS_PD:
            case _FPCLASS_PN:
                return float_hash_impl(v);
            default:
                BOOST_ASSERT(0);
                return 0;
            }
#else
            return v == 0 ? 0 : float_hash_impl(v);
#endif
        }
    }
}

#if defined(BOOST_MSVC)
#pragma warning(pop)
#endif

#endif
