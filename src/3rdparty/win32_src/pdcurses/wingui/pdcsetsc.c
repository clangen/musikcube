/* Public Domain Curses */

#include "pdcwin.h"

/*man-start**************************************************************

  Name:                                                         pdcsetsc

  Synopsis:
        int PDC_set_blink(bool blinkon);
        void PDC_set_title(const char *title);

  Description:
        PDC_set_blink() toggles whether the A_BLINK attribute sets an
        actual blink mode (TRUE), or sets the background color to high
        intensity (FALSE). The default is platform-dependent (FALSE in
        most cases). It returns OK if it could set the state to match
        the given parameter, ERR otherwise. Current platforms also
        adjust the value of COLORS according to this function -- 16 for
        FALSE, and 8 for TRUE.

        PDC_set_title() sets the title of the window in which the curses
        program is running. This function may not do anything on some
        platforms. (Currently it only works in Win32 and X11.)

  Portability                                X/Open    BSD    SYS V
        PDC_set_blink                           -       -       -
        PDC_set_title                           -       -       -

**man-end****************************************************************/

                /* Note that the following is a line-for-line   */
                /* copy of the SDL version of this function.    */
int PDC_curs_set(int visibility)
{
    int ret_vis;

    PDC_LOG(("PDC_curs_set() - called: visibility=%d\n", visibility));

    ret_vis = SP->visibility;

    SP->visibility = visibility;

    PDC_gotoyx(SP->cursrow, SP->curscol);

    return ret_vis;
}

void PDC_set_title(const char *title)
{
    extern HWND PDC_hWnd;
#ifdef PDC_WIDE
    wchar_t wtitle[512];
#endif
    PDC_LOG(("PDC_set_title() - called:<%s>\n", title));

#ifdef PDC_WIDE
    PDC_mbstowcs(wtitle, title, 511);
    SetWindowTextW( PDC_hWnd, wtitle);
#else
    SetWindowTextA( PDC_hWnd, title);
#endif
}

        /* If PDC_really_blinking is TRUE,  then text with the A_BLINK   */
        /* attribute will actually blink.  Otherwise,  such text will    */
        /* be shown with higher color intensity (the R, G, and B values  */
        /* are averaged with pure white).  See pdcdisp.c for details of  */
        /* how this is done.                                             */
        /*     Unlike on other PDCurses platforms,  this doesn't require */
        /* decreasing the number of colors by half.  Also,  the choice   */
        /* indicated by 'blinkon' will actually be respected,  so OK is  */
        /* always returned (most platforms don't actually support        */
        /* blinking).                                                    */
        /*      The default behavior is to not show A_BLINK text as      */
        /* blinking,  i.e.,  PDC_really_blinking = FALSE.  Blinking text */
        /* can be pretty annoying to some people.  You should probably   */
        /* call PDC_set_blink( TRUE) only if there is something to which */
        /* the user _must_ pay attention;  say,  "the nuclear reactor    */
        /* is about to melt down".  Otherwise,  the bolder,  brighter    */
        /* text should be attention-getting enough.                      */

int PDC_really_blinking = FALSE;

int PDC_set_blink(bool blinkon)
{
    PDC_really_blinking = blinkon;
    return OK;
}
