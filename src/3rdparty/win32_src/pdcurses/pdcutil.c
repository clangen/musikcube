/* Public Domain Curses */

#include "pdcwin.h"
#include "curses.h"

void PDC_beep(void)
{
    PDC_LOG(("PDC_beep() - called\n"));

/*  MessageBeep(MB_OK); */
    MessageBeep(0XFFFFFFFF);
}


void PDC_napms(int ms)     /* 'ms' = milli,  _not_ microseconds! */
{
    /* RR: keep GUI window responsive while PDCurses sleeps */
    MSG msg;
    DWORD start, end, delta;
    extern bool PDC_bDone;

    start = GetTickCount();

    //PDC_LOG(("PDC_napms() - called: ms=%d\n", ms));

    /* Pump all pending messages from WIN32 to the window handler */
    while( !PDC_bDone && PeekMessage(&msg, NULL, 0, 0, PM_REMOVE) )
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    end = GetTickCount();
    delta = end - start;
    delta = ms > delta ? ms - delta : 0;
    Sleep(delta);
}

const char *PDC_sysname(void)
{
   return "Win32a";
}

const PDC_version_info PDC_version = { PDC_PORT_WIN32A,
          PDC_VER_MAJOR, PDC_VER_MINOR, PDC_VER_CHANGE,
          sizeof( chtype),
#ifdef PDC_WIDE
          TRUE,
#else
          FALSE,
#endif
#ifdef PDC_FORCE_UTF8
          TRUE,
#else
          FALSE,
#endif
          };
