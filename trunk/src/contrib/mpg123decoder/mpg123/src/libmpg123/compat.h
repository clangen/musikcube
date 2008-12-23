/*
	compat: Some compatibility functions and header inclusions.
	Basic standard C stuff, that may barely be above/around C89.

	The mpg123 code is determined to keep it's legacy. A legacy of old, old UNIX.
	It is envisioned to include this compat header instead of any of the "standard" headers, to catch compatibility issues.
	So, don't include stdlib.h or string.h ... include compat.h.

	copyright 2007-8 by the mpg123 project - free software under the terms of the LGPL 2.1
	see COPYING and AUTHORS files in distribution or http://mpg123.org
	initially written by Thomas Orgis
*/

#ifndef MPG123_COMPAT_H
#define MPG123_COMPAT_H

#include "config.h"

#ifdef HAVE_STDLIB_H
/* realloc, size_t */
#include <stdlib.h>
#endif

#include        <stdio.h>
#include        <math.h>

#ifdef HAVE_SIGNAL_H
#include <signal.h>
#else
#ifdef HAVE_SYS_SIGNAL_H
#include <sys/signal.h>
#endif
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

/* Types, types, types. */
/* Do we actually need these two in addition to sys/types.h? As replacement? */
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_INTTYPES_H
#include <inttypes.h>
#endif
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif
/* We want SIZE_MAX, etc. */
#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif
 
#ifndef SIZE_MAX
#define SIZE_MAX ((size_t)-1)
#endif
#ifndef ULONG_MAX
#define ULONG_MAX ((unsigned long)-1)
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef OS2
#include <float.h>
#endif

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
/* For select(), I need select.h according to POSIX 2001, else: sys/time.h sys/types.h unistd.h */
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif

/* To parse big numbers... */
#ifdef HAVE_ATOLL
#define atobigint atoll
#else
#define atobigint atol
#endif

typedef unsigned char byte;

/* A safe realloc also for very old systems where realloc(NULL, size) returns NULL. */
void *safe_realloc(void *ptr, size_t size);
#ifndef HAVE_STRERROR
const char *strerror(int errnum);
#endif

#ifndef HAVE_STRDUP
char *strdup(const char *s);
#endif

/* If we have the size checks enabled, try to derive some sane printfs.
   Simple start: Use max integer type and format if long is not big enough.
   I am hesitating to use %ll without making sure that it's there... */
#if (defined SIZEOF_OFF_T) && (SIZEOF_OFF_T > SIZEOF_LONG) && (defined PRIiMAX)
# define OFF_P PRIiMAX
typedef intmax_t off_p;
#else
# define OFF_P "li"
typedef long off_p;
#endif

#if (defined SIZEOF_SIZE_T) && (SIZEOF_SIZE_T > SIZEOF_LONG) && (defined PRIuMAX)
# define SIZE_P PRIuMAX
typedef uintmax_t size_p;
#else
# define SIZE_P "lu"
typedef unsigned long size_p;
#endif

#if (defined SIZEOF_SSIZE_T) && (SIZEOF_SSIZE_T > SIZEOF_LONG) && (defined PRIiMAX)
# define SSIZE_P PRIuMAX
typedef intmax_t ssize_p;
#else
# define SSIZE_P "li"
typedef long ssize_p;
#endif

#endif
