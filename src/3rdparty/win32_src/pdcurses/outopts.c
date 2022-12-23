/* PDCursesMod */

#include <curspriv.h>
#include <assert.h>

/*man-start**************************************************************

outopts
-------

### Synopsis

    int clearok(WINDOW *win, bool bf);
    int idlok(WINDOW *win, bool bf);
    void idcok(WINDOW *win, bool bf);
    void immedok(WINDOW *win, bool bf);
    int leaveok(WINDOW *win, bool bf);
    int setscrreg(int top, int bot);
    int wsetscrreg(WINDOW *win, int top, int bot);
    int wgetscrreg(const WINDOW *win, int *top, int *bot);
    int scrollok(WINDOW *win, bool bf);

    int raw_output(bool bf);

    bool is_cleared(const WINDOW *win);
    bool is_idlok(const WINDOW *win);
    bool is_idcok(const WINDOW *win);
    bool is_immedok(const WINDOW *win);
    bool is_leaveok(const WINDOW *win);
    bool is_scrollok(const WINDOW *win);

### Description

   With clearok(), if bf is TRUE, the next call to wrefresh() with this
   window will clear the screen completely and redraw the entire screen.

   immedok(), called with a second argument of TRUE, causes an automatic
   wrefresh() every time a change is made to the specified window.

   Normally, the hardware cursor is left at the location of the window
   being refreshed. leaveok() allows the cursor to be left wherever the
   update happens to leave it. It's useful for applications where the
   cursor is not used, since it reduces the need for cursor motions. If
   possible, the cursor is made invisible when this option is enabled.

   wsetscrreg() sets a scrolling region in a window; "top" and "bot" are
   the line numbers for the top and bottom margins. If this option and
   scrollok() are enabled, any attempt to move off the bottom margin
   will cause all lines in the scrolling region to scroll up one line.
   setscrreg() is the stdscr version.

   wgetscrreg() gets the top and bottom margins as set in wsetscrreg().

   idlok(), idcok(), is_idlok() and is_idcok() do nothing in PDCursesMod,
   but are provided for compatibility with other curses implementations.

   raw_output() enables the output of raw characters using the standard
   *add* and *ins* curses functions (that is, it disables translation of
   control characters).

   is_cleared() reports whether the specified window causes clear at next
   refresh.

   is_immedok() reports whether the specified window is in immedok mode.

   is_leaveok() reports whether the specified window is in leaveok mode.

   is_scrollok() reports whether the specified window allows scrolling.

### Return Value

   All functions returning integers return OK on success and ERR on
   error.

   is_cleared(), is_immedok(), is_leaveok() and is_scrollok() are
   booleans and return TRUE or FALSE.

   is_idlok() and is_idcok() are provided for compatibility with other
   curses implementations.  They have no real meaning in PDCursesMod and
   will always return FALSE.

### Portability
                             X/Open  ncurses  NetBSD
    clearok                     Y       Y       Y
    idlok                       Y       Y       Y
    idcok                       Y       Y       Y
    immedok                     Y       Y       Y
    leaveok                     Y       Y       Y
    setscrreg                   Y       Y       Y
    wsetscrreg                  Y       Y       Y
    wgetscrreg                  -       Y       -
    scrollok                    Y       Y       Y
    is_cleared                  -       Y       -
    is_idlok                    -       Y       -
    is_idcok                    -       Y       -
    is_immedok                  -       Y       -
    is_leaveok                  -       Y       Y
    is_scrollok                 -       Y       -
    raw_output                  -       -       -

**man-end****************************************************************/

int clearok(WINDOW *win, bool bf)
{
    PDC_LOG(("clearok() - called\n"));

    assert( win);
    if (!win)
        return ERR;

    win->_clear = bf;

    return OK;
}

int idlok(WINDOW *win, bool bf)
{
    INTENTIONALLY_UNUSED_PARAMETER( win);
    INTENTIONALLY_UNUSED_PARAMETER( bf);
    PDC_LOG(("idlok() - called\n"));

    return OK;
}

void idcok(WINDOW *win, bool bf)
{
    INTENTIONALLY_UNUSED_PARAMETER( win);
    INTENTIONALLY_UNUSED_PARAMETER( bf);
    PDC_LOG(("idcok() - called\n"));
}

void immedok(WINDOW *win, bool bf)
{
    PDC_LOG(("immedok() - called\n"));

    if (win)
        win->_immed = bf;
}

int leaveok(WINDOW *win, bool bf)
{
    PDC_LOG(("leaveok() - called\n"));

    assert( win);
    if (!win)
        return ERR;

    win->_leaveit = bf;

    curs_set(!bf);

    return OK;
}

int setscrreg(int top, int bottom)
{
    PDC_LOG(("setscrreg() - called: top %d bottom %d\n", top, bottom));

    return wsetscrreg(stdscr, top, bottom);
}

int wsetscrreg(WINDOW *win, int top, int bottom)
{
    PDC_LOG(("wsetscrreg() - called: top %d bottom %d\n", top, bottom));

    assert( win);
    if (win && 0 <= top && top <= win->_cury &&
        win->_cury <= bottom && bottom < win->_maxy)
    {
        win->_tmarg = top;
        win->_bmarg = bottom;

        return OK;
    }
    else
        return ERR;
}

int wgetscrreg(const WINDOW *win, int *top, int *bot)
{
    PDC_LOG(("wgetscrreg() - called\n"));

    assert( win);
    assert( top);
    assert( bot);
    if (!win || !top || !bot)
        return ERR;

    *top = win->_tmarg;
    *bot = win->_bmarg;

    return OK;
}

int scrollok(WINDOW *win, bool bf)
{
    PDC_LOG(("scrollok() - called\n"));

    assert( win);
    if (!win)
        return ERR;

    win->_scroll = bf;

    return OK;
}

bool is_cleared(const WINDOW *win)
{
    PDC_LOG(("is_cleared() - called\n"));

    assert( win);
    if (!win)
        return FALSE;

    return win->_clear;
}

bool is_idlok(const WINDOW *win)
{
    PDC_LOG(("is_idlok() - called\n"));

    INTENTIONALLY_UNUSED_PARAMETER( win);
    assert( win);

    return FALSE;
}

bool is_idcok(const WINDOW *win)
{
    PDC_LOG(("is_idcok() - called\n"));

    INTENTIONALLY_UNUSED_PARAMETER( win);
    assert( win);

    return FALSE;
}

bool is_immedok(const WINDOW *win)
{
    PDC_LOG(("is_immedok() - called\n"));

    assert( win);
    if (!win)
        return FALSE;

    return win->_immed;
}

bool is_leaveok(const WINDOW *win)
{
    PDC_LOG(("is_leaveok() - called\n"));

    assert( win);
    if (!win)
        return FALSE;

    return win->_leaveit;
}

bool is_scrollok(const WINDOW *win)
{
    PDC_LOG(("is_scrollok() - called\n"));

    assert( win);
    if (!win)
        return FALSE;

    return win->_scroll;
}

int raw_output(bool bf)
{
    PDC_LOG(("raw_output() - called\n"));

    assert( SP);
    if (!SP)
        return ERR;

    SP->raw_out = bf;

    return OK;
}
