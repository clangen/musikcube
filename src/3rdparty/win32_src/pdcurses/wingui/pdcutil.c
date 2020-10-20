/* Public Domain Curses */

#include "pdcwin.h"
#ifdef WIN32_LEAN_AND_MEAN
#include <mmsystem.h>
#endif

extern CRITICAL_SECTION PDC_cs;

void PDC_beep(void)
{
    PDC_LOG(("PDC_beep() - called\n"));

    LeaveCriticalSection(&PDC_cs);
    if (!PlaySound((LPCTSTR) SND_ALIAS_SYSTEMDEFAULT, NULL, SND_ALIAS_ID))
        Beep(800, 200);
    EnterCriticalSection(&PDC_cs);
}

void PDC_napms(int ms)     /* 'ms' = milli,  _not_ microseconds! */
{
    /* RR: keep GUI window responsive while PDCurses sleeps */

    PDC_LOG(("PDC_napms() - called: ms=%d\n", ms));

    LeaveCriticalSection(&PDC_cs);
    Sleep(ms);
    EnterCriticalSection(&PDC_cs);
}

const char *PDC_sysname(void)
{
   return "WinGUI";
}

enum PDC_port PDC_port_val = PDC_PORT_WINGUI;
