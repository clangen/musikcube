/*
  This file is part of libmicrohttpd
  Copyright (C) 2016 Karlson2k (Evgeny Grin)

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

*/

/**
 * @file microhttpd/mhd_threads.c
 * @brief  Implementation for thread functions
 * @author Karlson2k (Evgeny Grin)
 */

#include "mhd_threads.h"
#ifdef MHD_USE_W32_THREADS
#include "mhd_limits.h"
#include <process.h>
#endif
#ifdef MHD_USE_THREAD_NAME_
#include <stdlib.h>
#ifdef HAVE_PTHREAD_NP_H
#include <pthread_np.h>
#endif /* HAVE_PTHREAD_NP_H */
#endif /* MHD_USE_THREAD_NAME_ */
#include <errno.h>



#if defined(MHD_USE_POSIX_THREADS)
typedef pthread_t MHD_thread_ID_;
#elif defined(MHD_USE_W32_THREADS)
typedef DWORD MHD_thread_ID_;
#endif


#ifndef MHD_USE_THREAD_NAME_

#define MHD_set_thread_name_(t, n) (void)
#define MHD_set_cur_thread_name_(n) (void)

#else  /* MHD_USE_THREAD_NAME_ */

#if defined(MHD_USE_POSIX_THREADS)
#if defined(HAVE_PTHREAD_ATTR_SETNAME_NP_NETBSD) || defined(HAVE_PTHREAD_ATTR_SETNAME_NP_IBMI)
#  define MHD_USE_THREAD_ATTR_SETNAME 1
#endif /* HAVE_PTHREAD_ATTR_SETNAME_NP_NETBSD || HAVE_PTHREAD_ATTR_SETNAME_NP_IBMI */

#if defined(HAVE_PTHREAD_SETNAME_NP_GNU) || defined(HAVE_PTHREAD_SET_NAME_NP_FREEBSD) \
    || defined(HAVE_PTHREAD_SETNAME_NP_NETBSD)

/**
 * Set thread name
 *
 * @param thread_id ID of thread
 * @param thread_name name to set
 * @return non-zero on success, zero otherwise
 */
static int
MHD_set_thread_name_(const MHD_thread_ID_ thread_id,
                     const char *thread_name)
{
  if (NULL == thread_name)
    return 0;

#if defined(HAVE_PTHREAD_SETNAME_NP_GNU)
  return !pthread_setname_np (thread_id, thread_name);
#elif defined(HAVE_PTHREAD_SET_NAME_NP_FREEBSD)
  /* FreeBSD and OpenBSD use different name and void return type */
  pthread_set_name_np (thread_id, thread_name);
  return !0;
#elif defined(HAVE_PTHREAD_SETNAME_NP_NETBSD)
  /* NetBSD use 3 arguments: second argument is string in printf-like format,
   *                         third argument is single argument for printf;
   * OSF1 use 3 arguments too, but last one always must be zero (NULL).
   * MHD doesn't use '%' in thread names, so both form are used in same way.
   */
  return !pthread_setname_np (thread_id, thread_name, 0);
#endif /* HAVE_PTHREAD_SETNAME_NP_NETBSD */
}


#ifndef __QNXNTO__
/**
 * Set current thread name
 * @param n name to set
 * @return non-zero on success, zero otherwise
 */
#define MHD_set_cur_thread_name_(n) MHD_set_thread_name_(pthread_self(),(n))
#else  /* __QNXNTO__ */
/* Special case for QNX Neutrino - using zero for thread ID sets name faster. */
#define MHD_set_cur_thread_name_(n) MHD_set_thread_name_(0,(n))
#endif /* __QNXNTO__ */
#elif defined(HAVE_PTHREAD_SETNAME_NP_DARWIN)

/**
 * Set current thread name
 * @param n name to set
 * @return non-zero on success, zero otherwise
 */
#define MHD_set_cur_thread_name_(n) (!(pthread_setname_np((n))))
#endif /* HAVE_PTHREAD_SETNAME_NP_DARWIN */

#elif defined(MHD_USE_W32_THREADS)
#ifndef _MSC_FULL_VER
/* Thread name available only for VC-compiler */
#else  /* _MSC_FULL_VER */
/**
 * Set thread name
 *
 * @param thread_id ID of thread, -1 for current thread
 * @param thread_name name to set
 * @return non-zero on success, zero otherwise
 */
static int
MHD_set_thread_name_(const MHD_thread_ID_ thread_id,
                     const char *thread_name)
{
  static const DWORD VC_SETNAME_EXC = 0x406D1388;
#pragma pack(push,8)
  struct thread_info_struct
  {
    DWORD type;   /* Must be 0x1000. */
    LPCSTR name;  /* Pointer to name (in user address space). */
    DWORD ID;     /* Thread ID (-1 = caller thread). */
    DWORD flags;  /* Reserved for future use, must be zero. */
  } thread_info;
#pragma pack(pop)

  if (NULL == thread_name)
    return 0;

  thread_info.type  = 0x1000;
  thread_info.name  = thread_name;
  thread_info.ID    = thread_id;
  thread_info.flags = 0;

  __try
  { /* This exception is intercepted by debugger */
    RaiseException (VC_SETNAME_EXC,
                    0,
                    sizeof (thread_info) / sizeof(ULONG_PTR),
                    (ULONG_PTR *) &thread_info);
  }
  __except (EXCEPTION_EXECUTE_HANDLER)
  {}

  return !0;
}


/**
 * Set current thread name
 * @param n name to set
 * @return non-zero on success, zero otherwise
 */
#define MHD_set_cur_thread_name_(n) MHD_set_thread_name_(-1,(n))
#endif /* _MSC_FULL_VER */
#endif /* MHD_USE_W32_THREADS */

#endif /* MHD_USE_THREAD_NAME_ */


/**
 * Create a thread and set the attributes according to our options.
 *
 * @param thread        handle to initialize
 * @param stack_size    size of stack for new thread, 0 for default
 * @param start_routine main function of thread
 * @param arg argument  for start_routine
 * @return non-zero on success; zero otherwise (with errno set)
 */
int
MHD_create_thread_ (MHD_thread_handle_ *thread,
                    size_t stack_size,
                    MHD_THREAD_START_ROUTINE_ start_routine,
                    void *arg)
{
#if defined(MHD_USE_POSIX_THREADS)
  int res;

  if (0 != stack_size)
    {
      pthread_attr_t attr;
      res = pthread_attr_init (&attr);
      if (0 == res)
        {
          res = pthread_attr_setstacksize (&attr,
                                           stack_size);
          if (0 == res)
              res = pthread_create (thread,
                                    &attr,
                                    start_routine,
                                    arg);
          pthread_attr_destroy (&attr);
        }
    }
  else
    res = pthread_create (thread,
                          NULL,
                          start_routine,
                          arg);

  if (0 != res)
    errno = res;

  return !res;
#elif defined(MHD_USE_W32_THREADS)
#if SIZE_MAX != UINT_MAX
  if (stack_size > UINT_MAX)
    {
      errno = EINVAL;
      return 0;
    }
#endif /* SIZE_MAX != UINT_MAX */

  *thread = (HANDLE) _beginthreadex (NULL,
                                     (unsigned int) stack_size,
                                     start_routine,
                                     arg,
                                     0,
                                     NULL);
  if ((MHD_thread_handle_)-1 == (*thread))
    return 0;

  return !0;
#endif
}

#ifdef MHD_USE_THREAD_NAME_

#ifndef MHD_USE_THREAD_ATTR_SETNAME
struct MHD_named_helper_param_
{
  /**
   * Real thread start routine
   */
  MHD_THREAD_START_ROUTINE_ start_routine;

  /**
   * Argument for thread start routine
   */
  void *arg;

  /**
   * Name for thread
   */
  const char *name;
};


static MHD_THRD_RTRN_TYPE_ MHD_THRD_CALL_SPEC_
named_thread_starter (void *data)
{
  struct MHD_named_helper_param_ * const param =
      (struct MHD_named_helper_param_ *) data;
  void * arg;
  MHD_THREAD_START_ROUTINE_ thr_func;

  if (NULL == data)
    return (MHD_THRD_RTRN_TYPE_)0;

  MHD_set_cur_thread_name_ (param->name);

  arg = param->arg;
  thr_func = param->start_routine;
  free(data);

  return thr_func(arg);
}
#endif /* ! MHD_USE_THREAD_ATTR_SETNAME */


/**
 * Create a named thread and set the attributes according to our options.
 *
 * @param thread        handle to initialize
 * @param thread_name   name for new thread
 * @param stack_size    size of stack for new thread, 0 for default
 * @param start_routine main function of thread
 * @param arg argument  for start_routine
 * @return non-zero on success; zero otherwise (with errno set)
 */
int
MHD_create_named_thread_ (MHD_thread_handle_ *thread,
                          const char* thread_name,
                          size_t stack_size,
                          MHD_THREAD_START_ROUTINE_ start_routine,
                          void *arg)
{
#if defined(MHD_USE_THREAD_ATTR_SETNAME)
  int res;
  pthread_attr_t attr;

  res = pthread_attr_init (&attr);
  if (0 == res)
    {
#if defined(HAVE_PTHREAD_ATTR_SETNAME_NP_NETBSD)
  /* NetBSD use 3 arguments: second argument is string in printf-like format,
   *                         third argument is single argument for printf;
   * OSF1 use 3 arguments too, but last one always must be zero (NULL).
   * MHD doesn't use '%' in thread names, so both form are used in same way.
   */
      res = pthread_attr_setname_np (&attr, thread_name, 0);
#elif defined(HAVE_PTHREAD_ATTR_SETNAME_NP_IBMI)
      res = pthread_attr_setname_np (&attr, thread_name);
#else
#error No pthread_attr_setname_np() function.
#endif
      if (res == 0 && 0 != stack_size)
        res = pthread_attr_setstacksize (&attr,
                                         stack_size);
      if (0 == res)
          res = pthread_create (thread,
                                &attr,
                                start_routine,
                                arg);
      pthread_attr_destroy (&attr);
    }
  if (0 != res)
    errno = res;

  return !res;
#else  /* ! MHD_USE_THREAD_ATTR_SETNAME */
  struct MHD_named_helper_param_ *param;

  if (NULL == thread_name)
    {
      errno = EINVAL;
      return 0;
    }

  param = malloc (sizeof (struct MHD_named_helper_param_));
  if (NULL == param)
    return 0;

  param->start_routine = start_routine;
  param->arg = arg;
  param->name = thread_name;

  /* Set thread name in thread itself to avoid problems with
   * threads which terminated before name is set in other thread.
   */
  if (! MHD_create_thread_(thread,
                           stack_size,
                           &named_thread_starter,
                           (void*)param))
    {
      free (param);
      return 0;
    }

  return !0;
#endif /* ! MHD_USE_THREAD_ATTR_SETNAME */
}

#endif /* MHD_USE_THREAD_NAME_ */
