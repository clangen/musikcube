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

    int alloc_pair( int fg, int bg);
    int assume_default_colors(int f, int b);
    int find_pair( int fg, int bg);
    int free_pair( int pair);
    int use_default_colors(void);
    void reset_color_pairs(void);

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

   alloc_pair(), find_pair() and free_pair() are also from ncurses.
   free_pair() marks a pair as unused; find_pair() returns an existing
   pair with the specified foreground and background colors, if one
   exists. And alloc_pair() returns such a pair whether or not it was
   previously set, overwriting the oldest initialized pair if there are
   no free pairs.

   reset_color_pairs(),  also from ncurses,  discards all color pair
   information that was set with init_pair().  In practice,  this means
   all color pairs except pair 0 become undefined.

   VT and WinCon define 'default' colors to be those inherited from
   the terminal;  SDLn defines them to be the colors of the background
   image,  if any.  On all other platforms,  and on SDLn if there's no
   background images,  the default background is black;  the default
   foreground is white.

   PDC_set_line_color() is used to set the color, globally, for the
   color of the lines drawn for the attributes: A_UNDERLINE, A_LEFT and
   A_RIGHT. A value of -1 (the default) indicates that the current
   foreground color should be used.

   NOTE: COLOR_PAIR() and PAIR_NUMBER() are implemented as macros.

### Return Value

   Most functions return OK on success and ERR on error. has_colors()
   and can_change_colors() return TRUE or FALSE. alloc_pair() and
   find_pair() return a pair number, or -1 on error.


### Portability
                             X/Open  ncurses  NetBSD
    has_colors                  Y       Y       Y
    start_color                 Y       Y       Y
    init_pair                   Y       Y       Y
    pair_content                Y       Y       Y
    can_change_color            Y       Y       Y
    init_color                  Y       Y       Y
    color_content               Y       Y       Y
    alloc_pair                  -       Y       -
    assume_default_colors       -       Y       Y
    find_pair                   -       Y       -
    free_pair                   -       Y       -
    use_default_colors          -       Y       Y
    PDC_set_line_color          -       -       -

**man-end****************************************************************/

#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <assert.h>

/* Color pair structure */

typedef struct _pdc_pair
{
    int f;                /* foreground color */
    int b;                /* background color */
    int prev, next;       /* doubly-linked list indices */
} PDC_PAIR;

int COLORS = 0;
int COLOR_PAIRS = 1;       /* until start_color() is called */

static void _init_pair_core(int pair, int fg, int bg);

#define UNSET_COLOR_PAIR      -2

static void _unlink_color_pair( const int pair_no)
{
    PDC_PAIR *p = SP->opaque->pairs;
    PDC_PAIR *curr = p + pair_no;

    p[curr->next].prev = curr->prev;
    p[curr->prev].next = curr->next;
}

static void _link_color_pair( const int pair_no, const int head)
{
    PDC_PAIR *p = SP->opaque->pairs;
    PDC_PAIR *curr = p + pair_no;

    curr->next = p[head].next;
    curr->prev = head;
    p[head].next = p[curr->next].prev = pair_no;
}

static int _hash_color_pair( const int fg, const int bg)
{
    int rval = (fg * 31469 + bg * 19583);

    assert( SP->opaque->pair_hash_tbl_size);
    rval ^= rval >> 11;
    rval ^= rval << 7;
    rval &= (SP->opaque->pair_hash_tbl_size - 1);
    return( rval);
}

/* Linear/triangular-number hybrid hash table probing sequence.  See
https://www.projectpluto.com/hashing.htm for details.    */

#define GROUP_SIZE  4
#define ADVANCE_HASH_PROBE( idx, iter) \
              { idx++;        \
                if( iter % GROUP_SIZE == 0) idx += iter - GROUP_SIZE;  \
                idx &= (SP->opaque->pair_hash_tbl_size - 1); }

static void _check_hash_tbl( void)
{
   assert( SP->opaque && SP->opaque->pairs);
   if( SP->opaque->pair_hash_tbl_used * 5 / 4 >= SP->opaque->pair_hash_tbl_size)
      {
      int i, n_pairs;
      PDC_PAIR *p = SP->opaque->pairs;

      for( i = 1, n_pairs = 0; i < SP->opaque->pairs_allocated; i++)
         if( p[i].f != UNSET_COLOR_PAIR)
            n_pairs++;
      SP->opaque->pair_hash_tbl_used = n_pairs;
      SP->opaque->pair_hash_tbl_size = 8;    /* minimum table size */
      while( n_pairs >= SP->opaque->pair_hash_tbl_size * 3 / 4)
         SP->opaque->pair_hash_tbl_size <<= 1;    /* more than 75% of table is full */
      if( SP->opaque->pair_hash_tbl)
         free( SP->opaque->pair_hash_tbl);
      SP->opaque->pair_hash_tbl = (hash_idx_t *)calloc(
                              SP->opaque->pair_hash_tbl_size, sizeof( hash_idx_t));
      for( i = 1; i < SP->opaque->pairs_allocated; i++)
         if( p[i].f != UNSET_COLOR_PAIR)
            {
            int idx = _hash_color_pair( p[i].f, p[i].b), iter;

            for( iter = 0; SP->opaque->pair_hash_tbl[idx]; iter++)
               ADVANCE_HASH_PROBE( idx, iter);
            SP->opaque->pair_hash_tbl[idx] = (hash_idx_t)i;
            }
      }
}

static int _default_foreground_idx = COLOR_WHITE, _default_background_idx = COLOR_BLACK;

int start_color(void)
{
    PDC_LOG(("start_color() - called\n"));

    assert( SP);
    if (!SP || SP->mono)
        return ERR;

    SP->color_started = TRUE;

    PDC_set_blink(FALSE);   /* Also sets COLORS */

    if (!SP->opaque->default_colors && SP->orig_attr && getenv("PDC_ORIGINAL_COLORS"))
        SP->opaque->default_colors = TRUE;

    _init_pair_core( 0, _default_foreground_idx,
                        _default_background_idx);
    if( !SP->_preserve)
        curscr->_clear = TRUE;
#if !defined( CHTYPE_32) && !defined(OS2) && !defined(DOS)
    if( COLORS >= 1024 && (long)INT_MAX > 1024L * 1024L)
        COLOR_PAIRS = 1024 * 1024;
    else if( COLORS >= 16)
    {
        if( (long)COLORS * (long)COLORS < (long)INT_MAX)
            COLOR_PAIRS = COLORS * COLORS;
        else
            COLOR_PAIRS = INT_MAX;
    }
#else
    COLOR_PAIRS = (1 << PDC_COLOR_BITS);
#endif           /* will be 256 (wide-char builds) or 4096 (8-bit chars) */
    return OK;
}

void PDC_set_default_colors( const int fg_idx, const int bg_idx)
{
   _default_foreground_idx = fg_idx;
   _default_background_idx = bg_idx;
}

static void _normalize(int *fg, int *bg)
{
    const bool using_defaults = (SP->orig_attr && (SP->opaque->default_colors || !SP->color_started));

    if (*fg == -1 || *fg == UNSET_COLOR_PAIR)
        *fg = using_defaults ? SP->orig_fore : _default_foreground_idx;

    if (*bg == -1 || *fg == UNSET_COLOR_PAIR)
        *bg = using_defaults ? SP->orig_back : _default_background_idx;
}

/* When a color pair is reset,  all cells of that color should be
redrawn. refresh() and doupdate() don't redraw for color pair changes,
so we have to redraw that text specifically.  The following test is
equivalent to 'if( pair == (int)PAIR_NUMBER( *line))',  but saves a
few cycles by not shifting.  */

#define USES_TARGET_PAIR( ch)  (!(((ch) ^ mask) & A_COLOR))

static void _set_cells_to_refresh_for_pair_change( const int pair)
{
    int x, y;
    const chtype mask = ((chtype)pair << PDC_COLOR_SHIFT);

    assert( SP->lines);
    assert( curscr && curscr->_y);
    if( !curscr->_clear)
        for( y = 0; y < SP->lines; y++)
        {
            chtype *line = curscr->_y[y];

            assert( line);
            for( x = 0; x < SP->cols; x++)
                if( USES_TARGET_PAIR( line[x]))
                {
                    const int start_x = x++;

                    while( x < SP->cols && USES_TARGET_PAIR( line[x]))
                        x++;
                    PDC_transform_line( y, start_x, x - start_x, line + start_x);
                }
        }
}

/* Similarly,  if PDC_set_bold(),  PDC_set_blink(),  or
PDC_set_line_color() is called (and changes the way in which text
with those attributes is drawn),  the corresponding text should be
redrawn.    */

void PDC_set_cells_to_refresh_for_attr_change( const chtype attr)
{
    int x, y;

    assert( SP->lines);
    assert( curscr && curscr->_y);
    if( !curscr->_clear)
        for( y = 0; y < SP->lines; y++)
        {
            chtype *line = curscr->_y[y];

            assert( line);
            for( x = 0; x < SP->cols; x++)
                if( !(*line & attr))
                {
                    const int start_x = x++;

                    while( x < SP->cols && !(*line & attr))
                        x++;
                    PDC_transform_line( y, start_x, x - start_x, line + start_x);
                }
        }
}

static void _init_pair_core(int pair, int fg, int bg)
{
    PDC_PAIR *p;
    bool refresh_pair;

    assert( SP->opaque);
    assert( SP->opaque->pairs_allocated);
    assert( pair < COLOR_PAIRS);
    if( pair >= SP->opaque->pairs_allocated)
    {
        int i, new_size = SP->opaque->pairs_allocated * 2;

        while( pair >= new_size)
            new_size += new_size;
        SP->opaque->pairs = (PDC_PAIR *)realloc( SP->opaque->pairs,
                                      (new_size + 1) * sizeof( PDC_PAIR));
        assert( SP->opaque);
        for( i = SP->opaque->pairs_allocated + 1; i <= new_size; i++)
        {
            p = SP->opaque->pairs + i;
            p->f = UNSET_COLOR_PAIR;
            _link_color_pair( i, SP->opaque->pairs_allocated);
        }
        SP->opaque->pairs_allocated = new_size;
    }

    assert( pair >= 0);
    assert( pair < SP->opaque->pairs_allocated);
    p = SP->opaque->pairs + pair;

    /* To allow the PDC_PRESERVE_SCREEN option to work, we only reset
       curscr if this call to init_pair() alters a color pair created by
       the user. */

    _normalize(&fg, &bg);

    refresh_pair = (p->f != UNSET_COLOR_PAIR && (p->f != fg || p->b != bg));
    _check_hash_tbl( );
    if( pair && p->f != UNSET_COLOR_PAIR)
    {
       int idx = _hash_color_pair( p->f, p->b), iter;

       for( iter = 0; SP->opaque->pair_hash_tbl[idx] != pair; iter++)
       {
           assert( SP->opaque->pair_hash_tbl[idx]);
           ADVANCE_HASH_PROBE( idx, iter);
       }
       SP->opaque->pair_hash_tbl[idx] = -1;    /* mark as freed */
    }
    if( pair)
       _unlink_color_pair( pair);
    p->f = fg;
    p->b = bg;
    if( pair && fg != UNSET_COLOR_PAIR)
    {
       int idx = _hash_color_pair( fg, bg), iter;

       for( iter = 0; SP->opaque->pair_hash_tbl[idx] > 0; iter++)
           ADVANCE_HASH_PROBE( idx, iter);
       if( !SP->opaque->pair_hash_tbl[idx])    /* using a new pair */
           SP->opaque->pair_hash_tbl_used++;
       SP->opaque->pair_hash_tbl[idx] = (hash_idx_t)pair;
    }
    if( pair)
       _link_color_pair( pair, (p->f == UNSET_COLOR_PAIR ? SP->opaque->pairs_allocated : 0));
    if( refresh_pair)
        _set_cells_to_refresh_for_pair_change( pair);
}

int init_extended_pair(int pair, int fg, int bg)
{
    PDC_LOG(("init_pair() - called: pair %d fg %d bg %d\n", pair, fg, bg));

    assert( SP);
    if (!SP || !SP->color_started || pair < 1 || pair >= COLOR_PAIRS
        || fg < SP->opaque->first_col || fg >= COLORS
        || bg < SP->opaque->first_col || bg >= COLORS)
        return ERR;

    _init_pair_core(pair, fg, bg);
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
    PDC_PAIR *p = SP->opaque->pairs + pair;

    PDC_LOG(("pair_content() - called\n"));

    if (pair < 0 || pair >= COLOR_PAIRS || !fg || !bg)
        return ERR;

    if( pair >= SP->opaque->pairs_allocated || (pair && p->f == UNSET_COLOR_PAIR))
    {
        *fg = COLOR_RED;      /* signal use of uninitialized pair */
        *bg = COLOR_BLUE;     /* with visible,  but odd,  colors  */
    }
    else
    {
        *fg = p->f;
        *bg = p->b;
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

    SP->opaque->default_colors = TRUE;
    SP->opaque->first_col = -1;

    return assume_default_colors(-1, -1);
}

int PDC_set_line_color(short color)
{
    PDC_LOG(("PDC_set_line_color() - called: %d\n", color));

    assert( SP);
    if (!SP || color < -1 || color >= COLORS)
        return ERR;

    if( SP->line_color != color)
        PDC_set_cells_to_refresh_for_attr_change(
               A_OVERLINE | A_UNDERLINE | A_LEFTLINE | A_RIGHTLINE | A_STRIKEOUT);
    SP->line_color = color;
    return OK;
}

int PDC_init_atrtab(void)
{
    assert( SP);
    if( !SP->opaque)
    {
       PDC_PAIR *p;

       SP->opaque = (struct _opaque_screen_t *)calloc( 1, sizeof( struct _opaque_screen_t));
       assert( SP->opaque);
       if( !SP->opaque)
           return -1;
       SP->opaque->pairs_allocated = 1;
       SP->opaque->pairs = (PDC_PAIR *)calloc( 2, sizeof(PDC_PAIR));
       assert( SP->opaque->pairs);
       if( !SP->opaque->pairs)
           return -1;
       p = (PDC_PAIR *)SP->opaque->pairs;
       p[0].f = p[1].f = UNSET_COLOR_PAIR;
       p[0].prev = p[0].next = 0;
       p[1].prev = p[1].next = 1;
       SP->opaque->default_colors = FALSE;
       PDC_set_default_colors( _default_foreground_idx, _default_background_idx);
    }
    _init_pair_core( 0,
            (SP->orig_attr ? SP->orig_fore : _default_foreground_idx),
            (SP->orig_attr ? SP->orig_back : _default_background_idx));
    return( 0);
}

void PDC_free_atrtab(void)
{
    assert( SP);
    assert( SP->opaque);
    assert( SP->opaque->pairs);
    if( SP->opaque->pair_hash_tbl)
        free( SP->opaque->pair_hash_tbl);
    SP->opaque->pair_hash_tbl = NULL;
    SP->opaque->pair_hash_tbl_size = SP->opaque->pair_hash_tbl_used = 0;
    if( SP->opaque->pairs)
       free( SP->opaque->pairs);
    free( SP->opaque);
    SP->opaque = NULL;
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

int find_pair( int fg, int bg)
{
    int idx = _hash_color_pair( fg, bg), iter;

    assert( SP);
    assert( SP->opaque);
    assert( SP->opaque->pairs_allocated);
    for( iter = 0; SP->opaque->pair_hash_tbl[idx]; iter++)
    {
        int i;

        if( (i = SP->opaque->pair_hash_tbl[idx]) > 0)
        {
            PDC_PAIR *p = SP->opaque->pairs;

            if( p[i].f == fg && p[i].b == bg)
            {
                _unlink_color_pair( i);   /* unlink it and relink it */
                _link_color_pair( i, 0);  /* to make it the 'head' node */
                return( i);         /* we found the color */
            }
        }
        ADVANCE_HASH_PROBE( idx, iter);
    }
    return( -1);
}

/* alloc_pair() first simply looks to see if the desired pair is
already allocated.  If it has been,  we're done.

   If it hasn't been,  the doubly-linked list of free color
pairs (see 'pairs.txt') will indicate an available node.  If
we've actually run out of free color pairs,  the doubly-linked
list of used color pairs will link to the oldest inserted node.
*/

int alloc_pair( int fg, int bg)
{
    int rval = find_pair( fg, bg);

    if( -1 == rval)        /* pair isn't already allocated.  First,  look */
    {                      /* for an unset color pair. */
        PDC_PAIR *p = SP->opaque->pairs;

        rval = p[SP->opaque->pairs_allocated].prev;
        assert( rval);
        if( COLOR_PAIRS == rval)       /* all color pairs are in use; */
            rval = p[0].prev;          /* 'repurpose' the oldest pair */
        if( ERR == init_extended_pair( rval, fg, bg))
            rval = -1;
        assert( rval != -1);
   }
   return( rval);
}

int free_pair( int pair)
{
    PDC_PAIR *p;

    assert( SP && SP->opaque && SP->opaque->pairs);
    assert( pair >= 1 && pair < SP->opaque->pairs_allocated);
    p = SP->opaque->pairs + pair;
    assert( p->f != UNSET_COLOR_PAIR);
    if (!SP || !SP->color_started || pair < 1 || pair >= SP->opaque->pairs_allocated
               || p->f == UNSET_COLOR_PAIR)
        return ERR;

    _init_pair_core(pair, UNSET_COLOR_PAIR, 0);
    return OK;
}

void reset_color_pairs( void)
{
    assert( SP && SP->opaque && SP->opaque->pairs);
    PDC_free_atrtab( );
    PDC_init_atrtab( );
    curscr->_clear = TRUE;
}

#ifdef PDC_COLOR_PAIR_DEBUGGING_FUNCTIONS

/* The following is solely for testing the color pair table,  and
specifically its two doubly-linked lists (one of 'used' pairs, one of
'free' pairs).  The elements in both lists are counted. The total should
equal the number of allocated pairs.  All pairs in the first linked list
are checked to make sure they're really used;  all in the second to make
sure they're really free.  We also check that the links are consistent.
The return value is 0 if the table checks out,  -1 if it does not.
'results' contains the number of used pairs,  the number of free pairs,
and the number of allocated pairs (which should be the sum of the first
two numbers.)  It also returns some data on the hash table size and
usage.  See 'pairs.txt' for more details.   */

int PDC_check_color_pair_table( int *results)
{
    int idx, n_used = 1, n_free = 1;
    PDC_PAIR *p;

    assert( SP && SP->opaque && SP->opaque->pairs);
    p = (PDC_PAIR *)SP->opaque->pairs;
    idx = 0;
    while( n_used < SP->opaque->pairs_allocated + 10 && p[idx].next)
    {                /* loop through all _used_ color pairs */
        const int next = p[idx].next;

        assert( p[idx].f != UNSET_COLOR_PAIR);
        assert( next >= 0 && next < SP->opaque->pairs_allocated);
        assert( p[next].prev == idx);
        idx = p[idx].next;
        n_used++;
    }

    idx = SP->opaque->pairs_allocated;
    while( n_free < SP->opaque->pairs_allocated + 10 && p[idx].next != SP->opaque->pairs_allocated)
    {                /* loop through all _free_ color pairs */
        const int next = p[idx].next;

        assert( p[idx].f == UNSET_COLOR_PAIR);
        assert( next > 0 && next <= SP->opaque->pairs_allocated);
        assert( p[next].prev == idx);
        idx = p[idx].next;
        n_free++;
    }

    if( results)
    {
        results[0] = n_used;
        results[1] = n_free;
        results[2] = SP->opaque->pairs_allocated + 1;      /* include the 'dummy' pair */
        results[3] = SP->opaque->pair_hash_tbl_size;
        results[4] = SP->opaque->pair_hash_tbl_used;
    }
    return( (n_used + n_free == SP->opaque->pairs_allocated + 1) ? 0 : -1);
}
#endif   /*  #ifdef PDC_COLOR_PAIR_DEBUGGING_FUNCTIONS */
