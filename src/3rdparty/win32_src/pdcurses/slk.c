/* Public Domain Curses */

#include <curspriv.h>

/*man-start**************************************************************

slk
---

### Synopsis

    int slk_init(int fmt);
    int slk_set(int labnum, const char *label, int justify);
    int slk_refresh(void);
    int slk_noutrefresh(void);
    char *slk_label(int labnum);
    int slk_clear(void);
    int slk_restore(void);
    int slk_touch(void);
    int slk_attron(const chtype attrs);
    int slk_attr_on(const attr_t attrs, void *opts);
    int slk_attrset(const chtype attrs);
    int slk_attr_set(const attr_t attrs, short color_pair, void *opts);
    int slk_attroff(const chtype attrs);
    int slk_attr_off(const attr_t attrs, void *opts);
    int slk_color(short color_pair);

    int slk_wset(int labnum, const wchar_t *label, int justify);

    int PDC_mouse_in_slk(int y, int x);
    void PDC_slk_free(void);
    void PDC_slk_initialize(void);

    wchar_t *slk_wlabel(int labnum)

### Description

   These functions manipulate a window that contain Soft Label Keys
   (SLK). To use the SLK functions, a call to slk_init() must be
   made BEFORE initscr() or newterm(). slk_init() removes 1 or 2
   lines from the useable screen, depending on the format selected.

   The line(s) removed from the screen are used as a separate
   window, in which SLKs are displayed.

   slk_init() requires a single parameter which describes the
   format of the SLKs as follows:

   0       3-2-3 format
   1       4-4 format
   2       4-4-4 format (ncurses extension)
   3       4-4-4 format with index line (ncurses extension)
   2 lines used
   55      5-5 format (pdcurses format)

   In PDCurses,  one can alternatively set fmt as a series of hex
   digits specifying the format.  For example,  0x414 would result
   in 4-1-4 format; 0x21b3 would result in 2-1-11-3 format;  and
   so on.  Also,  negating fmt results in the index line being added.

   Also,  in PDCurses,  one can call slk_init() at any time
   _after_ initscr(),  to reset the label format.  If you do this,
   you'll need to reset the label text and call slk_refresh().  And
   you can't toggle the index line.  (Doing so would add/remove a line
   from the useable screen,  which would be hard to handle correctly.)

   slk_refresh(), slk_noutrefresh() and slk_touch() are analogous
   to refresh(), noutrefresh() and touch().

### Return Value

   All functions return OK on success and ERR on error.

### Portability
                             X/Open    BSD    SYS V
    slk_init                    Y       -       Y
    slk_set                     Y       -       Y
    slk_refresh                 Y       -       Y
    slk_noutrefresh             Y       -       Y
    slk_label                   Y       -       Y
    slk_clear                   Y       -       Y
    slk_restore                 Y       -       Y
    slk_touch                   Y       -       Y
    slk_attron                  Y       -       Y
    slk_attrset                 Y       -       Y
    slk_attroff                 Y       -       Y
    slk_attr_on                 Y
    slk_attr_set                Y
    slk_attr_off                Y
    slk_wset                    Y
    PDC_mouse_in_slk            -       -       -
    PDC_slk_free                -       -       -
    PDC_slk_initialize          -       -       -
    slk_wlabel                  -       -       -

**man-end****************************************************************/

#include <stdlib.h>

static int label_length = 0;
static int n_labels = 0;
static int label_fmt = 0;
static int label_line = 0;
static bool hidden = FALSE;

#define MAX_LABEL_LENGTH 32

static struct SLK {
    chtype label[MAX_LABEL_LENGTH];
    int len;
    int format;
    int start_col;
} *slk = (struct SLK *)NULL;

/* See comments above on this function.   */

int slk_init(int fmt)
{
    int i;

    PDC_LOG(("slk_init() - called\n"));

    switch (fmt)
    {
    case 0:  /* 3 - 2 - 3 */
        label_fmt = 0x323;
        break;

    case 1:   /* 4 - 4 */
        label_fmt = 0x44;
        break;

    case 2:   /* 4 4 4 */
        label_fmt = 0x444;
        break;

    case 3:   /* 4 4 4  with index */
        label_fmt = -0x444;
        break;

    case 55:  /* 5 - 5 */
        label_fmt = 0x55;
        break;

    default:
        label_fmt = fmt;
        break;
    }

    traceon( );
    n_labels = 0;
    for( i = abs( label_fmt); i; i /= 16)
       n_labels += i % 16;

    PDC_LOG(("slk_init: fmt %d, %d labels, %p\n",
               fmt, n_labels, slk));
    if( slk)
        free( slk);
    slk = calloc(n_labels, sizeof(struct SLK));
    PDC_LOG(( "New slk: %p; SP = %p\n", slk, SP));
    traceoff( );

    if (!slk)
        n_labels = 0;
    if( SP)
        {
        if( SP->slk_winptr)
            wclear( SP->slk_winptr);
        PDC_slk_initialize( );
        }

    return slk ? OK : ERR;
}

/* draw a single button */

static void _drawone(int num)
{
    int i, col, slen;

    if (hidden)
        return;

    slen = slk[num].len;

    switch (slk[num].format)
    {
    case 0:  /* LEFT */
        col = 0;
        break;

    case 1:  /* CENTER */
        col = (label_length - slen) / 2;

        if (col + slen > label_length)
            --col;
        break;

    default:  /* RIGHT */
        col = label_length - slen;
    }

    if( col < 0)  /* Ensure start of label is visible */
        col = 0;
    wmove(SP->slk_winptr, label_line, slk[num].start_col);

    for (i = 0; i < label_length; ++i)
        waddch(SP->slk_winptr, (i >= col && i < (col + slen)) ?
               slk[num].label[i - col] : ' ');
}

/* redraw each button */

static void _redraw(void)
{
    int i;

    if( !hidden)
    {
        for (i = 0; i < n_labels; ++i)
            _drawone(i);
        if (label_fmt < 0)
        {
            const chtype save_attr = SP->slk_winptr->_attrs;

            wattrset(SP->slk_winptr, A_NORMAL);
            wmove(SP->slk_winptr, 0, 0);
            whline(SP->slk_winptr, 0, COLS);

            for (i = 0; i < n_labels; i++)
                mvwprintw(SP->slk_winptr, 0, slk[i].start_col, "F%d", i + 1);

            SP->slk_winptr->_attrs = save_attr;
        }
    }
}

/* slk_set() Used to set a slk label to a string.

   labnum  = 1 - 8 (or 10) (number of the label)
   label   = string (8 or 7 bytes total), or NULL
   justify = 0 : left, 1 : center, 2 : right  */

int slk_set(int labnum, const char *label, int justify)
{
#ifdef PDC_WIDE
    wchar_t wlabel[MAX_LABEL_LENGTH];

    PDC_mbstowcs(wlabel, label, MAX_LABEL_LENGTH - 1);
    return slk_wset(labnum, wlabel, justify);
#else
    PDC_LOG(("slk_set() - called\n"));

    if (labnum < 1 || labnum > n_labels || justify < 0 || justify > 2)
        return ERR;

    labnum--;

    if (!label || !(*label))
    {
        /* Clear the label */

        *slk[labnum].label = 0;
        slk[labnum].format = 0;
        slk[labnum].len = 0;
    }
    else
    {
        int i;

        /* Skip leading spaces */

        while( *label == ' ')
            label++;

        /* Copy it */

        for (i = 0; label[i] && i < MAX_LABEL_LENGTH - 1; i++)
            slk[labnum].label[i] = label[i];

        /* Drop trailing spaces */

        while( i && label[i - 1] == ' ')
            i--;

        slk[labnum].label[i] = 0;
        slk[labnum].format = justify;
        slk[labnum].len = i;
    }

    _drawone(labnum);

    return OK;
#endif
}

int slk_refresh(void)
{
    PDC_LOG(("slk_refresh() - called\n"));

    return (slk_noutrefresh() == ERR) ? ERR : doupdate();
}

int slk_noutrefresh(void)
{
    PDC_LOG(("slk_noutrefresh() - called\n"));

    return wnoutrefresh(SP->slk_winptr);
}

char *slk_label(int labnum)
{
    static char temp[MAX_LABEL_LENGTH + 1];
#ifdef PDC_WIDE
    wchar_t *wtemp = slk_wlabel(labnum);

    PDC_wcstombs(temp, wtemp, MAX_LABEL_LENGTH);
#else
    chtype *p;
    int i;

    PDC_LOG(("slk_label() - called\n"));

    if (labnum < 1 || labnum > n_labels)
        return (char *)0;

    for (i = 0, p = slk[labnum - 1].label; *p; i++)
        temp[i] = (char)*p++;    /* BJG */

    temp[i] = '\0';
#endif
    return temp;
}

int slk_clear(void)
{
    PDC_LOG(("slk_clear() - called\n"));

    hidden = TRUE;
    werase(SP->slk_winptr);
    return wrefresh(SP->slk_winptr);
}

int slk_restore(void)
{
    PDC_LOG(("slk_restore() - called\n"));

    hidden = FALSE;
    _redraw();
    return wrefresh(SP->slk_winptr);
}

int slk_touch(void)
{
    PDC_LOG(("slk_touch() - called\n"));

    return touchwin(SP->slk_winptr);
}

int slk_attron(const chtype attrs)
{
    int rc;

    PDC_LOG(("slk_attron() - called\n"));

    rc = wattron(SP->slk_winptr, attrs);
    _redraw();

    return rc;
}

int slk_attr_on(const attr_t attrs, void *opts)
{
    PDC_LOG(("slk_attr_on() - called\n"));

    return slk_attron(attrs);
}

int slk_attroff(const chtype attrs)
{
    int rc;

    PDC_LOG(("slk_attroff() - called\n"));

    rc = wattroff(SP->slk_winptr, attrs);
    _redraw();

    return rc;
}

int slk_attr_off(const attr_t attrs, void *opts)
{
    PDC_LOG(("slk_attr_off() - called\n"));

    return slk_attroff(attrs);
}

int slk_attrset(const chtype attrs)
{
    int rc;

    PDC_LOG(("slk_attrset() - called\n"));

    rc = wattrset(SP->slk_winptr, attrs);
    _redraw();

    return rc;
}

int slk_color(short color_pair)
{
    int rc;

    PDC_LOG(("slk_color() - called\n"));

    rc = wcolor_set(SP->slk_winptr, color_pair, NULL);
    _redraw();

    return rc;
}

int slk_attr_set(const attr_t attrs, short color_pair, void *opts)
{
    PDC_LOG(("slk_attr_set() - called\n"));

    return slk_attrset(attrs | COLOR_PAIR(color_pair));
}

static void _slk_calc(void)
{
    int i, j, idx, remaining_space;
    int n_groups = 0, group_size[10];

    label_length = COLS / n_labels;
    if (label_length > MAX_LABEL_LENGTH)
        label_length = MAX_LABEL_LENGTH;
    remaining_space = COLS - label_length * n_labels + 1;
    for( i = abs( label_fmt); i; i /= 16)
        group_size[n_groups++] = i % 16;
               /* We really want at least two spaces between groups: */
    while( label_length > 1 && remaining_space < n_groups - 1)
    {
        label_length--;
        remaining_space += n_labels;
    }

    for( i = idx = 0; i < n_groups; i++)
        for( j = 0; j < group_size[i]; j++, idx++)
            slk[idx].start_col = label_length * idx
                     + (i ? (i * remaining_space) / (n_groups - 1) : 0);

    if( label_length)
       --label_length;

    /* make sure labels are all in window */

    _redraw();
}

void PDC_slk_initialize(void)
{
    if (slk)
    {
        if( label_fmt < 0)
        {
            SP->slklines = 2;
            label_line = 1;
        }
        else
            SP->slklines = 1;

        if (!SP->slk_winptr)
        {
            if ( !(SP->slk_winptr = newwin(SP->slklines, COLS,
                                           LINES - SP->slklines, 0)) )
                return;

            wattrset(SP->slk_winptr, A_REVERSE);
        }

        _slk_calc();

        touchwin(SP->slk_winptr);
    }
}

void PDC_slk_free(void)
{
    if (slk)
    {
        if (SP->slk_winptr)
        {
            delwin(SP->slk_winptr);
            SP->slk_winptr = (WINDOW *)NULL;
        }

        free(slk);
        slk = (struct SLK *)NULL;

        label_length = 0;
        n_labels = 0;
        label_fmt = 0;
        label_line = 0;
        hidden = FALSE;
    }
}

int PDC_mouse_in_slk(int y, int x)
{
    int i;

    PDC_LOG(("PDC_mouse_in_slk() - called: y->%d x->%d\n", y, x));

    /* If the line on which the mouse was clicked is NOT the last line
       of the screen, or the SLKs are hidden,  we are not interested in it. */

    if (!slk || hidden || !SP->slk_winptr
                        || (y != SP->slk_winptr->_begy + label_line))
        return 0;

    for (i = 0; i < n_labels; i++)
        if (x >= slk[i].start_col && x < (slk[i].start_col + label_length))
            return i + 1;

    return 0;
}

#ifdef PDC_WIDE
int slk_wset(int labnum, const wchar_t *label, int justify)
{
    PDC_LOG(("slk_wset() - called\n"));

    if (labnum < 1 || labnum > n_labels || justify < 0 || justify > 2)
        return ERR;

    labnum--;

    if (!label || !(*label))
    {
        /* Clear the label */

        *slk[labnum].label = 0;
        slk[labnum].format = 0;
        slk[labnum].len = 0;
    }
    else
    {
        int i;

        /* Skip leading spaces */

        while( *label == L' ')
            label++;

        /* Copy it */

        for (i = 0; label[i] && i < MAX_LABEL_LENGTH - 1; i++)
            slk[labnum].label[i] = label[i];

        /* Drop trailing spaces */

        while( i && label[i - 1] == L' ')
            i--;

        slk[labnum].label[i] = 0;
        slk[labnum].format = justify;
        slk[labnum].len = i;
    }

    _drawone(labnum);

    return OK;
}

wchar_t *slk_wlabel(int labnum)
{
    static wchar_t temp[MAX_LABEL_LENGTH + 1];
    chtype *p;
    int i;

    PDC_LOG(("slk_wlabel() - called\n"));

    if (labnum < 1 || labnum > n_labels)
        return (wchar_t *)0;

    for (i = 0, p = slk[labnum - 1].label; *p; i++)
        temp[i] = (wchar_t)*p++;

    temp[i] = '\0';

    return temp;
}
#endif
