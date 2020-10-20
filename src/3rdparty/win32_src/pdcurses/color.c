/* PDCurses */

#include <curspriv.h>

/*man-start**************************************************************

color
-----

### Synopsis

    bool has_colors(void);
    int start_color(void);
    int init_pair(short pair, short fg, short bg);
    int pair_content(short pair, short *fg, short *bg);
    int init_extended_pair(int pair, int fg, int bg);
    int extended_pair_content(int pair, int *fg, int *bg);
    bool can_change_color(void);
    int init_color(short color, short red, short green, short blue);
    int color_content(short color, short *red, short *green, short *blue);
    int init_extended_color(int color, int red, int green, int blue);
    int extended_color_content(int color, int *red, int *green, int *blue);

    int assume_default_colors(int f, int b);
    int use_default_colors(void);

    int PDC_set_line_color(short color);

### Description

   To use these routines, first, call start_color(). Colors are always
   used in pairs, referred to as color-pairs. A color-pair is created by
   init_pair(), and consists of a foreground color and a background
   color. After initialization, COLOR_PAIR(n) can be used like any other
   video attribute.

   has_colors() reports whether the terminal supports color.

   start_color() initializes eight basic colors (black, red, green,
   yellow, blue, magenta, cyan, and white), and two global variables:
   COLORS and COLOR_PAIRS (respectively defining the maximum number of
   colors and color-pairs the terminal is capable of displaying).

   init_pair() changes the definition of a color-pair. It takes three
   arguments: the number of the color-pair to be redefined, and the new
   values of the foreground and background colors. The pair number must
   be between 0 and COLOR_PAIRS - 1, inclusive. The foreground and
   background must be between 0 and COLORS - 1, inclusive. If the color
   pair was previously initialized, the screen is refreshed, and all
   occurrences of that color-pair are changed to the new definition.

   pair_content() is used to determine what the colors of a given color-
   pair consist of.

   init_extended_pair() and extended_pair_content() use ints for the
   color pair index and the color values.  These allow a larger number
   of colors and color pairs to be supported,  eliminating the 32767
   color and color pair limits.

   can_change_color() indicates if the terminal has the capability to
   change the definition of its colors.

   init_color() is used to redefine a color, if possible. Each of the
   components -- red, green, and blue -- is specified in a range from 0
   to 1000, inclusive.

   color_content() reports the current definition of a color in the same
   format as used by init_color().

   init_extended_color() and extended_color_content() use integers for
   the color index.  This enables us to have more than 32767 colors.

   assume_default_colors() and use_default_colors() emulate the ncurses
   extensions of the same names. assume_default_colors(f, b) is
   essentially the same as init_pair(0, f, b) (which isn't allowed); it
   redefines the default colors. use_default_colors() allows the use of
   -1 as a foreground or background color with init_pair(), and calls
   assume_default_colors(-1, -1); -1 represents the foreground or
   background color that the terminal had at startup. If the environment
   variable PDC_ORIGINAL_COLORS is set at the time start_color() is
   called, that's equivalent to calling use_default_colors().

   PDC_set_line_color() is used to set the color, globally, for the
   color of the lines drawn for the attributes: A_UNDERLINE, A_LEFT and
   A_RIGHT. A value of -1 (the default) indicates that the current
   foreground color should be used.

   NOTE: COLOR_PAIR() and PAIR_NUMBER() are implemented as macros.

### Return Value

   All functions return OK on success and ERR on error, except for
   has_colors() and can_change_colors(), which return TRUE or FALSE.

### Portability
                             X/Open  ncurses  NetBSD
    has_colors                  Y       Y       Y
    start_color                 Y       Y       Y
    init_pair                   Y       Y       Y
    pair_content                Y       Y       Y
    can_change_color            Y       Y       Y
    init_color                  Y       Y       Y
    color_content               Y       Y       Y
    assume_default_colors       -       Y       Y
    use_default_colors          -       Y       Y
    PDC_set_line_color          -       -       -

**man-end****************************************************************/

#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <assert.h>

int COLORS = 0;
int COLOR_PAIRS = PDC_COLOR_PAIRS;
static int atrtab_size_alloced;

static bool default_colors = FALSE;
static int first_col = 0;

#define UNSET_COLOR_PAIR      -2

int start_color(void)
{
    PDC_LOG(("start_color() - called\n"));

    assert( SP);
    if (!SP || SP->mono)
        return ERR;

    SP->color_started = TRUE;

    PDC_set_blink(FALSE);   /* Also sets COLORS */

    if (!default_colors && SP->orig_attr && getenv("PDC_ORIGINAL_COLORS"))
        default_colors = TRUE;

    PDC_init_atrtab();
#if defined( CHTYPE_64) && !defined(OS2) && !defined(DOS)
    if( COLORS >= 1024 && (long)INT_MAX > 1024L * 1024L)
        COLOR_PAIRS = 1024 * 1024;
    else if( COLORS >= 16)
    {
        if( (long)COLORS * (long)COLORS < (long)INT_MAX)
            COLOR_PAIRS = COLORS * COLORS;
        else
            COLOR_PAIRS = INT_MAX;
    }
#endif
    return OK;
}

static int _default_foreground_idx = COLOR_WHITE;
static int _default_background_idx = COLOR_BLACK;

void PDC_set_default_colors( const int fg_idx, const int bg_idx)
{
   _default_foreground_idx = fg_idx;
   _default_background_idx = bg_idx;
}

static void _normalize(int *fg, int *bg)
{
    const bool using_defaults = (SP->orig_attr && (default_colors || !SP->color_started));

    if (*fg == -1 || *fg == UNSET_COLOR_PAIR)
        *fg = using_defaults ? SP->orig_fore : _default_foreground_idx;

    if (*bg == -1 || *bg == UNSET_COLOR_PAIR)
        *bg = using_defaults ? SP->orig_back : _default_background_idx;
}

static void _init_pair_core(int pair, int fg, int bg)
{
    PDC_PAIR *p;

    assert( SP->atrtab);
    assert( atrtab_size_alloced);
    if( pair >= atrtab_size_alloced)
    {
        int i, new_size = atrtab_size_alloced * 2;

        while( pair >= new_size)
            new_size += new_size;
        SP->atrtab = (PDC_PAIR *)realloc( SP->atrtab, new_size * sizeof( PDC_PAIR));
        assert( SP->atrtab);
        p = SP->atrtab + atrtab_size_alloced;
        for( i = new_size - atrtab_size_alloced; i; i--, p++)
        {
            p->f = COLOR_GREEN;    /* signal uninitialized pairs by */
            p->b = COLOR_YELLOW;   /* using unusual colors          */
        }
        atrtab_size_alloced = new_size;
    }

    assert( pair >= 0);
    assert( pair < atrtab_size_alloced);
    p = SP->atrtab + pair;

    /* To allow the PDC_PRESERVE_SCREEN option to work, we only reset
       curscr if this call to init_pair() alters a color pair created by
       the user. */

    _normalize(&fg, &bg);

    if (p->f != UNSET_COLOR_PAIR)
    {
        if (p->f != fg || p->b != bg)
            curscr->_clear = TRUE;
    }
    p->f = fg;
    p->b = bg;
}

int init_extended_pair(int pair, int fg, int bg)
{
    PDC_LOG(("init_pair() - called: pair %d fg %d bg %d\n", pair, fg, bg));

    assert( SP);
    if (!SP || !SP->color_started || pair < 1 || pair >= COLOR_PAIRS ||
        fg < first_col || fg >= COLORS || bg < first_col || bg >= COLORS)
        return ERR;

    _init_pair_core(pair, fg, bg);
    curscr->_clear = TRUE;
    return OK;
}

bool has_colors(void)
{
    PDC_LOG(("has_colors() - called\n"));

    assert( SP);
    return SP ? !(SP->mono) : FALSE;
}

int init_extended_color(int color, int red, int green, int blue)
{
    PDC_LOG(("init_color() - called\n"));

    assert( SP);
    if (!SP || color < 0 || color >= COLORS || !PDC_can_change_color() ||
        red < -1 || red > 1000 || green < -1 || green > 1000 ||
        blue < -1 || blue > 1000)
        return ERR;

    SP->dirty = TRUE;
    curscr->_clear = TRUE;
    return PDC_init_color(color, red, green, blue);
}

int extended_color_content(int color, int *red, int *green, int *blue)
{
    PDC_LOG(("color_content() - called\n"));

    if (color < 0 || color >= COLORS || !red || !green || !blue)
        return ERR;

    if (PDC_can_change_color())
        return PDC_color_content(color, red, green, blue);
    else
    {
        /* Simulated values for platforms that don't support palette
           changing */

        int maxval = (color & 8) ? 1000 : 680;

        *red = (color & COLOR_RED) ? maxval : 0;
        *green = (color & COLOR_GREEN) ? maxval : 0;
        *blue = (color & COLOR_BLUE) ? maxval : 0;

        return OK;
    }
}

bool can_change_color(void)
{
    PDC_LOG(("can_change_color() - called\n"));

    return PDC_can_change_color();
}

int extended_pair_content(int pair, int *fg, int *bg)
{
    PDC_LOG(("pair_content() - called\n"));

    if (pair < 0 || pair >= COLOR_PAIRS || !fg || !bg)
        return ERR;

    if( pair >= atrtab_size_alloced)
    {
        *fg = COLOR_RED;      /* signal use of uninitialized pair */
        *bg = COLOR_BLUE;     /* with visible,  but odd,  colors  */
    }
    else
    {
        *fg = SP->atrtab[pair].f;
        *bg = SP->atrtab[pair].b;
    }
    return OK;
}

int assume_default_colors(int f, int b)
{
    PDC_LOG(("assume_default_colors() - called: f %d b %d\n", f, b));

    if (f < -1 || f >= COLORS || b < -1 || b >= COLORS)
        return ERR;

    if (SP->color_started)
    {
        _init_pair_core(0, f, b);
        curscr->_clear = TRUE;
    }

    return OK;
}

int use_default_colors(void)
{
    PDC_LOG(("use_default_colors() - called\n"));

    default_colors = TRUE;
    first_col = -1;

    return assume_default_colors(-1, -1);
}

int PDC_set_line_color(short color)
{
    PDC_LOG(("PDC_set_line_color() - called: %d\n", color));

    assert( SP);
    if (!SP || color < -1 || color >= COLORS)
        return ERR;

    SP->line_color = color;
    curscr->_clear = TRUE;
    return OK;
}

int PDC_init_atrtab(void)
{
    assert( SP);
    if( !SP->atrtab)
    {
       atrtab_size_alloced = 1;
       SP->atrtab = calloc( atrtab_size_alloced, sizeof(PDC_PAIR));
       if( !SP->atrtab)
           return -1;
    }
    _init_pair_core( 0, UNSET_COLOR_PAIR, UNSET_COLOR_PAIR);
    return( 0);
}

int init_pair( short pair, short fg, short bg)
{
    return( init_extended_pair( (int)pair, (int)fg, (int)bg));
}

int pair_content( short pair, short *fg, short *bg)
{
    int i_fg, i_bg;
    const int rval = extended_pair_content( (int)pair, &i_fg, &i_bg);

    if( rval != ERR)
    {
        *fg = (short)i_fg;
        *bg = (short)i_bg;
    }
    return( rval);
}

int init_color( short color, short red, short green, short blue)
{
    return( init_extended_color( (int)color, (int)red, (int)green, (int)blue));
}

int color_content( short color, short *red, short *green, short *blue)
{
    int i_red, i_green, i_blue;
    const int rval = extended_color_content( (int)color, &i_red, &i_green, &i_blue);

    if( rval != ERR)
    {
        *red   = (short)i_red;
        *green = (short)i_green;
        *blue  = (short)i_blue;
    }
    return( rval);
}
