/*
	compat: Some compatibility functions.

	copyright 2007 by the mpg123 project - free software under the terms of the LGPL 2.1
	see COPYING and AUTHORS files in distribution or http://mpg123.org
	initially written by Thomas Orgis
*/

#ifndef MPG123_COMPAT_H
#define MPG123_COMPAT_H

/* realloc, size_t */
#include <stdlib.h>

void *safe_realloc(void *ptr, size_t size);
#ifndef HAVE_STRERROR
const char *strerror(int errnum);
#endif

#endif
