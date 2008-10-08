/*
	mpg123lib_intern: Common non-public stuff for libmpg123

	copyright 1995-2008 by the mpg123 project - free software under the terms of the LGPL 2.1
	see COPYING and AUTHORS files in distribution or http://mpg123.org

	derived from the old mpg123.h
*/

#ifndef MPG123_H_INTERN
#define MPG123_H_INTERN

#define MPG123_RATES 9
#define MPG123_ENCODINGS 10

/* ABI conformance for other compilers.
   mpg123 needs 16byte-aligned stack for SSE and friends.
   gcc provides that, but others don't necessarily. */
#ifndef attribute_align_arg
#if defined(__GNUC__) && (__GNUC__ > 4 || __GNUC__ == 4 && __GNUC_MINOR__>1)
#    define attribute_align_arg __attribute__((force_align_arg_pointer))
#else
#    define attribute_align_arg
#endif
#endif

/* export DLL symbols */
#if defined(WIN32) && defined(DYNAMIC_BUILD)
#define BUILD_MPG123_DLL
#endif
#include "mpg123.h"
#include "config.h"
#include "compat.h"

#define SKIP_JUNK 1

#ifndef M_PI
# define M_PI       3.14159265358979323846
#endif
#ifndef M_SQRT2
# define M_SQRT2	1.41421356237309504880
#endif

#ifdef SUNOS
#define memmove(dst,src,size) bcopy(src,dst,size)
#endif

/* some stuff has to go back to mpg123.h */
#ifdef REAL_IS_FLOAT
#  define real float
#  define REAL_SCANF "%f"
#  define REAL_PRINTF "%f"
#elif defined(REAL_IS_LONG_DOUBLE)
#  define real long double
#  define REAL_SCANF "%Lf"
#  define REAL_PRINTF "%Lf"
#elif defined(REAL_IS_FIXED)
# define real long

# define REAL_RADIX            15
# define REAL_FACTOR           (32.0 * 1024.0)

# define REAL_PLUS_32767       ( 32767 << REAL_RADIX )
# define REAL_MINUS_32768      ( -32768 << REAL_RADIX )

# define DOUBLE_TO_REAL(x)     ((int)((x) * REAL_FACTOR))
# define REAL_TO_SHORT(x)      ((x) >> REAL_RADIX)
# define REAL_MUL(x, y)                (((long long)(x) * (long long)(y)) >> REAL_RADIX)
#  define REAL_SCANF "%ld"
#  define REAL_PRINTF "%ld"

#else
#  define real double
#  define REAL_SCANF "%lf"
#  define REAL_PRINTF "%f"
#endif

#ifndef DOUBLE_TO_REAL
# define DOUBLE_TO_REAL(x)     (x)
#endif
#ifndef REAL_TO_SHORT
# define REAL_TO_SHORT(x)      (x)
#endif
#ifndef REAL_PLUS_32767
# define REAL_PLUS_32767       32767.0
#endif
#ifndef REAL_MINUS_32768
# define REAL_MINUS_32768      -32768.0
#endif
#ifndef REAL_MUL
# define REAL_MUL(x, y)                ((x) * (y))
#endif

/* used to be: AUDIOBUFSIZE = n*64 with n=1,2,3 ...
   now: factor on minimum frame buffer size (which takes upsampling into account) */
#define		AUDIOBUFSIZE		2

#include "true.h"

#define         MAX_NAME_SIZE           81
#define         SBLIMIT                 32
#define         SCALE_BLOCK             12
#define         SSLIMIT                 18

/* Same as MPG_M_* */
#define         MPG_MD_STEREO           0
#define         MPG_MD_JOINT_STEREO     1
#define         MPG_MD_DUAL_CHANNEL     2
#define         MPG_MD_MONO             3

/* float output only for generic decoder! */
#ifdef FLOATOUT
#define MAXOUTBURST 1.0
#define scale_t double
#else
/* I suspect that 32767 would be a better idea here, but Michael put this in... */
#define MAXOUTBURST 32768
#define scale_t long
#endif

/* Pre Shift fo 16 to 8 bit converter table */
#define AUSHIFT (3)

/* stuff that should be moved... */
#include        <stdio.h>
#include        <string.h>
#include        <signal.h>
#include        <math.h>

#ifndef WIN32
#include        <sys/signal.h>
#include        <unistd.h>
#endif

/* Types, types, types. */
/* Do we actually need these two in addition to sys/types.h? As replacement? */
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

typedef unsigned char byte;

#ifdef OS2
#include <float.h>
#endif

#include "decode.h"
#include "parse.h"
#include "optimize.h"
#include "frame.h"

/* fr is a mpg123_handle* by convention here... */
#define NOQUIET  (!(fr->p.flags & MPG123_QUIET))
#define VERBOSE  (NOQUIET && fr->p.verbose)
#define VERBOSE2 (NOQUIET && fr->p.verbose > 1)
#define VERBOSE3 (NOQUIET && fr->p.verbose > 2)
#define VERBOSE4 (NOQUIET && fr->p.verbose > 3)
#define PVERB(mp, level) (!((mp)->flags & MPG123_QUIET) && (mp)->verbose >= (level))

int decode_update(mpg123_handle *mh);

#endif
