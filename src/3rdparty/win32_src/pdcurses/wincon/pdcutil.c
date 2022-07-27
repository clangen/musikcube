/* PDCurses */

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

void PDC_napms(int ms)
{
    PDC_LOG(("PDC_napms() - called: ms=%d\n", ms));

    if ((SP->termattrs & A_BLINK) && (GetTickCount() >= pdc_last_blink + 500))
        PDC_blink_text();

    if( ms)
       Sleep(ms);
}

const char *PDC_sysname(void)
{
    return "Windows";
}

enum PDC_port PDC_port_val = PDC_PORT_WINCON;
