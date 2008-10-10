/*
	compat: Some compatibility functions. Basic standard C stuff, that may barely be above/around C89.

	The mpg123 code is determined to keep it's legacy. A legacy of old, old UNIX.

	copyright 2007-8 by the mpg123 project - free software under the terms of the LGPL 2.1
	see COPYING and AUTHORS files in distribution or http://mpg123.org
	initially written by Thomas Orgis
*/

#include "config.h"
#include "compat.h"

/* A safe realloc also for very old systems where realloc(NULL, size) returns NULL. */
void *safe_realloc(void *ptr, size_t size)
{
	if(ptr == NULL) return malloc(size);
	else return realloc(ptr, size);
}

#ifndef HAVE_STRERROR
const char *strerror(int errnum)
{
	extern int sys_nerr;
	extern char *sys_errlist[];

	return (errnum < sys_nerr) ?  sys_errlist[errnum]  :  "";
}
#endif

#ifndef HAVE_STRDUP
char *strdup(const char *src)
{
	char *dest;

	if (!(dest = (char *) malloc(strlen(src)+1)))
	return NULL;
	else
	return strcpy(dest, src);
}
#endif
