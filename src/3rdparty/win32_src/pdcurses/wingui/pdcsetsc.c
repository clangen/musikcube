/* Public Domain Curses */

#include "pdcwin.h"
#include "pdccolor.h"

/*man-start**************************************************************

pdcsetsc
--------

### Synopsis

    int PDC_set_blink(bool blinkon);
    int PDC_set_bold(bool boldon);
    void PDC_set_title(const char *title);

### Description

   PDC_set_blink() toggles whether the A_BLINK attribute sets an actual
   blink mode (TRUE), or sets the background color to high intensity
   (FALSE). The default is platform-dependent (FALSE in most cases). It
   returns OK if it could set the state to match the given parameter,
   ERR otherwise.

   PDC_set_bold() toggles whether the A_BOLD attribute selects an actual
   bold font (TRUE), or sets the foreground color to high intensity
   (FALSE). It returns OK if it could set the state to match the given
   parameter, ERR otherwise.

   PDC_set_title() sets the title of the window in which the curses
   program is running. This function may not do anything on some
   platforms.

### Portability
                             X/Open  ncurses  NetBSD
    PDC_set_blink               -       -       -
    PDC_set_bold                -       -       -
    PDC_set_title               -       -       -

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

        /* If SP->termattrs & A_BLINK is on, then text with the A_BLINK  */
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
        /* blinking,  i.e., SP->termattrs & A_BLINK = 0.  Blinking text  */
        /* can be pretty annoying to some people.  You should probably   */
        /* call PDC_set_blink( TRUE) only if there is something to which */
        /* the user _must_ pay attention;  say,  "the nuclear reactor    */
        /* is about to melt down".  Otherwise,  the bolder,  brighter    */
        /* text should be attention-getting enough.                      */

static int reset_attr( const attr_t attr, const bool attron)
{
    if (!SP)
        return ERR;
    if( attron)
        SP->termattrs |= attr;
    else
        SP->termattrs &= ~attr;
    return OK;
}

int PDC_set_blink(bool blinkon)
{
   return( reset_attr( A_BLINK, blinkon));
}

int PDC_set_bold(bool boldon)
{
   return( reset_attr( A_BOLD, boldon));
}
