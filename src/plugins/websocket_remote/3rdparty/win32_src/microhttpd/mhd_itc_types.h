/*
  This file is part of libmicrohttpd
  Copyright (C) 2016 Karlson2k (Evgeny Grin), Christian Grothoff

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
 * @file microhttpd/mhd_itc_types.h
 * @brief  Types for platform-independent inter-thread communication
 * @author Karlson2k (Evgeny Grin)
 * @author Christian Grothoff
 *
 * Provides basic types for inter-thread communication.
 * Designed to be included by other headers.
 */
#ifndef MHD_ITC_TYPES_H
#define MHD_ITC_TYPES_H 1
#include "mhd_options.h"

/* Force socketpair on native W32 */
#if defined(_WIN32) && !defined(__CYGWIN__) && !defined(_MHD_ITC_SOCKETPAIR)
#error _MHD_ITC_SOCKETPAIR is not defined on naitive W32 platform
#endif /* _WIN32 && !__CYGWIN__ && !_MHD_ITC_SOCKETPAIR */

#if defined(_MHD_ITC_EVENTFD)
/* **************** Optimized GNU/Linux ITC implementation by eventfd ********** */

/**
 * Data type for a MHD ITC.
 */
struct MHD_itc_
{
  int fd;
};

#elif defined(_MHD_ITC_PIPE)
/* **************** Standard UNIX ITC implementation by pipe ********** */

/**
 * Data type for a MHD ITC.
 */
struct MHD_itc_
{
  int fd[2];
};


#elif defined(_MHD_ITC_SOCKETPAIR)
/* **************** ITC implementation by socket pair ********** */

#include "mhd_sockets.h"

/**
 * Data type for a MHD ITC.
 */
struct MHD_itc_
{
  MHD_socket sk[2];
};

#endif /* _MHD_ITC_SOCKETPAIR */

#endif /* ! MHD_ITC_TYPES_H */
