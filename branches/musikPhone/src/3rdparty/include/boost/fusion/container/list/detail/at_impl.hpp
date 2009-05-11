/*=============================================================================
    Copyright (c) 2001-2006 Joel de Guzman

    Distributed under the Boost Software License, Version 1.0. (See accompanying 
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/
#if !defined(FUSION_AT_IMPL_07172005_0726)
#define FUSION_AT_IMPL_07172005_0726

#include <boost/fusion/support/detail/access.hpp>
#include <boost/type_traits/is_const.hpp>
#include <boost/type_traits/add_const.hpp>
#include <boost/mpl/eval_if.hpp>
#include <boost/mpl/bool.hpp>

namespace boost { namespace fusion
{
    struct cons_tag;

    namespace extension
    {
        template <typename Tag>
        struct at_impl;

        template <>
        struct at_impl<cons_tag>
        {
            template <typename Sequence, typename N>
            struct apply 
            {
                typedef typename
                    mpl::eval_if<
                        is_const<Sequence>
                      , add_const<typename Sequence::cdr_type>
                      , mpl::identity<typename Sequence::cdr_type>
                    >::type
                cdr_type;

                typedef typename 
                    mpl::eval_if<
                        mpl::bool_<N::value == 0>
                      , mpl::identity<typename Sequence::car_type>
                      , apply<cdr_type, mpl::int_<N::value-1> >
                    >
                element;

                typedef typename
                    mpl::eval_if<
                        is_const<Sequence>
                      , detail::cref_result<element>
                      , detail::ref_result<element>
                    >::type
                type;

                template <typename Cons, int N2>
                static type
                call(Cons& s, mpl::int_<N2>)
                {
                    return call(s.cdr, mpl::int_<N2-1>());
                }

                template <typename Cons>
                static type
                call(Cons& s, mpl::int_<0>)
                {
                    return s.car;
                }

                static type
                call(Sequence& s)
                {
                    return call(s, mpl::int_<N::value>());
                }
            };
        };
    }
}}

#endif
