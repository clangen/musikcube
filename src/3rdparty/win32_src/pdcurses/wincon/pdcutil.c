/* PDCurses */

#include "pdcwin.h"
#ifdef WIN32_LEAN_AND_MEAN
#include <mmsystem.h>
#endif

void PDC_beep(void)
{
    PDC_LOG(("PDC_beep() - called\n"));

    if (!PlaySound((LPCTSTR) SND_ALIAS_SYSTEMDEFAULT, NULL, SND_ALIAS_ID))
        Beep(800, 200);
}

void PDC_napms(int ms)
{
    PDC_LOG(("PDC_napms() - called: ms=%d\n", ms));

    if ((SP->termattrs & A_BLINK) && (GetTickCount() >= pdc_last_blink + 500))
        PDC_blink_text();

    Sleep(ms);
}

const char *PDC_sysname(void)
{
    return "Windows";
}

enum PDC_port PDC_port_val = PDC_PORT_WINCON;
