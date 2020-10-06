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
  License along with this library.
  If not, see <http://www.gnu.org/licenses/>.
*/

/**
 * @file microhttpd/mhd_byteorder.h
 * @brief  macro definitions for host byte order
 * @author Karlson2k (Evgeny Grin)
 */

#ifndef MHD_BYTEORDER_H
#define MHD_BYTEORDER_H

#include "platform.h"

#if HAVE_ENDIAN_H
#include <endian.h>
#endif

#if HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif

#if HAVE_MACHINE_ENDIAN_H
#include <machine/endian.h>
#endif

#if HAVE_SYS_ENDIAN_H
#include <sys/endian.h>
#endif

#if HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#if HAVE_SYS_BYTEORDER_H
#include <sys/byteorder.h>
#endif

#if HAVE_SYS_MACHINE_H
#include <sys/machine.h>
#endif

#if HAVE_MACHINE_PARAM_H
#include <machine/param.h>
#endif

#if HAVE_SYS_ISA_DEFS_H
#include <sys/isa_defs.h>
#endif

#define _MHD_BIG_ENDIAN 1234
#define _MHD_LITTLE_ENDIAN 4321
#define _MHD_PDP_ENDIAN 2143

#if defined(__BYTE_ORDER__)
#if defined(__ORDER_BIG_ENDIAN__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define _MHD_BYTE_ORDER _MHD_BIG_ENDIAN
#elif defined(__ORDER_LITTLE_ENDIAN__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define _MHD_BYTE_ORDER _MHD_LITTLE_ENDIAN
#elif defined(__ORDER_PDP_ENDIAN__) && __BYTE_ORDER__ == __ORDER_PDP_ENDIAN__
#define _MHD_BYTE_ORDER _MHD_PDP_ENDIAN
#endif /* __BYTE_ORDER__ == __ORDER_PDP_ENDIAN__ */
#elif defined(__BYTE_ORDER)
#if defined(__BIG_ENDIAN) && __BYTE_ORDER == __BIG_ENDIAN
#define _MHD_BYTE_ORDER _MHD_BIG_ENDIAN
#elif defined(__LITTLE_ENDIAN) && __BYTE_ORDER == __LITTLE_ENDIAN
#define _MHD_BYTE_ORDER _MHD_LITTLE_ENDIAN
#elif defined(__PDP_ENDIAN) && __BYTE_ORDER == __PDP_ENDIAN
#define _MHD_BYTE_ORDER _MHD_PDP_ENDIAN
#endif /* __BYTE_ORDER == __PDP_ENDIAN */
#elif defined (BYTE_ORDER)
#if defined(BIG_ENDIAN) && BYTE_ORDER == BIG_ENDIAN
#define _MHD_BYTE_ORDER _MHD_BIG_ENDIAN
#elif defined(LITTLE_ENDIAN) && BYTE_ORDER == LITTLE_ENDIAN
#define _MHD_BYTE_ORDER _MHD_LITTLE_ENDIAN
#elif defined(PDP_ENDIAN) && BYTE_ORDER == PDP_ENDIAN
#define _MHD_BYTE_ORDER _MHD_PDP_ENDIAN
#endif /* __BYTE_ORDER == _PDP_ENDIAN */
#elif defined (_BYTE_ORDER)
#if defined(_BIG_ENDIAN) && _BYTE_ORDER == _BIG_ENDIAN
#define _MHD_BYTE_ORDER _MHD_BIG_ENDIAN
#elif defined(_LITTLE_ENDIAN) && _BYTE_ORDER == _LITTLE_ENDIAN
#define _MHD_BYTE_ORDER _MHD_LITTLE_ENDIAN
#elif defined(_PDP_ENDIAN) && _BYTE_ORDER == _PDP_ENDIAN
#define _MHD_BYTE_ORDER _MHD_PDP_ENDIAN
#endif /* _BYTE_ORDER == _PDP_ENDIAN */
#endif /* _BYTE_ORDER */

#ifndef _MHD_BYTE_ORDER
/* Byte order specification didn't detected in system headers */
/* Try some guessing */

#if   (defined(__BIG_ENDIAN__) && !defined(__LITTLE_ENDIAN__)) || \
      (defined(_BIG_ENDIAN) && !defined(_LITTLE_ENDIAN))
/* Seems that we are on big endian platform */
#define _MHD_BYTE_ORDER _MHD_BIG_ENDIAN
#elif (defined(__LITTLE_ENDIAN__) && !defined(__BIG_ENDIAN__)) || \
      (defined(_LITTLE_ENDIAN) && !defined(_BIG_ENDIAN))
/* Seems that we are on little endian platform */
#define _MHD_BYTE_ORDER _MHD_LITTLE_ENDIAN
#elif defined(__amd64__) || defined(__amd64) || defined(__x86_64__) || defined(__x86_64) || \
      defined(_M_X64) || defined(_M_AMD64) || defined(i386) || defined(__i386) || \
      defined(__i386__) || defined(__i486__) || defined(__i586__) || defined(__i686__) || \
      defined(_M_IX86) || defined(_X86_) || defined (__THW_INTEL__)
/* x86 family is little endian */
#define _MHD_BYTE_ORDER _MHD_LITTLE_ENDIAN
#elif defined(__ARMEB__) || defined(__THUMBEB__) ||  defined(__AARCH64EB__) || \
      defined(_MIPSEB) || defined(__MIPSEB) || defined(__MIPSEB__)
/* Looks like we are on ARM/MIPS in big endian mode */
#define _MHD_BYTE_ORDER _MHD_BIG_ENDIAN
#elif defined(__ARMEL__) || defined(__THUMBEL__) || defined(__AARCH64EL__) || \
      defined(_MIPSEL) || defined(__MIPSEL) || defined(__MIPSEL__)
/* Looks like we are on ARM/MIPS in little endian mode */
#define _MHD_BYTE_ORDER _MHD_LITTLE_ENDIAN
#elif defined(__m68k__) || defined(M68000) || defined(__hppa__) || defined(__hppa) || \
      defined(__HPPA__) || defined(__370__) || defined(__THW_370__) || \
      defined(__s390__) || defined(__s390x__) || defined(__SYSC_ZARCH__)
/* Looks like we are on big endian platform */
#define _MHD_BYTE_ORDER _MHD_BIG_ENDIAN
#elif defined(__ia64__) || defined(_IA64) || defined(__IA64__) || defined(__ia64) || \
      defined(_M_IA64) || defined(__itanium__) || defined(__bfin__) || \
      defined(__BFIN__) || defined(bfin) || defined(BFIN)
/* Looks like we are on little endian platform */
#define _MHD_BYTE_ORDER _MHD_LITTLE_ENDIAN
#elif defined(_WIN32)
/* W32 is always little endian on all platforms */
#define _MHD_BYTE_ORDER _MHD_LITTLE_ENDIAN
#elif defined(WORDS_BIGENDIAN)
/* Use byte order detected by configure */
#define _MHD_BYTE_ORDER _MHD_BIG_ENDIAN
#endif /* _WIN32 */

#endif /* !_MHD_BYTE_ORDER */

#ifdef _MHD_BYTE_ORDER
/* Some safety checks */
#if defined(WORDS_BIGENDIAN) && _MHD_BYTE_ORDER != _MHD_BIG_ENDIAN
#error Configure detected big endian byte order but headers specify different byte order
#elif !defined(WORDS_BIGENDIAN) && _MHD_BYTE_ORDER == _MHD_BIG_ENDIAN
#error Configure did not detect big endian byte order but headers specify big endian byte order
#endif /* !WORDS_BIGENDIAN && _MHD_BYTE_ORDER == _MHD_BIG_ENDIAN */
#endif /* _MHD_BYTE_ORDER */

#endif /* !MHD_BYTEORDER_H */
