//
// win_thread.hpp
// ~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2008 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_ASIO_DETAIL_WIN_THREAD_HPP
#define BOOST_ASIO_DETAIL_WIN_THREAD_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include <boost/asio/detail/push_options.hpp>

#include <boost/asio/detail/push_options.hpp>
#include <boost/config.hpp>
#include <boost/system/system_error.hpp>
#include <boost/asio/detail/pop_options.hpp>

#if defined(BOOST_WINDOWS) && !defined(UNDER_CE)

#include <boost/asio/error.hpp>
#include <boost/asio/detail/noncopyable.hpp>
#include <boost/asio/detail/socket_types.hpp>

#include <boost/asio/detail/push_options.hpp>
#include <boost/throw_exception.hpp>
#include <memory>
#include <process.h>
#include <boost/asio/detail/pop_options.hpp>

namespace boost {
namespace asio {
namespace detail {

unsigned int __stdcall win_thread_function(void* arg);

class win_thread
  : private noncopyable
{
public:
  // Constructor.
  template <typename Function>
  win_thread(Function f)
  {
    std::auto_ptr<func_base> arg(new func<Function>(f));
    unsigned int thread_id = 0;
    thread_ = reinterpret_cast<HANDLE>(::_beginthreadex(0, 0,
          win_thread_function, arg.get(), 0, &thread_id));
    if (!thread_)
    {
      DWORD last_error = ::GetLastError();
      boost::system::system_error e(
          boost::system::error_code(last_error,
            boost::asio::error::get_system_category()),
          "thread");
      boost::throw_exception(e);
    }
    arg.release();
  }

  // Destructor.
  ~win_thread()
  {
    ::CloseHandle(thread_);
  }

  // Wait for the thread to exit.
  void join()
  {
    ::WaitForSingleObject(thread_, INFINITE);
  }

private:
  friend unsigned int __stdcall win_thread_function(void* arg);

  class func_base
  {
  public:
    virtual ~func_base() {}
    virtual void run() = 0;
  };

  template <typename Function>
  class func
    : public func_base
  {
  public:
    func(Function f)
      : f_(f)
    {
    }

    virtual void run()
    {
      f_();
    }

  private:
    Function f_;
  };

  ::HANDLE thread_;
};

inline unsigned int __stdcall win_thread_function(void* arg)
{
  std::auto_ptr<win_thread::func_base> func(
      static_cast<win_thread::func_base*>(arg));
  func->run();
  return 0;
}

} // namespace detail
} // namespace asio
} // namespace boost

#endif // defined(BOOST_WINDOWS) && !defined(UNDER_CE)

#include <boost/asio/detail/pop_options.hpp>

#endif // BOOST_ASIO_DETAIL_WIN_THREAD_HPP
