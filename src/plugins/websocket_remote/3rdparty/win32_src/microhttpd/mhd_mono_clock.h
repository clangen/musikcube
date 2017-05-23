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
 * @file microhttpd/mhd_mono_clock.h
 * @brief  internal monotonic clock functions declarations
 * @author Karlson2k (Evgeny Grin)
 */

#ifndef MHD_MONO_CLOCK_H
#define MHD_MONO_CLOCK_H 1
#include "mhd_options.h"

#if defined(HAVE_TIME_H)
#include <time.h>
#elif defined(HAVE_SYS_TYPES_H)
#include <sys/types.h>
#endif

/**
 * Initialise monotonic seconds counter.
 */
void
MHD_monotonic_sec_counter_init(void);


/**
 * Deinitialise monotonic seconds counter by freeing any allocated resources
 */
void
MHD_monotonic_sec_counter_finish(void);


/**
 * Monotonic seconds counter, useful for timeout calculation.
 * Tries to be not affected by manually setting the system real time
 * clock or adjustments by NTP synchronization.
 *
 * @return number of seconds from some fixed moment
 */
time_t
MHD_monotonic_sec_counter(void);

#endif /* MHD_MONO_CLOCK_H */
