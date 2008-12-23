/*
	module: module loading and listing interface

	copyright ?-2007 by the mpg123 project - free software under the terms of the LGPL 2.1
	see COPYING and AUTHORS files in distribution or http://mpg123.org
*/

#ifndef _MPG123_MODULE_H_
#define _MPG123_MODULE_H_

/* Pulled in by mpg123app.h! */

#ifdef HAVE_LTDL
#include <ltdl.h>
#endif

#define MPG123_MODULE_API_VERSION		(1)

/* The full structure is delared in audio.h */
struct audio_output_struct;

typedef struct mpg123_module_struct {
	const int api_version;						/* module API version number */

	const char* name;							/* short name of the module */
	const char* description;					/* description of what the module does */
	const char* revision;						/* source code revision */
	
#ifdef HAVE_LTDL
	lt_dlhandle handle;							/* ltdl handle - set by open_module */
#else
	void* handle;
#endif

	/* Initialisers - set to NULL if unsupported by module */
	int (*init_output)(struct audio_output_struct* ao);		/* audio output - returns 0 on success */

} mpg123_module_t;



/* ------ Declarations from "module.c" ------ */

mpg123_module_t* open_module( const char* type, const char* name );
void close_module( mpg123_module_t* module );
void list_modules();

#endif
