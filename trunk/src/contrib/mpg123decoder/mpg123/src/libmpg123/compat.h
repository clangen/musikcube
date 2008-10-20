/*
	compat: Some compatibility functions. Basic standard C stuff, that may barely be above/around C89.

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

#ifdef HAVE_STRING_H
#include <string.h>
#endif

void *safe_realloc(void *ptr, size_t size);
#ifndef HAVE_STRERROR
const char *strerror(int errnum);
#endif

#ifndef HAVE_STRDUP
char *strdup(const char *s);
#endif

#endif
