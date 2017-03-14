/* Public Domain Curses */

#include <curspriv.h>

/*man-start**************************************************************

  Name:                                                         addch

  Synopsis:
        int addch(const chtype ch);
        int waddch(WINDOW *win, const chtype ch);
        int mvaddch(int y, int x, const chtype ch);
        int mvwaddch(WINDOW *win, int y, int x, const chtype ch);
        int echochar(const chtype ch);
        int wechochar(WINDOW *win, const chtype ch);

        int addrawch(chtype ch);
        int waddrawch(WINDOW *win, chtype ch);
        int mvaddrawch(int y, int x, chtype ch);
        int mvwaddrawch(WINDOW *win, int y, int x, chtype ch);

        int add_wch(const cchar_t *wch);
        int wadd_wch(WINDOW *win, const cchar_t *wch);
        int mvadd_wch(int y, int x, const cchar_t *wch);
        int mvwadd_wch(WINDOW *win, int y, int x, const cchar_t *wch);
        int echo_wchar(const cchar_t *wch);
        int wecho_wchar(WINDOW *win, const cchar_t *wch);

  Description:
        addch() adds the chtype ch to the default window (stdscr) at the
        current cursor position, and advances the cursor. Note that
        chtypes can convey both text (a single character) and
        attributes, including a color pair. add_wch() is the wide-
        character version of this function, taking a pointer to a
        cchar_t instead of a chtype.

        waddch() is like addch(), but also lets you specify the window.
        (This is in fact the core output routine.) wadd_wch() is the
        wide version.

        mvaddch() moves the cursor to the specified (y, x) position, and
        adds ch to stdscr. mvadd_wch() is the wide version.

        mvwaddch() moves the cursor to the specified position and adds
        ch to the specified window. mvwadd_wch() is the wide version.

        echochar() adds ch to stdscr at the current cursor position and
        calls refresh(). echo_wchar() is the wide version.

        wechochar() adds ch to the specified window and calls
        wrefresh(). wecho_wchar() is the wide version.

        addrawch(), waddrawch(), mvaddrawch() and mvwaddrawch() are
        PDCurses-specific wrappers for addch() etc. that disable the
        translation of control characters.

        The following applies to all these functions:

        If the cursor moves on to the right margin, an automatic newline
        is performed.  If scrollok is enabled, and a character is added
        to the bottom right corner of the window, the scrolling region
        will be scrolled up one line.  If scrolling is not allowed, ERR
        will be returned.

        If ch is a tab, newline, or backspace, the cursor will be moved
        appropriately within the window.  If ch is a newline, the
        clrtoeol routine is called before the cursor is moved to the
        beginning of the next line.  If newline mapping is off, the
        cursor will be moved to the next line, but the x coordinate will
        be unchanged.  If ch is a tab the cursor is moved to the next
        tab position within the window.  If ch is another control
        character, it will be drawn in the ^X notation.  Calling the
        inch() routine after adding a control character returns the
        representation of the control character, not the control
        character.

        Video attributes can be combined with a character by ORing them
        into the parameter. Text, including attributes, can be copied
        from one place to another by using inch() and addch().

        Note that in PDCurses, for now, a cchar_t and a chtype are the
        same. The text field is 16 bits wide, and is treated as Unicode
        (UCS-2) when PDCurses is built with wide-character support
        (define PDC_WIDE). So, in functions that take a chtype, like
        addch(), both the wide and narrow versions will handle Unicode.
        But for portability, you should use the wide functions.

  Return Value:
        All functions return OK on success and ERR on error.

  Portability                                X/Open    BSD    SYS V
        addch                                   Y       Y       Y
        waddch                                  Y       Y       Y
        mvaddch                                 Y       Y       Y
        mvwaddch                                Y       Y       Y
        echochar                                Y       -      3.0
        wechochar                               Y       -      3.0
        addrawch                                -       -       -
        waddrawch                               -       -       -
        mvaddrawch                              -       -       -
        mvwaddrawch                             -       -       -
        add_wch                                 Y
        wadd_wch                                Y
        mvadd_wch                               Y
        mvwadd_wch                              Y
        echo_wchar                              Y
        wecho_wchar                             Y

**man-end****************************************************************/

/* As will be described below,  the method used here for combining
   characters requires going beyond the usual 17*2^16 limit for Unicode.
   That can only happen with 64-bit chtype / cchar_t,  and it's only
   worth doing if we're going past 8-byte characters in the first place.
   So if PDC_WIDE is defined _and_ we're using 64-bit chtypes,  we're
   using the combining character scheme : */

#if defined( PDC_WIDE) && defined( CHTYPE_LONG) && CHTYPE_LONG >= 2
   #define USING_COMBINING_CHARACTER_SCHEME
#endif

#ifdef USING_COMBINING_CHARACTER_SCHEME
#include <stdlib.h>
#include <assert.h>
/*
 * A greatly stripped-down version of Markus Kuhn's excellent
 * wcwidth implementation.  For his latest version and many
 * comments,  see http://www.cl.cam.ac.uk/~mgk25/ucs/wcwidth.c
 * For PDCurses,  only mk_wcwidth is used,  modified to take an
 * int argument instead of wchar_t,  because in MS-land, wchar_t
 * is 16 bits;  getting the full Unicode range requires 21 bits.
 * Also modified format/indenting to conform to PDCurses norms.
 * NOTE that this version is current only to Unicode 5.0!  Some
 * updates are almost certainly needed...
 */

struct interval
{
    int first, last;
};

/* auxiliary function for binary search in interval table */

static int bisearch( const int ucs, const struct interval *table, int max)
{
    int min = 0;
    int mid;

    if (ucs < table[0].first || ucs > table[max].last)
        return 0;
    while (max >= min)
    {
        mid = (min + max) / 2;
        if (ucs > table[mid].last)
            min = mid + 1;
        else if (ucs < table[mid].first)
            max = mid - 1;
        else
           return 1;
    }
  return 0;
}

/* The following two functions define the column width of an ISO 10646
 * character as follows:
 *
 *    - The null character (U+0000) has a column width of 0.
 *
 *    - Other C0/C1 control characters and DEL will lead to a return
 *      value of -1.
 *
 *    - Non-spacing and enclosing combining characters (general
 *      category code Mn or Me in the Unicode database) have a
 *      column width of 0.
 *
 *    - SOFT HYPHEN (U+00AD) has a column width of 1.
 *
 *    - Other format characters (general category code Cf in the Unicode
 *      database) and ZERO WIDTH SPACE (U+200B) have a column width of 0.
 *
 *    - Hangul Jamo medial vowels and final consonants (U+1160-U+11FF)
 *      have a column width of 0.
 *
 *    - Spacing characters in the East Asian Wide (W) or East Asian
 *      Full-width (F) category as defined in Unicode Technical
 *      Report #11 have a column width of 2.
 *
 *    - All remaining characters (including all printable
 *      ISO 8859-1 and WGL4 characters, Unicode control characters,
 *      etc.) have a column width of 1.
 *
 * This implementation assumes that wchar_t characters are encoded
 * in ISO 10646.
 */

static int mk_wcwidth( const int ucs)
{
  /* sorted list of non-overlapping intervals of non-spacing characters */
  /* generated by "uniset +cat=Me +cat=Mn +cat=Cf -00AD +1160-11FF +200B c" */
    static const struct interval combining[] =
    {
         { 0x0300, 0x036F }, { 0x0483, 0x0486 }, { 0x0488, 0x0489 },
         { 0x0591, 0x05BD }, { 0x05BF, 0x05BF }, { 0x05C1, 0x05C2 },
         { 0x05C4, 0x05C5 }, { 0x05C7, 0x05C7 }, { 0x0600, 0x0603 },
         { 0x0610, 0x0615 }, { 0x064B, 0x065E }, { 0x0670, 0x0670 },
         { 0x06D6, 0x06E4 }, { 0x06E7, 0x06E8 }, { 0x06EA, 0x06ED },
         { 0x070F, 0x070F }, { 0x0711, 0x0711 }, { 0x0730, 0x074A },
         { 0x07A6, 0x07B0 }, { 0x07EB, 0x07F3 }, { 0x0901, 0x0902 },
         { 0x093C, 0x093C }, { 0x0941, 0x0948 }, { 0x094D, 0x094D },
         { 0x0951, 0x0954 }, { 0x0962, 0x0963 }, { 0x0981, 0x0981 },
         { 0x09BC, 0x09BC }, { 0x09C1, 0x09C4 }, { 0x09CD, 0x09CD },
         { 0x09E2, 0x09E3 }, { 0x0A01, 0x0A02 }, { 0x0A3C, 0x0A3C },
         { 0x0A41, 0x0A42 }, { 0x0A47, 0x0A48 }, { 0x0A4B, 0x0A4D },
         { 0x0A70, 0x0A71 }, { 0x0A81, 0x0A82 }, { 0x0ABC, 0x0ABC },
         { 0x0AC1, 0x0AC5 }, { 0x0AC7, 0x0AC8 }, { 0x0ACD, 0x0ACD },
         { 0x0AE2, 0x0AE3 }, { 0x0B01, 0x0B01 }, { 0x0B3C, 0x0B3C },
         { 0x0B3F, 0x0B3F }, { 0x0B41, 0x0B43 }, { 0x0B4D, 0x0B4D },
         { 0x0B56, 0x0B56 }, { 0x0B82, 0x0B82 }, { 0x0BC0, 0x0BC0 },
         { 0x0BCD, 0x0BCD }, { 0x0C3E, 0x0C40 }, { 0x0C46, 0x0C48 },
         { 0x0C4A, 0x0C4D }, { 0x0C55, 0x0C56 }, { 0x0CBC, 0x0CBC },
         { 0x0CBF, 0x0CBF }, { 0x0CC6, 0x0CC6 }, { 0x0CCC, 0x0CCD },
         { 0x0CE2, 0x0CE3 }, { 0x0D41, 0x0D43 }, { 0x0D4D, 0x0D4D },
         { 0x0DCA, 0x0DCA }, { 0x0DD2, 0x0DD4 }, { 0x0DD6, 0x0DD6 },
         { 0x0E31, 0x0E31 }, { 0x0E34, 0x0E3A }, { 0x0E47, 0x0E4E },
         { 0x0EB1, 0x0EB1 }, { 0x0EB4, 0x0EB9 }, { 0x0EBB, 0x0EBC },
         { 0x0EC8, 0x0ECD }, { 0x0F18, 0x0F19 }, { 0x0F35, 0x0F35 },
         { 0x0F37, 0x0F37 }, { 0x0F39, 0x0F39 }, { 0x0F71, 0x0F7E },
         { 0x0F80, 0x0F84 }, { 0x0F86, 0x0F87 }, { 0x0F90, 0x0F97 },
         { 0x0F99, 0x0FBC }, { 0x0FC6, 0x0FC6 }, { 0x102D, 0x1030 },
         { 0x1032, 0x1032 }, { 0x1036, 0x1037 }, { 0x1039, 0x1039 },
         { 0x1058, 0x1059 }, { 0x1160, 0x11FF }, { 0x135F, 0x135F },
         { 0x1712, 0x1714 }, { 0x1732, 0x1734 }, { 0x1752, 0x1753 },
         { 0x1772, 0x1773 }, { 0x17B4, 0x17B5 }, { 0x17B7, 0x17BD },
         { 0x17C6, 0x17C6 }, { 0x17C9, 0x17D3 }, { 0x17DD, 0x17DD },
         { 0x180B, 0x180D }, { 0x18A9, 0x18A9 }, { 0x1920, 0x1922 },
         { 0x1927, 0x1928 }, { 0x1932, 0x1932 }, { 0x1939, 0x193B },
         { 0x1A17, 0x1A18 }, { 0x1B00, 0x1B03 }, { 0x1B34, 0x1B34 },
         { 0x1B36, 0x1B3A }, { 0x1B3C, 0x1B3C }, { 0x1B42, 0x1B42 },
         { 0x1B6B, 0x1B73 }, { 0x1DC0, 0x1DCA }, { 0x1DFE, 0x1DFF },
         { 0x200B, 0x200F }, { 0x202A, 0x202E }, { 0x2060, 0x2063 },
         { 0x206A, 0x206F }, { 0x20D0, 0x20EF }, { 0x302A, 0x302F },
         { 0x3099, 0x309A }, { 0xA806, 0xA806 }, { 0xA80B, 0xA80B },
         { 0xA825, 0xA826 }, { 0xFB1E, 0xFB1E }, { 0xFE00, 0xFE0F },
         { 0xFE20, 0xFE23 }, { 0xFEFF, 0xFEFF }, { 0xFFF9, 0xFFFB },
         { 0x10A01, 0x10A03 }, { 0x10A05, 0x10A06 }, { 0x10A0C, 0x10A0F },
         { 0x10A38, 0x10A3A }, { 0x10A3F, 0x10A3F }, { 0x1D167, 0x1D169 },
         { 0x1D173, 0x1D182 }, { 0x1D185, 0x1D18B }, { 0x1D1AA, 0x1D1AD },
         { 0x1D242, 0x1D244 }, { 0xE0001, 0xE0001 }, { 0xE0020, 0xE007F },
         { 0xE0100, 0xE01EF }
    };

  /* test for 8-bit control characters */
    if (ucs == 0)
       return 0;
    if (ucs < 32 || (ucs >= 0x7f && ucs < 0xa0))
       return -1;

    if( ucs < combining[0].first)   /* everything else up to 0x300 is a */
       return( 1);                  /* plain old single-width character */
  /* binary search in table of non-spacing characters */
    if (bisearch(ucs, combining,
          sizeof(combining) / sizeof(struct interval) - 1))
       return 0;

  /* if we arrive here, ucs is not a combining or C0/C1 control character */

    return 1 +
        (ucs >= 0x1100 &&
       (ucs <= 0x115f ||                    /* Hangul Jamo init. consonants */
       ucs == 0x2329 || ucs == 0x232a ||
      (ucs >= 0x2e80 && ucs <= 0xa4cf &&
       ucs != 0x303f) ||                  /* CJK ... Yi */
      (ucs >= 0xac00 && ucs <= 0xd7a3) || /* Hangul Syllables */
      (ucs >= 0xf900 && ucs <= 0xfaff) || /* CJK Compatibility Ideographs */
      (ucs >= 0xfe10 && ucs <= 0xfe19) || /* Vertical forms */
      (ucs >= 0xfe30 && ucs <= 0xfe6f) || /* CJK Compatibility Forms */
      (ucs >= 0xff00 && ucs <= 0xff60) || /* Fullwidth Forms */
      (ucs >= 0xffe0 && ucs <= 0xffe6) ||
      (ucs >= 0x20000 && ucs <= 0x2fffd) ||
      (ucs >= 0x30000 && ucs <= 0x3fffd)));
}

/* The handling of "fullwidth" characters (those consuming two "normal"
columns) and combining characters (characters that can add accents to a
preceding character) in PDCurses is,  of necessity,  complex.

Unicode is defined to have 17 planes of 2^16 characters each,  so that
the maximum Unicode code point is U+10FFFF.  When addch() is given a
fullwidth character,  it handles that character "normally",  and then
stores the non-Unicode character DUMMY_CHAR_NEXT_TO_FULLWIDTH
(U+110000) next to it,  just as a placeholder.  (That part is actually
rather simple.)

PDCurses handles combining characters by creating entirely new "Unicode"
(let's call them "post-Unicode") characters,  at code point U+110001
(COMBINED_CHAR_START) and beyond. The 'combos' table keeps track of
these newly-created characters,  essentially saying:  "This post-Unicode
character consists of the following 'root' character,  plus an
added combining character."  The 'root' character in question may itself
be a post-Unicode character;  this allows us to add more than one
combining character.

For example,  if one calls wchar() with,  in succession, 'r' (U+72),
a circumflex (U+0302),  and an acute accent below (U+317),  the call
with the circumflex would cause 'combo' to be allocated,  with
combo[0].root = 'r' and combo[0].added = 0x302.  Code point U+110001
would correspond to this character ('r' plus circumflex).  The call
with the acute accent below would create code point U+110002,
combo[1].root = 0x110001 and combo[1].added = 0x317.  Thus,  a character
with multiple combining characters simply resolves itself as a series
of "post-Unicode" characters.  When the display function in pdcdisp.c
is asked to show character 0x110001 or 0x110002,  it can use
PDC_expand_combined_characters() to convert that code point to the
actual series of characters.

'ncurses' handles combined characters in a very different manner:  a
'cchar' is defined as an array of five characters,  so that you can
add up to four combining characters in any given cell.  I had to reject
that solution because backward compatibility within PDCurses would be
broken.  Quite aside from that,  this is a simpler solution,  and allows
for any number of combining characters (though four ought to be enough
for anybody).      */

#define MAX_UNICODE                  0x10ffff
#define DUMMY_CHAR_NEXT_TO_FULLWIDTH (MAX_UNICODE + 1)
#define COMBINED_CHAR_START          (MAX_UNICODE + 2)

                                /* "non-standard" 64-bit chtypes     */
static int n_combos = 0, n_combos_allocated = 0;
static struct combined_char
{
    int32_t root, added;
} *combos = NULL;

static int find_combined_char_idx( const cchar_t root, const cchar_t added)
{
    int i;

    for( i = 0; i < n_combos; i++)
        if( (int32_t)root == combos[i].root && (int32_t)added == combos[i].added)
            return( i);
                            /* Didn't find this pair among existing combos; */
                            /* create a new one */
    if( i == n_combos_allocated)
    {
        n_combos_allocated += 30 + n_combos_allocated / 2;
        combos = realloc( combos, n_combos_allocated * sizeof( struct combined_char));
    }
    combos[i].root = (int32_t)root;
    combos[i].added = (int32_t)added;
    n_combos++;
    return( i);
}

int PDC_expand_combined_characters( const cchar_t c, cchar_t *added)
{
    if( !c)    /* flag to free up memory */
    {
        n_combos = n_combos_allocated = 0;
        if( combos)
            free( combos);
        combos = NULL;
        return( 0);
    }
    assert( c >= COMBINED_CHAR_START && c < COMBINED_CHAR_START + n_combos);
    *added = combos[c - COMBINED_CHAR_START].added;
    return( combos[c - COMBINED_CHAR_START].root);
}

#endif      /* #ifdef USING_COMBINING_CHARACTER_SCHEME  */

int waddch( WINDOW *win, const chtype ch)
{
    int x, y;
#ifdef USING_COMBINING_CHARACTER_SCHEME
    int text_width;
#endif
    chtype text, attr;
    bool xlat;

    PDC_LOG(("waddch() - called: win=%p ch=%x (text=%c attr=0x%x)\n",
             win, ch, ch & A_CHARTEXT, ch & A_ATTRIBUTES));

    if (!win)
        return ERR;

    x = win->_curx;
    y = win->_cury;

    if (y > win->_maxy || x > win->_maxx || y < 0 || x < 0)
        return ERR;

    xlat = !SP->raw_out && !(ch & A_ALTCHARSET);
    text = ch & A_CHARTEXT;
    attr = ch & A_ATTRIBUTES;
#ifdef USING_COMBINING_CHARACTER_SCHEME
    text_width = mk_wcwidth( (int)text);

    if( !text_width && text && (x || y))
    {          /* it's a combining char;  combine w/prev char */
        if( x)
            x--;
        else
        {
            y--;
            x = win->_maxx - 1;
        }
        text = COMBINED_CHAR_START
                     + find_combined_char_idx( win->_y[y][x], text);
    }
#endif

    if (xlat && (text < ' ' || text == 0x7f))
    {
        int x2;

        switch ((int)text)
        {
        case '\t':
            for (x2 = ((x / TABSIZE) + 1) * TABSIZE; x < x2; x++)
            {
                if (waddch(win, attr | ' ') == ERR)
                    return ERR;

                /* if tab to next line, exit the loop */

                if (!win->_curx)
                    break;
            }
            return OK;

        case '\n':
            /* if lf -> crlf */

            if (!SP->raw_out)
                x = 0;

            wclrtoeol(win);

            if (++y > win->_bmarg)
            {
                y--;

                if (wscrl(win, 1) == ERR)
                    return ERR;
            }

            break;

        case '\b':
            /* don't back over left margin */

            if (--x < 0)
        case '\r':
                x = 0;

            break;

        case 0x7f:
            if (waddch(win, attr | '^') == ERR)
                return ERR;

            return waddch(win, attr | '?');

        default:
            /* handle control chars */

            if (waddch(win, attr | '^') == ERR)
                return ERR;

            return waddch(win, ch + '@');
        }
    }
    else
    {
        /* If the incoming character doesn't have its own attribute,
           then use the current attributes for the window. If it has
           attributes but not a color component, OR the attributes to
           the current attributes for the window. If it has a color
           component, use the attributes solely from the incoming
           character. */

        if (!(attr & A_COLOR))
            attr |= win->_attrs;

        /* wrs (4/10/93): Apply the same sort of logic for the window
           background, in that it only takes precedence if other color
           attributes are not there and that the background character
           will only print if the printing character is blank. */

        if (!(attr & A_COLOR))
            attr |= win->_bkgd & A_ATTRIBUTES;
        else
            attr |= win->_bkgd & (A_ATTRIBUTES ^ A_COLOR);

        if (text == ' ')
            text = win->_bkgd & A_CHARTEXT;

        /* Add the attribute back into the character. */

        text |= attr;

        /* Only change _firstch/_lastch if the character to be added is
           different from the character/attribute that is already in
           that position in the window. */

        if (win->_y[y][x] != text)
        {
            if (win->_firstch[y] == _NO_CHANGE)
                win->_firstch[y] = win->_lastch[y] = x;
            else
                if (x < win->_firstch[y])
                    win->_firstch[y] = x;
                else
                    if (x > win->_lastch[y])
                        win->_lastch[y] = x;

            win->_y[y][x] = text;
        }

        if (++x >= win->_maxx)
        {
            /* wrap around test */

            x = 0;

            if (++y > win->_bmarg)
            {
                y--;

                if (wscrl(win, 1) == ERR)
                {
                    PDC_sync(win);
                    return ERR;
                }
            }
        }
    }

    win->_curx = x;
    win->_cury = y;

#ifdef USING_COMBINING_CHARACTER_SCHEME
         /* If the character was fullwidth (should occupy two cells),  we */
         /* add a "dummy" character next to it : */
    if( text_width == 2 && x)
        waddch( win, DUMMY_CHAR_NEXT_TO_FULLWIDTH);
#endif

    if (win->_immed)
        wrefresh(win);
    if (win->_sync)
        wsyncup(win);

    return OK;
}

int addch(const chtype ch)
{
    PDC_LOG(("addch() - called: ch=%x\n", ch));

    return waddch(stdscr, ch);
}

int mvaddch(int y, int x, const chtype ch)
{
    PDC_LOG(("mvaddch() - called: y=%d x=%d ch=%x\n", y, x, ch));

    if (move(y,x) == ERR)
        return ERR;

    return waddch(stdscr, ch);
}

int mvwaddch(WINDOW *win, int y, int x, const chtype ch)
{
    PDC_LOG(("mvwaddch() - called: win=%p y=%d x=%d ch=%d\n", win, y, x, ch));

    if (wmove(win, y, x) == ERR)
        return ERR;

    return waddch(win, ch);
}

int echochar(const chtype ch)
{
    PDC_LOG(("echochar() - called: ch=%x\n", ch));

    return wechochar(stdscr, ch);
}

int wechochar(WINDOW *win, const chtype ch)
{
    PDC_LOG(("wechochar() - called: win=%p ch=%x\n", win, ch));

    if (waddch(win, ch) == ERR)
        return ERR;

    return wrefresh(win);
}

int waddrawch(WINDOW *win, chtype ch)
{
    PDC_LOG(("waddrawch() - called: win=%p ch=%x (text=%c attr=0x%x)\n",
             win, ch, ch & A_CHARTEXT, ch & A_ATTRIBUTES));

    if ((ch & A_CHARTEXT) < ' ' || (ch & A_CHARTEXT) == 0x7f)
        ch |= A_ALTCHARSET;

    return waddch(win, ch);
}

int addrawch(chtype ch)
{
    PDC_LOG(("addrawch() - called: ch=%x\n", ch));

    return waddrawch(stdscr, ch);
}

int mvaddrawch(int y, int x, chtype ch)
{
    PDC_LOG(("mvaddrawch() - called: y=%d x=%d ch=%d\n", y, x, ch));

    if (move(y, x) == ERR)
        return ERR;

    return waddrawch(stdscr, ch);
}

int mvwaddrawch(WINDOW *win, int y, int x, chtype ch)
{
    PDC_LOG(("mvwaddrawch() - called: win=%p y=%d x=%d ch=%d\n",
             win, y, x, ch));

    if (wmove(win, y, x) == ERR)
        return ERR;

    return waddrawch(win, ch);
}

#ifdef PDC_WIDE
int wadd_wch(WINDOW *win, const cchar_t *wch)
{
    PDC_LOG(("wadd_wch() - called: win=%p wch=%x\n", win, *wch));

    return wch ? waddch(win, *wch) : ERR;
}

int add_wch(const cchar_t *wch)
{
    PDC_LOG(("add_wch() - called: wch=%x\n", *wch));

    return wadd_wch(stdscr, wch);
}

int mvadd_wch(int y, int x, const cchar_t *wch)
{
    PDC_LOG(("mvaddch() - called: y=%d x=%d wch=%x\n", y, x, *wch));

    if (move(y,x) == ERR)
        return ERR;

    return wadd_wch(stdscr, wch);
}

int mvwadd_wch(WINDOW *win, int y, int x, const cchar_t *wch)
{
    PDC_LOG(("mvwaddch() - called: win=%p y=%d x=%d wch=%d\n",
             win, y, x, *wch));

    if (wmove(win, y, x) == ERR)
        return ERR;

    return wadd_wch(win, wch);
}

int echo_wchar(const cchar_t *wch)
{
    PDC_LOG(("echo_wchar() - called: wch=%x\n", *wch));

    return wecho_wchar(stdscr, wch);
}

int wecho_wchar(WINDOW *win, const cchar_t *wch)
{
    PDC_LOG(("wecho_wchar() - called: win=%p wch=%x\n", win, *wch));

    if (!wch || (wadd_wch(win, wch) == ERR))
        return ERR;

    return wrefresh(win);
}
#endif
