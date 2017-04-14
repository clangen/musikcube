/*
  This file is part of libmicrohttpd
  Copyright (C) 2015 Karlson2k (Evgeny Grin)

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
 * @file microhttpd/mhd_limits.h
 * @brief  limits values definitions
 * @author Karlson2k (Evgeny Grin)
 */

#ifndef MHD_LIMITS_H
#define MHD_LIMITS_H

#include "platform.h"

#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif /* HAVE_LIMITS_H */

#ifndef UINT_MAX
#ifdef __UINT_MAX__
#define UINT_MAX __UINT_MAX__
#else  /* ! __UINT_MAX__ */
#define UINT_MAX ((unsigned int) ~((unsigned int)0))
#endif /* ! __UINT_MAX__ */
#endif /* !UINT_MAX */

#ifndef LONG_MAX
#ifdef __LONG_MAX__
#define LONG_MAX __LONG_MAX__
#else  /* ! __LONG_MAX__ */
#define LONG_MAX ((long) ~(((uint64_t) 1) << (8 * sizeof(long) - 1)))
#endif /* ! __LONG_MAX__ */
#endif /* !OFF_T_MAX */

#ifndef ULLONG_MAX
#define ULLONG_MAX ((MHD_UNSIGNED_LONG_LONG) ~((MHD_UNSIGNED_LONG_LONG)0))
#endif /* !ULLONG_MAX */

#ifndef INT32_MAX
#ifdef __INT32_MAX__
#define INT32_MAX __INT32_MAX__
#else  /* ! __INT32_MAX__ */
#define INT32_MAX ((int32_t)0x7FFFFFFF)
#endif /* ! __INT32_MAX__ */
#endif /* !INT32_MAX */

#ifndef UINT32_MAX
#ifdef __UINT32_MAX__
#define UINT32_MAX __UINT32_MAX__
#else  /* ! __UINT32_MAX__ */
#define UINT32_MAX ((int32_t)0xFFFFFFFF)
#endif /* ! __UINT32_MAX__ */
#endif /* !UNT32_MAX */

#ifndef UINT64_MAX
#ifdef __UINT64_MAX__
#define UINT64_MAX __UINT64_MAX__
#else  /* ! __UINT64_MAX__ */
#define UINT64_MAX ((uint64_t)0xFFFFFFFFFFFFFFFF)
#endif /* ! __UINT64_MAX__ */
#endif /* !INT32_MAX */

#ifndef SIZE_MAX
#ifdef __SIZE_MAX__
#define SIZE_MAX __SIZE_MAX__
#else  /* ! __SIZE_MAX__ */
#define SIZE_MAX ((size_t) ~((size_t)0))
#endif /* ! __SIZE_MAX__ */ 
#endif /* !SIZE_MAX */

#ifndef OFF_T_MAX
#define OFF_T_MAX ((off_t) ~(((uint64_t) 1) << (8 * sizeof(off_t) - 1)))
#endif /* !OFF_T_MAX */

#if defined(_LARGEFILE64_SOURCE) && !defined(OFF64_T_MAX)
#define OFF64_T_MAX ((off64_t) ~(((uint64_t) 1) << (8 * sizeof(off64_t) - 1)))
#endif /* _LARGEFILE64_SOURCE && !OFF64_T_MAX */

#ifndef TIME_T_MAX
/* Assume that time_t is signed type. */
/* Even if time_t is unsigned, TIME_T_MAX will be safe limit */
#define TIME_T_MAX ( (time_t) ~(((uint64_t) 1) << (8 * sizeof(time_t) - 1)) )
#endif /* !TIME_T_MAX */

#ifndef TIMEVAL_TV_SEC_MAX
#ifndef _WIN32
#define TIMEVAL_TV_SEC_MAX TIME_T_MAX
#else  /* _WIN32 */
#define TIMEVAL_TV_SEC_MAX LONG_MAX
#endif /* _WIN32 */
#endif /* !TIMEVAL_TV_SEC_MAX */

#endif /* MHD_LIMITS_H */
