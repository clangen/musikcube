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
 * @brief  internal monotonic clock functions implementations
 * @author Karlson2k (Evgeny Grin)
 */

#include "mhd_mono_clock.h"

#if defined(_WIN32) && defined(HAVE_CLOCK_GETTIME)
/* Prefer native clock source over wrappers */
#undef HAVE_CLOCK_GETTIME
#endif /* _WIN32 && HAVE_CLOCK_GETTIME */

#ifdef HAVE_CLOCK_GETTIME
#include <time.h>
#endif /* HAVE_CLOCK_GETTIME */

#ifdef HAVE_GETHRTIME
#ifdef HAVE_SYS_TIME_H
/* Solaris defines gethrtime() in sys/time.h */
#include <sys/time.h>
#endif /* HAVE_SYS_TIME_H */
#ifdef HAVE_TIME_H
/* HP-UX defines gethrtime() in time.h */
#include <time.h>
#endif /* HAVE_TIME_H */
#endif /* HAVE_GETHRTIME */

#ifdef HAVE_CLOCK_GET_TIME
#include <mach/mach.h>
/* for host_get_clock_service(), mach_host_self(), mach_task_self() */
#include <mach/clock.h>
/* for clock_get_time() */

#define _MHD_INVALID_CLOCK_SERV ((clock_serv_t) -2)

static clock_serv_t mono_clock_service = _MHD_INVALID_CLOCK_SERV;
#endif /* HAVE_CLOCK_GET_TIME */

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
/* Do not include unneeded parts of W32 headers. */
#define WIN32_LEAN_AND_MEAN 1
#endif /* !WIN32_LEAN_AND_MEAN */
#include <windows.h>
#include <stdint.h>
#endif /* _WIN32 */

#ifdef HAVE_CLOCK_GETTIME
#ifdef CLOCK_REALTIME
#define _MHD_UNWANTED_CLOCK CLOCK_REALTIME
#else  /* !CLOCK_REALTIME */
#define _MHD_UNWANTED_CLOCK ((clockid_t) -2)
#endif /* !CLOCK_REALTIME */

static clockid_t mono_clock_id = _MHD_UNWANTED_CLOCK;
#endif /* HAVE_CLOCK_GETTIME */

/* sync clocks; reduce chance of value wrap */
#if defined(HAVE_CLOCK_GETTIME) || defined(HAVE_CLOCK_GET_TIME) || defined(HAVE_GETHRTIME)
static time_t mono_clock_start;
#endif /* HAVE_CLOCK_GETTIME || HAVE_CLOCK_GET_TIME || HAVE_GETHRTIME */
static time_t sys_clock_start;
#ifdef HAVE_GETHRTIME
static hrtime_t hrtime_start;
#endif /* HAVE_GETHRTIME */
#ifdef _WIN32
#if _WIN32_WINNT >= 0x0600
static uint64_t tick_start;
#else  /* _WIN32_WINNT < 0x0600 */
static int64_t perf_freq;
static int64_t perf_start;
#endif /* _WIN32_WINNT < 0x0600 */
#endif /* _WIN32 */



/**
 * Type of monotonic clock source
 */
enum _MHD_mono_clock_source
{
  /**
   * No monotonic clock
   */
  _MHD_CLOCK_NO_SOURCE = 0,

  /**
   * clock_gettime() with specific clock
   */
  _MHD_CLOCK_GETTIME,

  /**
   * clock_get_time() with specific clock service
   */
  _MHD_CLOCK_GET_TIME,

  /**
   * gethrtime() / 1000000000
   */
  _MHD_CLOCK_GETHRTIME,

  /**
   * GetTickCount64() / 1000
   */
  _MHD_CLOCK_GETTICKCOUNT64,

   /**
    * QueryPerformanceCounter() / QueryPerformanceFrequency()
    */
  _MHD_CLOCK_PERFCOUNTER
};


/**
 * Initialise monotonic seconds counter.
 */
void
MHD_monotonic_sec_counter_init (void)
{
#ifdef HAVE_CLOCK_GET_TIME
  mach_timespec_t cur_time;
#endif /* HAVE_CLOCK_GET_TIME */
  enum _MHD_mono_clock_source mono_clock_source = _MHD_CLOCK_NO_SOURCE;
#ifdef HAVE_CLOCK_GETTIME
  struct timespec ts;

  mono_clock_id = _MHD_UNWANTED_CLOCK;
#endif /* HAVE_CLOCK_GETTIME */
#ifdef HAVE_CLOCK_GET_TIME
  mono_clock_service = _MHD_INVALID_CLOCK_SERV;
#endif /* HAVE_CLOCK_GET_TIME */

  /* just a little syntactic trick to get the
     various following ifdef's to work out nicely */
  if (0)
    {
    }
  else
#ifdef HAVE_CLOCK_GETTIME
#ifdef CLOCK_MONOTONIC_COARSE
  /* Linux-specific fast value-getting clock */
  /* Can be affected by frequency adjustment and don't count time in suspend, */
  /* but preferred since it's fast */
  if (0 == clock_gettime (CLOCK_MONOTONIC_COARSE,
                          &ts))
    {
      mono_clock_id = CLOCK_MONOTONIC_COARSE;
      mono_clock_start = ts.tv_sec;
      mono_clock_source = _MHD_CLOCK_GETTIME;
    }
  else
#endif /* CLOCK_MONOTONIC_COARSE */
#ifdef CLOCK_MONOTONIC_FAST
  /* FreeBSD/DragonFly fast value-getting clock */
  /* Can be affected by frequency adjustment, but preferred since it's fast */
  if (0 == clock_gettime (CLOCK_MONOTONIC_FAST,
                          &ts))
    {
      mono_clock_id = CLOCK_MONOTONIC_FAST;
      mono_clock_start = ts.tv_sec;
      mono_clock_source = _MHD_CLOCK_GETTIME;
    }
  else
#endif /* CLOCK_MONOTONIC_COARSE */
#ifdef CLOCK_MONOTONIC_RAW
  /* Linux-specific clock */
  /* Not affected by frequency adjustment, but don't count time in suspend */
  if (0 == clock_gettime (CLOCK_MONOTONIC_RAW,
                          &ts))
    {
      mono_clock_id = CLOCK_MONOTONIC_RAW;
      mono_clock_start = ts.tv_sec;
      mono_clock_source = _MHD_CLOCK_GETTIME;
    }
  else
#endif /* CLOCK_MONOTONIC_RAW */
#ifdef CLOCK_BOOTTIME
  /* Linux-specific clock */
  /* Count time in suspend so it's real monotonic on Linux, */
  /* but can be slower value-getting than other clocks */
  if (0 == clock_gettime (CLOCK_BOOTTIME,
                          &ts))
    {
      mono_clock_id = CLOCK_BOOTTIME;
      mono_clock_start = ts.tv_sec;
      mono_clock_source = _MHD_CLOCK_GETTIME;
    }
  else
#endif /* CLOCK_BOOTTIME */
#ifdef CLOCK_MONOTONIC
  /* Monotonic clock */
  /* Widely supported, may be affected by frequency adjustment */
  /* On Linux it's not truly monotonic as it doesn't count time in suspend */
  if (0 == clock_gettime (CLOCK_MONOTONIC,
                          &ts))
    {
      mono_clock_id = CLOCK_MONOTONIC;
      mono_clock_start = ts.tv_sec;
      mono_clock_source = _MHD_CLOCK_GETTIME;
    }
  else
#endif /* CLOCK_BOOTTIME */
#endif /* HAVE_CLOCK_GETTIME */
#ifdef HAVE_CLOCK_GET_TIME
  /* Darwin-specific monotonic clock */
  /* Should be monotonic as clock_set_time function always unconditionally */
  /* failed on latest kernels */
  if ( (KERN_SUCCESS == host_get_clock_service (mach_host_self(),
                                                SYSTEM_CLOCK,
                                                &mono_clock_service)) &&
       (KERN_SUCCESS == clock_get_time (mono_clock_service,
                                        &cur_time)) )
    {
      mono_clock_start = cur_time.tv_sec;
      mono_clock_source = _MHD_CLOCK_GET_TIME;
    }
  else
#endif /* HAVE_CLOCK_GET_TIME */
#ifdef _WIN32
#if _WIN32_WINNT >= 0x0600
  /* W32 Vista or later specific monotonic clock */
  /* Available since Vista, ~15ms accuracy */
  if (1)
    {
      tick_start = GetTickCount64 ();
      mono_clock_source = _MHD_CLOCK_GETTICKCOUNT64;
    }
  else
#else  /* _WIN32_WINNT < 0x0600 */
  /* W32 specific monotonic clock */
  /* Available on Windows 2000 and later */
  if (1)
    {
      LARGE_INTEGER freq;
      LARGE_INTEGER perf_counter;

      QueryPerformanceFrequency (&freq); /* never fail on XP and later */
      QueryPerformanceCounter (&perf_counter); /* never fail on XP and later */
      perf_freq = freq.QuadPart;
      perf_start = perf_counter.QuadPart;
      mono_clock_source = _MHD_CLOCK_PERFCOUNTER;
    }
  else
#endif /* _WIN32_WINNT < 0x0600 */
#endif /* _WIN32 */
#ifdef HAVE_CLOCK_GETTIME
#ifdef CLOCK_HIGHRES
  /* Solaris-specific monotonic high-resolution clock */
  /* Not preferred due to be potentially resource-hungry */
  if (0 == clock_gettime (CLOCK_HIGHRES,
                          &ts))
    {
      mono_clock_id = CLOCK_HIGHRES;
      mono_clock_start = ts.tv_sec;
      mono_clock_source = _MHD_CLOCK_GETTIME;
    }
  else
#endif /* CLOCK_HIGHRES */
#endif /* HAVE_CLOCK_GETTIME */
#ifdef HAVE_GETHRTIME
  /* HP-UX and Solaris monotonic clock */
  /* Not preferred due to be potentially resource-hungry */
  if (1)
    {
      hrtime_start = gethrtime ();
      mono_clock_source = _MHD_CLOCK_GETHRTIME;
    }
  else
#endif /* HAVE_GETHRTIME */
    {
      /* no suitable clock source was found */
      mono_clock_source = _MHD_CLOCK_NO_SOURCE;
    }

#ifdef HAVE_CLOCK_GET_TIME
  if ( (_MHD_CLOCK_GET_TIME != mono_clock_source) &&
       (_MHD_INVALID_CLOCK_SERV != mono_clock_service) )
    {
      /* clock service was initialised but clock_get_time failed */
      mach_port_deallocate (mach_task_self(),
                            mono_clock_service);
      mono_clock_service = _MHD_INVALID_CLOCK_SERV;
    }
#else
  (void) mono_clock_source; /* avoid compiler warning */
#endif /* HAVE_CLOCK_GET_TIME */

  sys_clock_start = time (NULL);
}


/**
 * Deinitialise monotonic seconds counter by freeing any allocated resources
 */
void
MHD_monotonic_sec_counter_finish (void)
{
#ifdef HAVE_CLOCK_GET_TIME
  if (_MHD_INVALID_CLOCK_SERV != mono_clock_service)
    {
      mach_port_deallocate (mach_task_self(),
                            mono_clock_service);
      mono_clock_service = _MHD_INVALID_CLOCK_SERV;
    }
#endif /* HAVE_CLOCK_GET_TIME */
}


/**
 * Monotonic seconds counter, useful for timeout calculation.
 * Tries to be not affected by manually setting the system real time
 * clock or adjustments by NTP synchronization.
 *
 * @return number of seconds from some fixed moment
 */
time_t
MHD_monotonic_sec_counter (void)
{
#ifdef HAVE_CLOCK_GETTIME
  struct timespec ts;

  if ( (_MHD_UNWANTED_CLOCK != mono_clock_id) &&
       (0 == clock_gettime (mono_clock_id ,
                            &ts)) )
    return ts.tv_sec - mono_clock_start;
#endif /* HAVE_CLOCK_GETTIME */
#ifdef HAVE_CLOCK_GET_TIME
  if (_MHD_INVALID_CLOCK_SERV != mono_clock_service)
    {
      mach_timespec_t cur_time;

      if (KERN_SUCCESS == clock_get_time(mono_clock_service,
                                         &cur_time))
        return cur_time.tv_sec - mono_clock_start;
    }
#endif /* HAVE_CLOCK_GET_TIME */
#if defined(_WIN32)
#if _WIN32_WINNT >= 0x0600
  if (1)
    return (time_t)(((uint64_t)(GetTickCount64() - tick_start)) / 1000);
#else  /* _WIN32_WINNT < 0x0600 */
  if (0 != perf_freq)
    {
      LARGE_INTEGER perf_counter;

      QueryPerformanceCounter (&perf_counter); /* never fail on XP and later */
      return (time_t)(((uint64_t)(perf_counter.QuadPart - perf_start)) / perf_freq);
    }
#endif /* _WIN32_WINNT < 0x0600 */
#endif /* _WIN32 */
#ifdef HAVE_GETHRTIME
  if (1)
    return (time_t)(((uint64_t) (gethrtime () - hrtime_start)) / 1000000000);
#endif /* HAVE_GETHRTIME */

  return time (NULL) - sys_clock_start;
}
