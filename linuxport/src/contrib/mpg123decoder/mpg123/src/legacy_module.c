/*
	legacy_module.c: dummy interface to modular code loader for legacy build system

	copyright 2008 by the mpg123 project - free software under the terms of the LGPL 2.1
	see COPYING and AUTHORS files in distribution or http://mpg123.org
	initially written by Nicholas J Humfrey
*/

#include "mpg123app.h"
#include "debug.h"

/* A single module is staticly compiled in for each type */
extern mpg123_module_t mpg123_output_module_info;
/* extern mpg123_module_t mpg123_input_module_info; */


/* Open a module */
mpg123_module_t*
open_module( const char* type, const char* name )
{
	mpg123_module_t *mod = NULL;
	
	/* Select the module info structure, based on the desired type */
	if (strcmp(type, "output")==0) {
		mod = &mpg123_output_module_info;
/*
	} else if (strcmp(type, "input")==0) {
		mod = &mpg123_input_module_info;
*/
	} else {
		error1("Unable to open module type '%s'.", type);
		return NULL;
	}
	
	/* Check the module compiled in is the module requested */
	if (strcmp(name, mod->name)!=0) {
		error1("Unable to open requesed module '%s'.", name);
		error1("The only available staticly compiled module is '%s'.", mod->name);
		return NULL;
	}
	
	/* Debugging info */
	debug1("Details of static module type '%s':", type);
	debug1("  api_version=%d", mod->api_version);
	debug1("  name=%s", mod->name);
	debug1("  description=%s", mod->description);
	debug1("  revision=%s", mod->revision);
	debug1("  handle=%x", (unsigned int)mod->handle);

	return mod;
}


void close_module( mpg123_module_t* module )
{
	debug("close_module()");
	
	/* Module was never really 'loaded', so nothing to do here. */
}


void list_modules()
{
	debug("list_modules()" );

	printf("\nmpg123 has been built in legacy mode - dynamic modules are not available.\n");
	
	printf("Available modules\n");
	printf("-----------------\n");
	printf("%-15s%s  %s\n", mpg123_output_module_info.name, "output", mpg123_output_module_info.description );

}


