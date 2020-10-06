/*
  This file is part of libmicrohttpd
  Copyright (C) 2014-2016 Karlson2k (Evgeny Grin)

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
 * @file microhttpd/mhd_compat.c
 * @brief  Implementation of platform missing functions.
 * @author Karlson2k (Evgeny Grin)
 */

#include "mhd_compat.h"
#if defined(_WIN32) && !defined(__CYGWIN__)
#include <stdint.h>
#include <time.h>
#ifndef HAVE_SNPRINTF
#include <stdio.h>
#include <stdarg.h>
#endif  /* HAVE_SNPRINTF */
#endif /* _WIN32  && !__CYGWIN__ */

#ifndef HAVE_CALLOC
#include <string.h> /* for memset() */
#endif /* ! HAVE_CALLOC */

#if defined(_WIN32) && !defined(__CYGWIN__)

#ifndef HAVE_SNPRINTF
/* Emulate snprintf function on W32 */
int
W32_snprintf (char *__restrict s,
              size_t n,
              const char *__restrict format,
              ...)
{
  int ret;
  va_list args;

  if ( (0 != n) &&
       (NULL != s) )
  {
    va_start (args,
              format);
    ret = _vsnprintf (s,
                      n,
                      format,
                      args);
    va_end (args);
    if ((int)n == ret)
      s[n - 1] = 0;
    if (ret >= 0)
      return ret;
  }
  va_start(args,
           format);
  ret = _vscprintf (format,
                    args);
  va_end(args);
  if ( (0 <= ret) &&
       (0 != n) &&
       (NULL == s) )
    return -1;

  return ret;
}

#endif  /* HAVE_SNPRINTF */
#endif /* _WIN32  && !__CYGWIN__ */

#ifndef HAVE_CALLOC

#ifdef __has_builtin
#  if __has_builtin(__builtin_mul_overflow)
#    define MHD_HAVE_NUL_OVERFLOW 1
#  endif
#elif __GNUC__+0 >= 5
#  define MHD_HAVE_NUL_OVERFLOW 1
#endif /* __GNUC__ >= 5 */


void *MHD_calloc_(size_t nelem, size_t elsize)
{
  size_t alloc_size;
  void *ptr;
#ifdef MHD_HAVE_NUL_OVERFLOW
  if (__builtin_mul_overflow(nelem, elsize, &alloc_size) || 0 == alloc_size)
    return NULL;
#else  /* ! MHD_HAVE_NUL_OVERFLOW */
  alloc_size = nelem * elsize;
  if (0 == alloc_size || elsize != alloc_size / nelem)
    return NULL;
#endif /* ! MHD_HAVE_NUL_OVERFLOW */
  ptr = malloc (alloc_size);
  if (NULL == ptr)
    return NULL;
  memset(ptr, 0, alloc_size);
  return ptr;
}
#endif /* ! HAVE_CALLOC */
