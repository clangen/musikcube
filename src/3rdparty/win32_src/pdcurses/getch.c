/* PDCurses */

#include <curspriv.h>
#include <assert.h>

/*man-start**************************************************************

getch
-----

### Synopsis

    int getch(void);
    int wgetch(WINDOW *win);
    int mvgetch(int y, int x);
    int mvwgetch(WINDOW *win, int y, int x);
    int ungetch(int ch);
    int flushinp(void);

    int get_wch(wint_t *wch);
    int wget_wch(WINDOW *win, wint_t *wch);
    int mvget_wch(int y, int x, wint_t *wch);
    int mvwget_wch(WINDOW *win, int y, int x, wint_t *wch);
    int unget_wch(const wchar_t wch);

    unsigned long PDC_get_key_modifiers(void);
    int PDC_return_key_modifiers(bool flag);

### Description

   With the getch(), wgetch(), mvgetch(), and mvwgetch() functions, a
   character is read from the terminal associated with the window. In
   nodelay mode, if there is no input waiting, the value ERR is
   returned. In delay mode, the program will hang until the system
   passes text through to the program. Depending on the setting of
   cbreak(), this will be after one character or after the first
   newline. Unless noecho() has been set, the character will also be
   echoed into the designated window.

   If keypad() is TRUE, and a function key is pressed, the token for
   that function key will be returned instead of the raw characters.
   Possible function keys are defined in <curses.h> with integers
   starting at KEY_OFFSET, whose names begin with KEY_.

   If nodelay(win, TRUE) has been called on the window and no input is
   waiting, the value ERR is returned.

   ungetch() places ch back onto the input queue to be returned by the
   next call to wgetch().

   flushinp() throws away any type-ahead that has been typed by the user
   and has not yet been read by the program.

   wget_wch() is the wide-character version of wgetch(), available when
   PDCurses is built with the PDC_WIDE option. It takes a pointer to a
   wint_t rather than returning the key as an int, and instead returns
   KEY_CODE_YES if the key is a function key. Otherwise, it returns OK
   or ERR. It's important to check for KEY_CODE_YES; on most Curses
   implementations (not PDCursesMod),  regular wide characters can have
   the same values as function key codes.

   unget_wch() puts a wide character on the input queue.

   PDC_get_key_modifiers() returns the keyboard modifiers (shift,
   control, alt, numlock) effective at the time of the last getch()
   call. Use the macros PDC_KEY_MODIFIER_* to determine which
   modifier(s) were set. PDC_return_key_modifiers() tells getch() to
   return modifier keys pressed alone as keystrokes (KEY_ALT_L, etc.).
   These may not work on all platforms.

   NOTE: getch() and ungetch() are implemented as macros, to avoid
   conflict with many DOS compiler's runtime libraries.

### Return Value

   These functions return ERR or the value of the character, meta
   character or function key token.

### Portability
                             X/Open  ncurses  NetBSD
    getch                       Y       Y       Y
    wgetch                      Y       Y       Y
    mvgetch                     Y       Y       Y
    mvwgetch                    Y       Y       Y
    ungetch                     Y       Y       Y
    flushinp                    Y       Y       Y
    get_wch                     Y       Y       Y
    wget_wch                    Y       Y       Y
    mvget_wch                   Y       Y       Y
    mvwget_wch                  Y       Y       Y
    unget_wch                   Y       Y       Y
    PDC_get_key_modifiers       -       -       -

**man-end****************************************************************/

#include <stdlib.h>

       /* By default,  the PDC_function_key[] array contains 0       */
       /* (i.e., there's no key that's supposed to be returned for   */
       /* exit handling), and 22 = Ctrl-V (i.e.,  hit Ctrl-V to      */
       /* paste text from the clipboard into the key queue);  then   */
       /* no key by default to enlarge/decrease font size or to      */
       /* select a font from the font dialog;  then Ctrl-C for copy. */

static int PDC_function_key[PDC_MAX_FUNCTION_KEYS] = { 0, 22, 0, 0, 0, 0, 3 };

/*man-start**************************************************************

Function keys
-------------

### Synopsis

   int PDC_set_function_key( const unsigned function, const int new_key);
   int PDC_get_function_key( const unsigned function);

### Description

   Allows one to set a 'shut down' key,  and reassign hotkeys used for
   copying to/pasting from the clipboard and enlarging and decreasing the
   font size,  and for using the font selection dialog (on platforms where
   these things are possible and implemented).  For example, calling

   PDC_set_function_key( FUNCTION_KEY_SHUT_DOWN, ALT_Q);

   would reset PDCursesMod such that,  if the user clicks on the 'close'
   box, Alt-Q would be added to the key queue.  This would give the app the
   opportunity to shut things down gracefully,  perhaps asking "are you
   sure",  and/or "save changes or discard or cancel",  rather than just
   having the window close (the default behavior).

   Similarly,  one can set FUNCTION_KEY_ABORT to a key which,  when pressed,
   will cause the program to abort gracelessly (no key returned to the
   application).  One would normally use this to enable/disable Ctrl-C or
   Ctrl-Break,  or to set a different 'abort' key so that Ctrl-C can be
   used for copying.

### Return Value

   Returns key code previously set for that function,  or -1 if the
   function does not actually exist.

### Portability

   PDCursesMod-only function.

**man-end****************************************************************/
int PDC_set_function_key( const unsigned function, const int new_key)
{
    int old_key = -1;

    assert( function < PDC_MAX_FUNCTION_KEYS);
    if( function < PDC_MAX_FUNCTION_KEYS)
    {
         old_key = PDC_function_key[function];
         PDC_function_key[function] = new_key;
    }
    return( old_key);
}

int PDC_get_function_key( const unsigned function)
{
    assert( function < PDC_MAX_FUNCTION_KEYS);
    return( PDC_function_key[function]);
}

static int _get_box(int *y_start, int *y_end, int *x_start, int *x_end)
{
    int start, end;

    if (SP->sel_start < SP->sel_end)
    {
        start = SP->sel_start;
        end = SP->sel_end;
    }
    else
    {
        start = SP->sel_end;
        end = SP->sel_start;
    }

    *y_start = start / COLS;
    *x_start = start % COLS;

    *y_end = end / COLS;
    *x_end = end % COLS;

    return (end - start) + (*y_end - *y_start);
}

static bool _highlight(void)
{
    int i, j, y_start, y_end, x_start, x_end;

    if (-1 == SP->sel_start || SP->sel_start == SP->sel_end)
        return( FALSE);

    _get_box(&y_start, &y_end, &x_start, &x_end);

    for (j = y_start; j <= y_end; j++)
        for (i = (j == y_start ? x_start : 0);
             i < (j == y_end ? x_end : COLS); i++)
            curscr->_y[j][i] ^= A_REVERSE;
    return( TRUE);
}

static void _copy(void)
{
#ifdef PDC_WIDE
    wchar_t *wtmp;
# define TMP wtmp
# define MASK A_CHARTEXT
#else
# define TMP tmp
# define MASK 0xff
#endif
    char *tmp;
    long pos;
    int i, j, y_start, y_end, x_start, x_end, len;

    if (-1 == SP->sel_start)
        return;

    len = _get_box(&y_start, &y_end, &x_start, &x_end);

    if (!len)
        return;

#ifdef PDC_WIDE
    wtmp = (wchar_t *)malloc((len + 1) * sizeof(wchar_t));
    len *= 3;
#endif
    tmp = (char *)malloc(len + 1);

    for (j = y_start, pos = 0; j <= y_end; j++)
    {
        for (i = (j == y_start ? x_start : 0);
             i < (j == y_end ? x_end : COLS); i++)
#ifdef PDC_WIDE
            wtmp[pos++] = (wchar_t)( curscr->_y[j][i] & MASK);
#else
            tmp[pos++] = (char)( curscr->_y[j][i] & MASK);
#endif

        while (y_start != y_end && pos > 0 && TMP[pos - 1] == 32)
            pos--;

        if (j < y_end)
            TMP[pos++] = 10;
    }
    TMP[pos] = 0;

#ifdef PDC_WIDE
    pos = (long)PDC_wcstombs(tmp, wtmp, len);
#endif

    PDC_setclipboard(tmp, pos);
    free(tmp);
#ifdef PDC_WIDE
    free(wtmp);
#endif
}

static int _paste(void)
{
#ifdef PDC_WIDE
    wchar_t *wpaste;
# define PASTE wpaste
#else
# define PASTE paste
#endif
    char *paste;
    long len, newmax;
    int key;

    key = PDC_getclipboard(&paste, &len);
    if (PDC_CLIP_SUCCESS != key || !len)
        return -1;

#ifdef PDC_WIDE
    wpaste = (wchar_t *)malloc(len * sizeof(wchar_t));
    len = (long)PDC_mbstowcs(wpaste, paste, len);
#endif
    newmax = len + SP->c_ungind;
    if (newmax > SP->c_ungmax)
    {
        SP->c_ungch = (int *)realloc(SP->c_ungch, newmax * sizeof(int));
        if (!SP->c_ungch)
            return -1;
        SP->c_ungmax = newmax;
    }
    while (len > 1)
        PDC_ungetch(PASTE[--len]);
    key = *PASTE;
#ifdef PDC_WIDE
    free(wpaste);
#endif
    PDC_freeclipboard(paste);
    SP->key_modifiers = 0;

    return key;
}

#define WHEEL_EVENTS (PDC_MOUSE_WHEEL_UP|PDC_MOUSE_WHEEL_DOWN|PDC_MOUSE_WHEEL_RIGHT | PDC_MOUSE_WHEEL_LEFT)

static int _mouse_key(void)
{
    int i, key = KEY_MOUSE;
    const int changes = SP->mouse_status.changes;
    const mmask_t mbe = SP->_trap_mbe;
    bool can_select = !(mbe & (BUTTON1_MOVED | BUTTON1_PRESSED | BUTTON1_RELEASED));
    bool can_paste = !(mbe & BUTTON2_CLICKED);
            /* really means 'can do these things without shift' */

    /* Selection highlighting? */

    if ((can_select || SP->mouse_status.button[0] & BUTTON_SHIFT) && changes & 1)
    {
        i = SP->mouse_status.y * COLS + SP->mouse_status.x;
        switch (SP->mouse_status.button[0] & BUTTON_ACTION_MASK)
        {
        case BUTTON_PRESSED:
            if( _highlight())
                wrefresh(curscr);
            SP->sel_start = SP->sel_end = i;
            return -1;
        case BUTTON_MOVED:
            {
            const bool refresh_needed = _highlight();

            SP->sel_end = i;
            if( _highlight() || refresh_needed)
                wrefresh(curscr);
            return -1;
            }
        case BUTTON_RELEASED:
            _copy();
            return -1;
        }
    }
    else if ((can_paste || SP->mouse_status.button[1] & BUTTON_SHIFT) &&
             changes & 2 && (SP->mouse_status.button[1] &
             BUTTON_ACTION_MASK) == BUTTON_CLICKED)
    {
        return _paste();
    }

    /* Filter unwanted mouse events */

    for (i = 0; i < 3; i++)
    {
        if (changes & (1 << i))
        {
            int shf = i * PDC_BITS_PER_BUTTON;
            short button = SP->mouse_status.button[i] & BUTTON_ACTION_MASK;

            if (   (!(mbe & (BUTTON1_PRESSED << shf)) &&
                    (button == BUTTON_PRESSED))

                || (!(mbe & (BUTTON1_CLICKED << shf)) &&
                    (button == BUTTON_CLICKED))

                || (!(mbe & (BUTTON1_DOUBLE_CLICKED << shf)) &&
                    (button == BUTTON_DOUBLE_CLICKED))

                || (!(mbe & (BUTTON1_TRIPLE_CLICKED << shf)) &&
                    (button == BUTTON_TRIPLE_CLICKED))

                || (!(mbe & (BUTTON1_MOVED << shf)) &&
                    (button == BUTTON_MOVED))

                || (!(mbe & (BUTTON1_RELEASED << shf)) &&
                    (button == BUTTON_RELEASED))
            )
                SP->mouse_status.changes ^= (1 << i);
        }
    }

    if (changes & PDC_MOUSE_MOVED)
    {
        if (!(mbe & (BUTTON1_MOVED|BUTTON2_MOVED|BUTTON3_MOVED | REPORT_MOUSE_POSITION)))
            SP->mouse_status.changes ^= PDC_MOUSE_MOVED;
    }

    if (changes & WHEEL_EVENTS)
    {
        if (!(mbe & MOUSE_WHEEL_SCROLL))
            SP->mouse_status.changes &= ~WHEEL_EVENTS;
    }

    if (!SP->mouse_status.changes)
        return -1;

    /* Check for click in slk area */

    i = PDC_mouse_in_slk(SP->mouse_status.y, SP->mouse_status.x);

    if (i)
    {
        if (SP->mouse_status.button[0] & (BUTTON_PRESSED|BUTTON_CLICKED))
            key = KEY_F(i);
        else
            key = -1;
    }

    return key;
}

/* ftime() is consided obsolete.  But it's all we have for
millisecond precision on older compilers/systems.  We'll
use gettimeofday() when available.        */

#if defined(__TURBOC__) || defined(__EMX__) || defined(__DJGPP__) || \
    defined( __DMC__) || defined(__WATCOMC__) || defined(_MSC_VER)
#include <sys/timeb.h>

long PDC_millisecs( void)
{
    struct timeb t;

    ftime( &t);
    return( (long)t.time * 1000L + (long)t.millitm);
}
#else
#include <sys/time.h>

long PDC_millisecs( void)
{
    struct timeval t;

    gettimeofday( &t, NULL);
    return( t.tv_sec * 1000 + t.tv_usec / 1000);
}
#endif

/* On many systems,  checking for a key hit is quite slow.  If
PDC_check_key( ) returns FALSE,  we can safely stop checking for
a key hit for a millisecond.  This ensures we won't call it more
than 1000 times per second.

On DOS,  it appears that checking the time is so slow that we're
better off (by a small margin) not using this scheme.  */

static bool _fast_check_key( void)
{
#if defined( __DMC__) && !defined( _WIN32)
    return( PDC_check_key( ));
#else
    static long prev_millisecond;
    const long curr_ms = PDC_millisecs( );
    bool rval;

    if( prev_millisecond == curr_ms)
        return( FALSE);
    rval = PDC_check_key( );
    if( !rval)
        prev_millisecond = curr_ms;
    return( rval);
#endif
}


bool PDC_is_function_key( const int key)
{
   return( key >= KEY_MIN && key < KEY_MAX);
}

#define WAIT_FOREVER    -1

int wgetch(WINDOW *win)
{
    int key = ERR, remaining_millisecs;

    PDC_LOG(("wgetch() - called\n"));

    assert( SP);
    assert( win);
    if (!win || !SP)
        return ERR;

    if (SP->delaytenths)
        remaining_millisecs = 100 * SP->delaytenths;
    else
        remaining_millisecs = win->_delayms;
    if( !remaining_millisecs && !win->_nodelay)
        remaining_millisecs = WAIT_FOREVER;

    /* refresh window when wgetch is called if there have been changes
       to it and it is not a pad */

    if (!(win->_flags & _PAD) && ((!win->_leaveit &&
         (win->_begx + win->_curx != SP->curscol ||
          win->_begy + win->_cury != SP->cursrow)) || is_wintouched(win)))
        wrefresh(win);

    /* if ungotten char exists, remove and return it */

    if (SP->c_ungind)
        key = SP->c_ungch[--(SP->c_ungind)];

    /* if normal and data in buffer */

    else if ((!SP->raw_inp && !SP->cbreak) && (SP->c_gindex < SP->c_pindex))
        key = SP->c_buffer[SP->c_gindex++];

    if( key != ERR)
    {
        if( key == KEY_RESIZE)
            resize_term( 0, 0);
        return( key);
    }
    /* prepare to buffer data */

    SP->c_pindex = 0;
    SP->c_gindex = 0;

    /* to get here, no keys are buffered. go and get one. */

    for (;;)            /* loop for any buffering */
    {
        /* is there a keystroke ready? */

        while( !_fast_check_key())
        {
            /* if not, handle timeout() and halfdelay() */
            int nap_time = 50;

            if (remaining_millisecs != WAIT_FOREVER)
            {
                if (!remaining_millisecs)
                    return ERR;
                if( nap_time > remaining_millisecs)
                    nap_time = remaining_millisecs;
                remaining_millisecs -= nap_time;
            }
            napms( nap_time);
        }

        /* if there is, fetch it */

        key = PDC_get_key();

        /* loop back if we did not get a key yet */

        if (key == -1)
            continue;

        /* filter mouse events; translate mouse clicks in the slk
           area to function keys (especially copy + pase) */

        if( key == KEY_MOUSE)
        {
            key = _mouse_key();
        }
        else
        {

            /* copy or paste? */
#ifndef _WIN32
            if (SP->key_modifiers & PDC_KEY_MODIFIER_SHIFT)
#endif
            {
                if (PDC_function_key[FUNCTION_KEY_COPY] == key)
                {
                    _copy();
                    continue;
                }
                else if (PDC_function_key[FUNCTION_KEY_PASTE] == key)
                    key = _paste();
            }

        }

        /* filter special keys if not in keypad mode */

        if( key != KEY_RESIZE && PDC_is_function_key( key) && !win->_use_keypad)
            key = -1;

        /* unwanted key? loop back */

        if (key == -1)
            continue;

        if( _highlight())
            wrefresh(curscr);
        SP->sel_start = SP->sel_end = -1;

        /* translate CR */

        if (key == '\r' && SP->autocr && !SP->raw_inp)
            key = '\n';

        /* if echo is enabled */

        if (SP->echo && !PDC_is_function_key( key))
        {
            waddch(win, key);
            wrefresh(win);
        }

        /* if no buffering */

        if (SP->raw_inp || SP->cbreak)
        {
            if( key == KEY_RESIZE)
                resize_term( 0, 0);
            return key;
        }

        /* if no overflow, put data in buffer */

        if (key == '\b')
        {
            if (SP->c_pindex > SP->c_gindex)
                SP->c_pindex--;
        }
        else
            if (SP->c_pindex < _INBUFSIZ - 2)
                SP->c_buffer[SP->c_pindex++] = key;

        /* if we got a line */

        if (key == '\n' || key == '\r')
        {
            if( SP->c_buffer[SP->c_gindex] == KEY_RESIZE)
                resize_term( 0, 0);
            return SP->c_buffer[SP->c_gindex++];
        }
    }
}

int mvgetch(int y, int x)
{
    PDC_LOG(("mvgetch() - called\n"));

    if (move(y, x) == ERR)
        return ERR;

    return wgetch(stdscr);
}

int mvwgetch(WINDOW *win, int y, int x)
{
    PDC_LOG(("mvwgetch() - called\n"));

    if (wmove(win, y, x) == ERR)
        return ERR;

    return wgetch(win);
}

int PDC_ungetch(int ch)
{
    PDC_LOG(("ungetch() - called\n"));

    if (SP->c_ungind >= SP->c_ungmax)   /* pushback stack full */
        return ERR;

    SP->c_ungch[SP->c_ungind++] = ch;

    return OK;
}

int flushinp(void)
{
    PDC_LOG(("flushinp() - called\n"));

    assert( SP);
    if (!SP)
        return ERR;

    PDC_flushinp();

    SP->c_gindex = 1;       /* set indices to kill buffer */
    SP->c_pindex = 0;
    SP->c_ungind = 0;       /* clear SP->c_ungch array */

    return OK;
}

unsigned long PDC_get_key_modifiers(void)
{
    PDC_LOG(("PDC_get_key_modifiers() - called\n"));

    assert( SP);
    if (!SP)
        return (unsigned long)ERR;

    return SP->key_modifiers;
}

int PDC_return_key_modifiers(bool flag)
{
    PDC_LOG(("PDC_return_key_modifiers() - called\n"));

    assert( SP);
    if (!SP)
        return ERR;

    SP->return_key_modifiers = flag;
    return PDC_modifiers_set();
}

#ifdef PDC_WIDE
int wget_wch(WINDOW *win, wint_t *wch)
{
    int key;

    PDC_LOG(("wget_wch() - called\n"));

    assert( wch);
    if (!wch)
        return ERR;

    key = wgetch(win);

    if (key == ERR)
        return ERR;

    *wch = (wint_t)key;

    return PDC_is_function_key( key) ? KEY_CODE_YES : OK;
}

int get_wch(wint_t *wch)
{
    PDC_LOG(("get_wch() - called\n"));

    return wget_wch(stdscr, wch);
}

int mvget_wch(int y, int x, wint_t *wch)
{
    PDC_LOG(("mvget_wch() - called\n"));

    if (move(y, x) == ERR)
        return ERR;

    return wget_wch(stdscr, wch);
}

int mvwget_wch(WINDOW *win, int y, int x, wint_t *wch)
{
    PDC_LOG(("mvwget_wch() - called\n"));

    if (wmove(win, y, x) == ERR)
        return ERR;

    return wget_wch(win, wch);
}

int unget_wch(const wchar_t wch)
{
    return PDC_ungetch(wch);
}
#endif
