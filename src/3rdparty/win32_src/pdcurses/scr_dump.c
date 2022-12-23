/* PDCurses */

#include <curspriv.h>
#include <assert.h>

/*man-start**************************************************************

scr_dump
--------

### Synopsis

    int putwin(WINDOW *win, FILE *filep);
    WINDOW *getwin(FILE *filep);
    int scr_dump(const char *filename);
    int scr_init(const char *filename);
    int scr_restore(const char *filename);
    int scr_set(const char *filename);

### Description

   getwin() reads window-related data previously stored in a file by
   putwin(). It then creates and initialises a new window using that
   data.

   putwin() writes all data associated with a window into a file, using
   an unspecified format. This information can be retrieved later using
   getwin().

   scr_dump() writes the current contents of the virtual screen to the
   file named by filename in an unspecified format.

   scr_restore() function sets the virtual screen to the contents of the
   file named by filename, which must have been written using
   scr_dump(). The next refresh operation restores the screen to the way
   it looked in the dump file.

   In PDCurses, scr_init() does nothing, and scr_set() is a synonym for
   scr_restore(). Also, scr_dump() and scr_restore() save and load from
   curscr. This differs from some other implementations, where
   scr_init() works with curscr, and scr_restore() works with newscr;
   but the effect should be the same. (PDCurses has no newscr.)

### Return Value

   On successful completion, getwin() returns a pointer to the window it
   created. Otherwise, it returns a null pointer. Other functions return
   OK or ERR.

### Portability
                             X/Open  ncurses  NetBSD
    putwin                      Y       Y       Y
    getwin                      Y       Y       Y
    scr_dump                    Y       Y       -
    scr_init                    Y       Y       -
    scr_restore                 Y       Y       -
    scr_set                     Y       Y       -

**man-end****************************************************************/

#include <stdlib.h>
#include <string.h>

#define DUMPVER 2   /* Should be updated whenever the WINDOW struct is
                       changed */

static void _stuff_chtype_into_eight_bytes( char *buff, const chtype c)
{
   const chtype text = c & A_CHARTEXT;
   const chtype color_pair = PAIR_NUMBER( c);
   const chtype attribs = (c >> PDC_CHARTEXT_BITS) & (((chtype)1 << PDC_ATTRIBUTE_BITS) - 1);
   const uint32_t x = (uint32_t)text | ((uint32_t)attribs << 21);
   const uint32_t y = ((uint32_t)attribs >> 11) | ((uint32_t)color_pair << 1);

   memcpy( buff, &x, 4);
   memcpy( buff + 4, &y, 4);
         /* Should reverse these eight bytes on big-Endian machines */
}

static chtype _get_chtype_from_eight_bytes( const char *buff)
{
   uint32_t x, y;
   chtype c, text, color_pair, attribs;

         /* Should reverse these eight bytes on big-Endian machines */
   memcpy( &x, buff, 4);
   memcpy( &y, buff + 4, 4);
   text = (chtype)x & A_CHARTEXT;
   attribs = (chtype)( ((x >> 21) & 0xfff) | ((y & 1) << 11));
   color_pair = (chtype)(y >> 1) & 0xfffff;
   c = text | (attribs << PDC_CHARTEXT_BITS) | COLOR_PAIR( color_pair);
   return( (chtype)c);
}

/* In PDCursesMod 4.3.3 and earlier,  the on-disk representation of a
window was entirely binary.  A WINDOW struct was written out,  and the
window's chtype data was written out.  Portability of these files was
nearly zero.  Alignment and structure packing differences, differences
in the sizes of ints,  bools,  and pointers,  32-bit vs. 64-bit
chtypes and endianness would usually ensure that a file written by one
program couldn't be read by another.

Adding grief to this is the fact that the window structure changed in
4.3.1.  Files written with 4.3.0 and earlier could not be read in
4.3.1 and later.

The window structure is now written out in ASCII,  which should help
with cross-compiler and cross-OS compatibility.  chtypes are expanded
to the 64-bit form on writing and compacted back upon reading.  You
will get scrambled colors and/or attributes if you make a file with
one program that uses attributes or color pairs beyond the reach of
the program reading the file.  Error checks for this may be added. */

static const char *_format_nine_ints = "%d %d %d %d %d %d %d %d %d\n";
static const char *_format_three_ints = "%d %d %d\n";

int putwin(WINDOW *win, FILE *filep)
{
    char buff[16];
    int y, x;

    PDC_LOG(("putwin() - called\n"));

    assert( filep);
    /* write the marker and the WINDOW struct */

    if( !filep || !fprintf( filep, "%s\n", curses_version( )))
        return( ERR);

    if( !fprintf( filep, _format_nine_ints,
         DUMPVER, (int)sizeof( WINDOW), win->_cury, win->_curx,
         win->_maxy, win->_maxx, win->_begy, win->_begx, win->_flags))
        return( ERR);

    if( !fprintf( filep, _format_nine_ints,
         win->_clear, win->_leaveit, win->_scroll, win->_nodelay,
         win->_immed, win->_sync, win->_use_keypad, win->_tmarg, win->_bmarg))
        return( ERR);

    if( !fprintf( filep, _format_three_ints,
         win->_delayms, win->_parx, win->_pary))
        return( ERR);

    _stuff_chtype_into_eight_bytes( buff, win->_attrs);
    _stuff_chtype_into_eight_bytes( buff + 8, win->_bkgd);
    if( !fwrite(buff, 16, 1, filep))
        return ERR;

    for( y = 0; y < win->_maxy && win->_y[y]; y++)
        for( x = 0; x < win->_maxx; x++)
        {
            _stuff_chtype_into_eight_bytes( buff, win->_y[y][x]);
            if( !fwrite(buff, 8, 1, filep))
                return ERR;
         }
    return OK;
}

void PDC_add_window_to_list( WINDOW *win);

#ifdef _MSC_VER
   #pragma warning( disable: 4701)  /* suppress spurious warnings */
#endif                         /* about 'uninitialised' variables */

WINDOW *getwin(FILE *filep)
{
    WINDOW *win, temp_win;
    char buff[80];
    int nlines, y;
    int _clear, _leaveit, _scroll, _nodelay, _immed, _sync, _use_keypad;
    int version, window_size;
    bool failure = FALSE;

    PDC_LOG(("getwin() - called\n"));

    assert( filep);
    memset( &temp_win, 0, sizeof( WINDOW));

    if (!filep || !fgets( buff, sizeof( buff), filep)
                 || strncmp( buff, curses_version( ), 14))
        failure = TRUE;
    else if( !fgets( buff, sizeof( buff), filep)
         || 9 != sscanf( buff, _format_nine_ints, &version, &window_size,
                  &temp_win._cury, &temp_win._curx, &temp_win._maxy, &temp_win._maxx,
                  &temp_win._begy, &temp_win._begx, &temp_win._flags)
               || version != DUMPVER)
        failure = TRUE;
    else if( !fgets( buff, sizeof( buff), filep)
         || 9 != sscanf( buff, _format_nine_ints, &_clear, &_leaveit,
                     &_scroll, &_nodelay, &_immed, &_sync, &_use_keypad,
                     &temp_win._tmarg, &temp_win._bmarg))
        failure = TRUE;
    else if( !fgets( buff, sizeof( buff), filep)
         || 3 != sscanf( buff, _format_three_ints, &temp_win._delayms,
                        &temp_win._parx, &temp_win._pary))
        failure = TRUE;
    else if( !fread( buff, 16, 1, filep))
        failure = TRUE;

    if( failure)
        return (WINDOW *)NULL;

    win = PDC_makenew( temp_win._maxy, temp_win._maxx, temp_win._begy, temp_win._begx);
    if (!win)
        return (WINDOW *)NULL;
    else
    {
        chtype **saved_y = win->_y;
        int *saved_firstch = win->_firstch;
        int *saved_lastch = win->_lastch;

        memcpy( win, &temp_win, sizeof( WINDOW));
        win->_y = saved_y;
        win->_firstch = saved_firstch;
        win->_lastch  = saved_lastch;
    }
    win->_attrs = _get_chtype_from_eight_bytes( buff);
    win->_bkgd = _get_chtype_from_eight_bytes( buff + 8);
    win->_clear      = (bool)_clear;
    win->_leaveit    = (bool)_leaveit;
    win->_scroll     = (bool)_scroll;
    win->_nodelay    = (bool)_nodelay;
    win->_immed      = (bool)_immed;
    win->_sync       = (bool)_sync;
    win->_use_keypad = (bool)_use_keypad;

    nlines = win->_maxy;

    /* allocate the lines */

    win = PDC_makelines(win);
    if (!win)
        return (WINDOW *)NULL;

    /* read them */

    for( y = 0; y < nlines && !failure; y++)
    {
        const int ncols = win->_maxx;
        int x;

        for( x = 0; x < ncols && !failure; x++)
        {
            if (!fread( buff, 8, 1, filep))
                failure = TRUE;
            else
                win->_y[y][x] = _get_chtype_from_eight_bytes( buff);
        }
    }

    if( failure)
    {
        delwin(win);
        return (WINDOW *)NULL;
    }

    touchwin(win);
    PDC_add_window_to_list( win);

    return win;
}

#ifdef _MSC_VER
   #pragma warning( default: 4701)
#endif

int scr_dump(const char *filename)
{
    FILE *filep;

    PDC_LOG(("scr_dump() - called: filename %s\n", filename));

    if (filename && (filep = fopen(filename, "wb")) != NULL)
    {
        int result = putwin(curscr, filep);
        fclose(filep);
        return result;
    }

    return ERR;
}

int scr_init(const char *filename)
{
    PDC_LOG(("scr_init() - called: filename %s\n", filename));

    INTENTIONALLY_UNUSED_PARAMETER( filename);
    return OK;
}

int scr_restore(const char *filename)
{
    FILE *filep;

    PDC_LOG(("scr_restore() - called: filename %s\n", filename));

    if (filename && (filep = fopen(filename, "rb")) != NULL)
    {
        WINDOW *replacement = getwin(filep);
        fclose(filep);

        if (replacement)
        {
            int result = overwrite(replacement, curscr);
            delwin(replacement);
            return result;
        }
    }

    return ERR;
}

int scr_set(const char *filename)
{
    PDC_LOG(("scr_set() - called: filename %s\n", filename));

    return scr_restore(filename);
}
