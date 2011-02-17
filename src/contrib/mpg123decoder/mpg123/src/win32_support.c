#include "config.h"
#include "mpg123app.h"
#include "debug.h"

#ifdef WANT_WIN32_UNICODE
int win32_cmdline_utf8(int * argc, char *** argv)
{
	wchar_t **argv_wide;
	char *argvptr;
	int argcounter;

	/* That's too lame. */
	if(argv == NULL || argc == NULL) return -1;

	argv_wide = CommandLineToArgvW(GetCommandLineW(), argc);
	if(argv_wide == NULL){ error("Cannot get wide command line."); return -1; }

	*argv = (char **)calloc(sizeof (char *), *argc);
	if(*argv == NULL){ error("Cannot allocate memory for command line."); return -1; }

	for(argcounter = 0; argcounter < *argc; argcounter++)
	{
		win32_wide_utf8(argv_wide[argcounter], (const char **)&argvptr, NULL);
		(*argv)[argcounter] = argvptr;
	}
	LocalFree(argv_wide); /* We don't need it anymore */

	return 0;
}

void win32_cmdline_free(int argc, char **argv)
{
	int i;

	if(argv == NULL) return;

	for(i=0; i<argc; ++i) free(argv[i]);
}
#endif /* WIN32_WANT_UNICODE */

void win32_set_priority (const int arg)
{
	DWORD proc_result = 0;
	HANDLE current_proc = NULL;
	if (arg) {
	  if ((current_proc = GetCurrentProcess()))
	  {
	    switch (arg) {
	      case -2: proc_result = SetPriorityClass(current_proc, IDLE_PRIORITY_CLASS); break;  
	      case -1: proc_result = SetPriorityClass(current_proc, BELOW_NORMAL_PRIORITY_CLASS); break;  
	      case 0: proc_result = SetPriorityClass(current_proc, NORMAL_PRIORITY_CLASS); break; /*default priority*/
	      case 1: proc_result = SetPriorityClass(current_proc, ABOVE_NORMAL_PRIORITY_CLASS); break;
	      case 2: proc_result = SetPriorityClass(current_proc, HIGH_PRIORITY_CLASS); break;
	      case 3: proc_result = SetPriorityClass(current_proc, REALTIME_PRIORITY_CLASS); break;
	      default: fprintf(stderr,"Unknown priority class specified\n");
	    }
	    if(!proc_result) {
	      fprintf(stderr,"SetPriorityClass failed\n");
	    }
	  }
	  else {
	    fprintf(stderr,"GetCurrentProcess failed\n");
	  }
	}
}
