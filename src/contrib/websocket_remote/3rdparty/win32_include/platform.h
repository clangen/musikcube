/*
     This file is part of libmicrohttpd
     Copyright (C) 2008 Christian Grothoff (and other contributing authors)

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
 * @file platform.h
 * @brief platform-specific includes for libmicrohttpd
 * @author Christian Grothoff
 *
 * This file is included by the libmicrohttpd code
 * before "microhttpd.h"; it provides the required
 * standard headers (which are platform-specific).<p>
 *
 * Note that this file depends on our configure.ac
 * build process and the generated config.h file.
 * Hence you cannot include it directly in applications
 * that use libmicrohttpd.
 */
#ifndef MHD_PLATFORM_H
#define MHD_PLATFORM_H

#include "mhd_options.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <stdarg.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stddef.h>

/* different OSes have fd_set in
   a broad range of header files;
   we just include most of them (if they
   are available) */


#if defined(__VXWORKS__) || defined(__vxworks) || defined(OS_VXWORKS)
#include <stdarg.h>
#include <sys/mman.h>
#ifdef HAVE_SOCKLIB_H
#include <sockLib.h>
#endif /* HAVE_SOCKLIB_H */
#ifdef HAVE_INETLIB_H
#include <inetLib.h>
#endif /* HAVE_INETLIB_H */
#endif /* __VXWORKS__ */

#if HAVE_MEMORY_H
#include <memory.h>
#endif

#if HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif
#if HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#if HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#if HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#if HAVE_SYS_MSG_H
#include <sys/msg.h>
#endif
#if HAVE_SYS_MMAN_H
#include <sys/mman.h>
#endif
#if HAVE_TIME_H
#include <time.h>
#endif
#if HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#if defined(_WIN32) && !defined(__CYGWIN__)
#ifndef WIN32_LEAN_AND_MEAN
/* Do not include unneeded parts of W32 headers. */
#define WIN32_LEAN_AND_MEAN 1
#endif /* !WIN32_LEAN_AND_MEAN */
#include <winsock2.h>
#include <ws2tcpip.h>
#endif /* _WIN32 && !__CYGWIN__ */

#if defined(__CYGWIN__) && !defined(_SYS_TYPES_FD_SET)
/* Do not define __USE_W32_SOCKETS under Cygwin! */
#error Cygwin with winsock fd_set is not supported
#endif

#if defined(_WIN32) && !defined(__CYGWIN__)
#define sleep(seconds) ((SleepEx((seconds)*1000, 1)==0)?0:(seconds))
#define usleep(useconds) ((SleepEx((useconds)/1000, 1)==0)?0:-1)
#endif

#if defined(_MSC_FULL_VER) && !defined (_SSIZE_T_DEFINED)
#define _SSIZE_T_DEFINED
typedef intptr_t ssize_t;
#endif /* !_SSIZE_T_DEFINED */

#ifndef _WIN32
typedef time_t _MHD_TIMEVAL_TV_SEC_TYPE;
#else  /* _WIN32 */
typedef long _MHD_TIMEVAL_TV_SEC_TYPE;
#endif /* _WIN32 */

#if !defined(IPPROTO_IPV6) && defined(_MSC_FULL_VER) && _WIN32_WINNT >= 0x0501
/* VC use IPPROTO_IPV6 as part of enum */
#define IPPROTO_IPV6 IPPROTO_IPV6
#endif

#endif
