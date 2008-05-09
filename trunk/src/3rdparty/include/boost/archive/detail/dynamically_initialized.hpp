// Copyright David Abrahams 2006. Distributed under the Boost
// Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#ifndef BOOST_ARCHIVE_DETAIL_DYNAMICALLY_INITIALIZED_DWA2006524_HPP
# define BOOST_ARCHIVE_DETAIL_DYNAMICALLY_INITIALIZED_DWA2006524_HPP

# include <boost/serialization/force_include.hpp>

namespace boost { namespace archive { namespace detail { 

//
// Provides a dynamically-initialized (singleton) instance of T in a
// way that avoids LNK1179 on vc6.  See http://tinyurl.com/ljdp8 or
// http://lists.boost.org/Archives/boost/2006/05/105286.php for
// details.
//
template <class T>
struct dynamically_initialized
{
protected:
    static T& instance;
private:
    static T& get_instance();
#if defined(__GNUC__)
  // Workaround "warning: all member functions in class `
  // boost::archive::detail::dynamically_initialized<T>' are private"
 public:
  void work_around_gcc_warning();
#endif
};

template <class T>
T& dynamically_initialized<T>::instance
  = dynamically_initialized<T>::get_instance();

template <class T>
T& dynamically_initialized<T>::get_instance()
{
    static T instance_;
    return instance_;
}

}}} // namespace boost::archive::detail

#endif // BOOST_ARCHIVE_DETAIL_DYNAMICALLY_INITIALIZED_DWA2006524_HPP
