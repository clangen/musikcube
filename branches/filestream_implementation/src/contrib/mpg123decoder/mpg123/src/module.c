/*
	module.c: modular code loader

	copyright 1995-2008 by the mpg123 project - free software under the terms of the LGPL 2.1
	see COPYING and AUTHORS files in distribution or http://mpg123.org
	initially written by Nicholas J Humfrey
*/

#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <ltdl.h>

#include "mpg123app.h"
#include "debug.h"

#ifndef HAVE_LTDL
#error Cannot build without LTDL library support
#endif

#define MODULE_FILE_SUFFIX		".la"
#define MODULE_SYMBOL_PREFIX 	"mpg123_"
#define MODULE_SYMBOL_SUFFIX 	"_module_info"

/* It's nasty to hardcode that here...
   also it does need hacking around libtool's hardcoded .la paths:
   When the .la file is in the same dir as .so file, you need libdir='.' in there.
   Also, all this crap doesn't actually work on Win32 (for what I initially intended it). */
#ifdef WIN32
#define RELMOD "\\..\\lib\\mpg123" /* I suspect that win32api would accept "/", too */
#else
#define RELMOD "/../lib/mpg123"
#endif

static char *get_the_cwd(); /* further down... */
static char *get_module_dir()
{
	/* Either PKGLIBDIR is accessible right away or we assume ../lib/mpg123 from binpath. */
	DIR* dir = NULL;
	char *moddir = NULL;

	dir = opendir(PKGLIBDIR);
	if(dir != NULL)
	{
		size_t l = strlen(PKGLIBDIR);
		moddir = malloc(l+1);
		if(moddir != NULL)
		{
			strcpy(moddir, PKGLIBDIR);
			moddir[l] = 0;
		}
		closedir(dir);
	}
	else
	{
		size_t l = strlen(binpath) + strlen(RELMOD);
		moddir = malloc(l+1);
		if(moddir != NULL)
		{
			snprintf(moddir, l+1, "%s%s", binpath, RELMOD);
			moddir[l] = 0;
		}
	}
	return moddir;
}

/* Open a module */
mpg123_module_t*
open_module( const char* type, const char* name )
{
	lt_dlhandle handle = NULL;
	mpg123_module_t *module = NULL;
	char* module_path = NULL;
	size_t module_path_len = 0;
	char* module_symbol = NULL;
	size_t module_symbol_len = 0;
	char *workdir = NULL;
	char *moddir  = NULL;
	workdir = get_the_cwd();
	moddir  = get_module_dir();
	if(workdir == NULL || moddir == NULL)
	{
		error("Failure getting workdir or moddir!");
		if(workdir != NULL) free(workdir);
		if(moddir  != NULL) free(moddir);
		return NULL;
	}

	/* Initialize libltdl */
	if (lt_dlinit()) error( "Failed to initialise libltdl" );

	chdir(moddir);
	/* Work out the path of the module to open */
	module_path_len = strlen(type) + 1 + strlen(name) + strlen(MODULE_FILE_SUFFIX) + 1;
	module_path = malloc( module_path_len );
	if (module_path == NULL) {
		error1( "Failed to allocate memory for module name: %s", strerror(errno) );
		goto om_bad;
	}
	snprintf( module_path, module_path_len, "%s_%s%s", type, name, MODULE_FILE_SUFFIX );
	/* Display the path of the module created */
	debug1( "Module path: %s", module_path );

	/* Open the module */
	handle = lt_dlopen( module_path );
	free( module_path );
	if (handle==NULL) {
		error2( "Failed to open module %s: %s", name, lt_dlerror() );
		goto om_bad;
	}
	
	/* Work out the symbol name */
	module_symbol_len = strlen( MODULE_SYMBOL_PREFIX ) +
						strlen( type )  +
						strlen( MODULE_SYMBOL_SUFFIX ) + 1;
	module_symbol = malloc(module_symbol_len);
	snprintf( module_symbol, module_symbol_len, "%s%s%s", MODULE_SYMBOL_PREFIX, type, MODULE_SYMBOL_SUFFIX );
	debug1( "Module symbol: %s", module_symbol );
	
	/* Get the information structure from the module */
	module = (mpg123_module_t*)lt_dlsym(handle, module_symbol );
	free( module_symbol );
	if (module==NULL) {
		error1( "Failed to get module symbol: %s", lt_dlerror() );
		goto om_latebad;
	}
	
	/* Check the API version */
	if (MPG123_MODULE_API_VERSION > module->api_version) {
		error( "API version of module is too old" );
		goto om_latebad;
	} else if (MPG123_MODULE_API_VERSION > module->api_version) {
		error( "API version of module is too new" );
		goto om_latebad;
	}

	/* Store handle in the data structure */
	module->handle = handle;

	goto om_end;
om_latebad:
	lt_dlclose( handle );
om_bad:
	module = NULL;
om_end:
	chdir(workdir);
	free(moddir);
	free(workdir);
	return module;
}


void close_module( mpg123_module_t* module )
{
	lt_dlhandle handle = module->handle;
	int err = lt_dlclose( handle );
	
	if (err) error1("Failed to close module: %s", lt_dlerror() );

}

#define PATH_STEP 50
static char *get_the_cwd()
{
	size_t bs = PATH_STEP;
	char *buf = malloc(bs);
	while((buf != NULL) && getcwd(buf, bs) == NULL)
	{
		char *buf2;
		buf2 = realloc(buf, bs+=PATH_STEP);
		if(buf2 == NULL){ free(buf); buf = NULL; }
		else debug1("pwd: increased buffer to %lu", (unsigned long)bs);

		buf = buf2;
	}
	return buf;
}

void list_modules()
{
	DIR* dir = NULL;
	struct dirent *dp = NULL;
	char *workdir = NULL;
	char *moddir  = NULL;

	moddir = get_module_dir();
	/* Open the module directory */
	dir = opendir(moddir);
	if (dir==NULL) {
		error2("Failed to open the module directory (%s): %s\n", PKGLIBDIR, strerror(errno));
		free(moddir);
		exit(-1);
	}
	
	workdir = get_the_cwd();
	if(chdir(moddir) != 0)
	{
		error2("Failed to enter module directory (%s): %s\n", PKGLIBDIR, strerror(errno));
		closedir( dir );
		free(workdir);
		free(moddir);
		exit(-1);
	}
	/* Display the program title */
	/* print_title(stderr); */

	/* List the output modules */
	printf("\n");
	printf("Available modules\n");
	printf("-----------------\n");
	
	while( (dp = readdir(dir)) != NULL ) {
		struct stat fst;
		if(stat(dp->d_name, &fst) != 0) continue;
		if(S_ISREG(fst.st_mode)) /* Allow links? */
		{
			char* ext = dp->d_name + strlen( dp->d_name ) - strlen( MODULE_FILE_SUFFIX );
			if (strcmp(ext, MODULE_FILE_SUFFIX) == 0)
			{
				char *module_name = NULL;
				char *module_type = NULL;
				char *uscore_pos = NULL;
				mpg123_module_t *module = NULL;
				
				/* Extract the module type */
				module_type = strdup( dp->d_name );
				uscore_pos = strchr( module_type, '_' );
				if (uscore_pos==NULL) continue;
				if (uscore_pos>=module_type+strlen(module_type)+1) continue;
				*uscore_pos = '\0';
				
				/* Extract the short name of the module */
				module_name = strdup( dp->d_name + strlen( module_type ) + 1 );
				module_name[ strlen( module_name ) - strlen( MODULE_FILE_SUFFIX ) ] = '\0';
				
				/* Open the module */
				module = open_module( module_type, module_name );
				if (module) {
					printf("%-15s%s  %s\n", module->name, module_type, module->description );
				
					/* Close the module again */
					close_module( module );
				}
				
				free( module_name );
			}
		}
	}

	chdir(workdir);
	free(workdir);
	closedir( dir );
	free(moddir);
	exit(0);
}


