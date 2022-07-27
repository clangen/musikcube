/* Public Domain Curses */

#include "pdcwin.h"
#ifdef WIN32_LEAN_AND_MEAN
#include <mmsystem.h>
#include <process.h>
#endif

static volatile int _beep_count = 0;

static void beep_thread(LPVOID lpParameter)
{
    INTENTIONALLY_UNUSED_PARAMETER( lpParameter);
    while( _beep_count)
    {
        if (!PlaySound((LPCTSTR) SND_ALIAS_SYSTEMDEFAULT, NULL, SND_ALIAS_ID))
            Beep(800, 200);
        _beep_count--;
    }
}

void PDC_beep(void)
{
    PDC_LOG(("PDC_beep() - called\n"));
    _beep_count++;
    if( _beep_count == 1)
        _beginthread( beep_thread, 0, NULL);
}

void PDC_napms(int ms)     /* 'ms' = milli,  _not_ microseconds! */
{
    /* RR: keep GUI window responsive while PDCurses sleeps */
    MSG msg;
    DWORD curr_ms = GetTickCount( );
    const DWORD milliseconds_sleep_limit = ms + curr_ms;
    extern bool PDC_bDone;

    PDC_LOG(("PDC_napms() - called: ms=%d\n", ms));

    /* Pump all pending messages from WIN32 to the window handler */
    while( !PDC_bDone && curr_ms < milliseconds_sleep_limit )
    {
        const DWORD max_sleep_ms = 50;      /* check msgs 20 times/second */
        DWORD sleep_millisecs;

        while( PeekMessage(&msg, NULL, 0, 0, PM_REMOVE) )
        {
           TranslateMessage(&msg);
           DispatchMessage(&msg);
        }
        curr_ms = GetTickCount( );
        sleep_millisecs = milliseconds_sleep_limit - curr_ms;
        if( sleep_millisecs > max_sleep_ms)
            sleep_millisecs = max_sleep_ms;
        Sleep( sleep_millisecs);
        curr_ms += sleep_millisecs;
    }
}

const char *PDC_sysname(void)
{
   return "WinGUI";
}

enum PDC_port PDC_port_val = PDC_PORT_WINGUI;
