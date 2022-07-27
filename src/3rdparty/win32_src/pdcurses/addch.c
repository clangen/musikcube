/* PDCurses */

/* On Linux,  and probably some other platforms,  we can just
use the built-in wcwidth() function.  */
#ifdef __linux
   #define HAVE_WCWIDTH
   #define _XOPEN_SOURCE
   #include <wchar.h>
#endif

#include <curspriv.h>
#include <assert.h>

/*man-start**************************************************************

addch
-----

### Synopsis

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

### Description

   addch() adds the chtype ch to the default window (stdscr) at the
   current cursor position, and advances the cursor. Note that chtypes
   can convey both text (a single character) and attributes, including a
   color pair. add_wch() is the wide-character version of this function,
   taking a pointer to a cchar_t instead of a chtype.

   waddch() is like addch(), but also lets you specify the window. (This
   is in fact the core output routine.) wadd_wch() is the wide version.

   mvaddch() moves the cursor to the specified (y, x) position, and adds
   ch to stdscr. mvadd_wch() is the wide version.

   mvwaddch() moves the cursor to the specified position and adds ch to
   the specified window. mvwadd_wch() is the wide version.

   echochar() adds ch to stdscr at the current cursor position and calls
   refresh(). echo_wchar() is the wide version.

   wechochar() adds ch to the specified window and calls wrefresh().
   wecho_wchar() is the wide version.

   addrawch(), waddrawch(), mvaddrawch() and mvwaddrawch() are PDCurses-
   specific wrappers for addch() etc. that disable the translation of
   control characters.

   The following applies to all these functions:

   If the cursor moves on to the right margin, an automatic newline is
   performed. If scrollok is enabled, and a character is added to the
   bottom right corner of the window, the scrolling region will be
   scrolled up one line. If scrolling is not allowed, ERR will be
   returned.

   If ch is a tab, newline, or backspace, the cursor will be moved
   appropriately within the window. If ch is a newline, the clrtoeol
   routine is called before the cursor is moved to the beginning of the
   next line. If newline mapping is off, the cursor will be moved to
   the next line, but the x coordinate will be unchanged. If ch is a
   tab the cursor is moved to the next tab position within the window.
   If ch is another control character, it will be drawn in the ^X
   notation. Calling the inch() routine after adding a control
   character returns the representation of the control character, not
   the control character.

   Video attributes can be combined with a character by ORing them into
   the parameter. Text, including attributes, can be copied from one
   place to another by using inch() and addch().

   Note that in PDCurses, for now, a cchar_t and a chtype are the same.
   The text field is 16 bits wide, and is treated as Unicode (UCS-2)
   when PDCurses is built with wide-character support (define PDC_WIDE).
   So, in functions that take a chtype, like addch(), both the wide and
   narrow versions will handle Unicode. But for portability, you should
   use the wide functions.

### Return Value

   All functions return OK on success and ERR on error.

### Portability
                             X/Open  ncurses  NetBSD
    addch                       Y       Y       Y
    waddch                      Y       Y       Y
    mvaddch                     Y       Y       Y
    mvwaddch                    Y       Y       Y
    echochar                    Y       Y       Y
    wechochar                   Y       Y       Y
    add_wch                     Y       Y       Y
    wadd_wch                    Y       Y       Y
    mvadd_wch                   Y       Y       Y
    mvwadd_wch                  Y       Y       Y
    echo_wchar                  Y       Y       Y
    wecho_wchar                 Y       Y       Y
    addrawch                    -       -       -
    waddrawch                   -       -       -
    mvaddrawch                  -       -       -
    mvwaddrawch                 -       -       -

**man-end****************************************************************/

/* As will be described below,  the method used here for combining
   characters requires going beyond the usual 17*2^16 limit for Unicode.
   That can only happen with 64-bit chtype / cchar_t,  and it's only
   worth doing if we're going past 8-byte characters in the first place.
   So if PDC_WIDE is defined _and_ we're using 64-bit chtypes,  we're
   using the combining character scheme.  See curses.h. */

#ifdef PDC_WIDE
#include <stdlib.h>

#ifdef HAVE_WCWIDTH

PDCEX int PDC_wcwidth( const int32_t ucs)
{
   return( wcwidth( (wchar_t)ucs));
}

#else
/*  A greatly modified version of Markus Kuhn's excellent
wcwidth implementation.  For his latest version and many
comments,  see http://www.cl.cam.ac.uk/~mgk25/ucs/wcwidth.c
For PDCurses,  only PDC_wcwidth is used,  modified to take an
int argument instead of wchar_t,  because in MS-land, wchar_t
is 16 bits;  getting the full Unicode range requires 21 bits.
Also modified format/indenting to conform to PDCurses norms,
and (June 2022) updated from Unicode 5.0 to 14.0.0.  See
uni_tbl.c in the Bill-Gray/junk repository.

Following function modified from one in the README.md at
https://github.com/depp/uniset */

static bool _uniset_test( uint16_t const set[][2], uint32_t c)
{
    const unsigned int p = c >> 16;
    unsigned int l = set[p][0] + 17, r = set[p][1] + 17;

    assert( p <= 16);      /* i.e.,  0 <= c <= 0x10ffff => c is in Unicode's range */
    c &= 0xffff;
    while (l < r)
    {
        const unsigned int m = (l + r) / 2;

        if( c < set[m][0])
            r = m;
        else if( c > set[m][1])
            l = m + 1;
        else
            return TRUE;
    }
    return( FALSE);
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

PDCEX int PDC_wcwidth( const int32_t ucs)
{
      /* see 'uni_tbl.c' in the Bill-Gray/junk repo */
const uint16_t tbl_for_zero_width_chars[][2] = {
     { /* plane 0 */ 0, 202 },
     { /* plane 1 */ 202, 312 },
     { /* plane 2 */ 0, 0 },
     { /* plane 3 */ 0, 0 },
     { /* plane 4 */ 0, 0 },
     { /* plane 5 */ 0, 0 },
     { /* plane 6 */ 0, 0 },
     { /* plane 7 */ 0, 0 },
     { /* plane 8 */ 0, 0 },
     { /* plane 9 */ 0, 0 },
     { /* plane 10 */ 0, 0 },
     { /* plane 11 */ 0, 0 },
     { /* plane 12 */ 0, 0 },
     { /* plane 13 */ 0, 0 },
     { /* plane 14 */ 312, 313 },
     { /* plane 15 */ 0, 0 },
     { /* plane 16 */ 0, 0 },
     { 0x00AD, 0x036F }, { 0x0483, 0x0489 }, { 0x0591, 0x05BD },
     { 0x05BF, 0x05BF }, { 0x05C1, 0x05C2 }, { 0x05C4, 0x05C5 },
     { 0x05C7, 0x05C7 }, { 0x0600, 0x0605 }, { 0x0610, 0x061A },
     { 0x061C, 0x061C }, { 0x064B, 0x065F }, { 0x0670, 0x0670 },
     { 0x06D6, 0x06DD }, { 0x06DF, 0x06E4 }, { 0x06E7, 0x06E8 },
     { 0x06EA, 0x06ED }, { 0x070F, 0x070F }, { 0x0711, 0x0711 },
     { 0x0730, 0x074A }, { 0x07A6, 0x07B0 }, { 0x07EB, 0x07F3 },
     { 0x07FD, 0x07FD }, { 0x0816, 0x0819 }, { 0x081B, 0x0823 },
     { 0x0825, 0x0827 }, { 0x0829, 0x082D }, { 0x0859, 0x085B },
     { 0x0890, 0x089F }, { 0x08CA, 0x0902 }, { 0x093A, 0x093A },
     { 0x093C, 0x093C }, { 0x0941, 0x0948 }, { 0x094D, 0x094D },
     { 0x0951, 0x0957 }, { 0x0962, 0x0963 }, { 0x0981, 0x0981 },
     { 0x09BC, 0x09BC }, { 0x09C1, 0x09C4 }, { 0x09CD, 0x09CD },
     { 0x09E2, 0x09E3 }, { 0x09FE, 0x0A02 }, { 0x0A3C, 0x0A3C },
     { 0x0A41, 0x0A51 }, { 0x0A70, 0x0A71 }, { 0x0A75, 0x0A75 },
     { 0x0A81, 0x0A82 }, { 0x0ABC, 0x0ABC }, { 0x0AC1, 0x0AC8 },
     { 0x0ACD, 0x0ACD }, { 0x0AE2, 0x0AE3 }, { 0x0AFA, 0x0B01 },
     { 0x0B3C, 0x0B3C }, { 0x0B3F, 0x0B3F }, { 0x0B41, 0x0B44 },
     { 0x0B4D, 0x0B56 }, { 0x0B62, 0x0B63 }, { 0x0B82, 0x0B82 },
     { 0x0BC0, 0x0BC0 }, { 0x0BCD, 0x0BCD }, { 0x0C00, 0x0C00 },
     { 0x0C04, 0x0C04 }, { 0x0C3C, 0x0C3C }, { 0x0C3E, 0x0C40 },
     { 0x0C46, 0x0C56 }, { 0x0C62, 0x0C63 }, { 0x0C81, 0x0C81 },
     { 0x0CBC, 0x0CBC }, { 0x0CBF, 0x0CBF }, { 0x0CC6, 0x0CC6 },
     { 0x0CCC, 0x0CCD }, { 0x0CE2, 0x0CE3 }, { 0x0D00, 0x0D01 },
     { 0x0D3B, 0x0D3C }, { 0x0D41, 0x0D44 }, { 0x0D4D, 0x0D4D },
     { 0x0D62, 0x0D63 }, { 0x0D81, 0x0D81 }, { 0x0DCA, 0x0DCA },
     { 0x0DD2, 0x0DD6 }, { 0x0E31, 0x0E31 }, { 0x0E34, 0x0E3A },
     { 0x0E47, 0x0E4E }, { 0x0EB1, 0x0EB1 }, { 0x0EB4, 0x0EBC },
     { 0x0EC8, 0x0ECD }, { 0x0F18, 0x0F19 }, { 0x0F35, 0x0F35 },
     { 0x0F37, 0x0F37 }, { 0x0F39, 0x0F39 }, { 0x0F71, 0x0F7E },
     { 0x0F80, 0x0F84 }, { 0x0F86, 0x0F87 }, { 0x0F8D, 0x0FBC },
     { 0x0FC6, 0x0FC6 }, { 0x102D, 0x1030 }, { 0x1032, 0x1037 },
     { 0x1039, 0x103A }, { 0x103D, 0x103E }, { 0x1058, 0x1059 },
     { 0x105E, 0x1060 }, { 0x1071, 0x1074 }, { 0x1082, 0x1082 },
     { 0x1085, 0x1086 }, { 0x108D, 0x108D }, { 0x109D, 0x109D },
     { 0x1160, 0x11FF }, { 0x135D, 0x135F }, { 0x1712, 0x1714 },
     { 0x1732, 0x1733 }, { 0x1752, 0x1753 }, { 0x1772, 0x1773 },
     { 0x17B4, 0x17B5 }, { 0x17B7, 0x17BD }, { 0x17C6, 0x17C6 },
     { 0x17C9, 0x17D3 }, { 0x17DD, 0x17DD }, { 0x180B, 0x180F },
     { 0x1885, 0x1886 }, { 0x18A9, 0x18A9 }, { 0x1920, 0x1922 },
     { 0x1927, 0x1928 }, { 0x1932, 0x1932 }, { 0x1939, 0x193B },
     { 0x1A17, 0x1A18 }, { 0x1A1B, 0x1A1B }, { 0x1A56, 0x1A56 },
     { 0x1A58, 0x1A60 }, { 0x1A62, 0x1A62 }, { 0x1A65, 0x1A6C },
     { 0x1A73, 0x1A7F }, { 0x1AB0, 0x1B03 }, { 0x1B34, 0x1B34 },
     { 0x1B36, 0x1B3A }, { 0x1B3C, 0x1B3C }, { 0x1B42, 0x1B42 },
     { 0x1B6B, 0x1B73 }, { 0x1B80, 0x1B81 }, { 0x1BA2, 0x1BA5 },
     { 0x1BA8, 0x1BA9 }, { 0x1BAB, 0x1BAD }, { 0x1BE6, 0x1BE6 },
     { 0x1BE8, 0x1BE9 }, { 0x1BED, 0x1BED }, { 0x1BEF, 0x1BF1 },
     { 0x1C2C, 0x1C33 }, { 0x1C36, 0x1C37 }, { 0x1CD0, 0x1CD2 },
     { 0x1CD4, 0x1CE0 }, { 0x1CE2, 0x1CE8 }, { 0x1CED, 0x1CED },
     { 0x1CF4, 0x1CF4 }, { 0x1CF8, 0x1CF9 }, { 0x1DC0, 0x1DFF },
     { 0x200B, 0x200F }, { 0x202A, 0x202E }, { 0x2060, 0x206F },
     { 0x20D0, 0x20F0 }, { 0x2CEF, 0x2CF1 }, { 0x2D7F, 0x2D7F },
     { 0x2DE0, 0x2DFF }, { 0x302A, 0x302D }, { 0x3099, 0x309A },
     { 0xA66F, 0xA672 }, { 0xA674, 0xA67D }, { 0xA69E, 0xA69F },
     { 0xA6F0, 0xA6F1 }, { 0xA802, 0xA802 }, { 0xA806, 0xA806 },
     { 0xA80B, 0xA80B }, { 0xA825, 0xA826 }, { 0xA82C, 0xA82C },
     { 0xA8C4, 0xA8C5 }, { 0xA8E0, 0xA8F1 }, { 0xA8FF, 0xA8FF },
     { 0xA926, 0xA92D }, { 0xA947, 0xA951 }, { 0xA980, 0xA982 },
     { 0xA9B3, 0xA9B3 }, { 0xA9B6, 0xA9B9 }, { 0xA9BC, 0xA9BD },
     { 0xA9E5, 0xA9E5 }, { 0xAA29, 0xAA2E }, { 0xAA31, 0xAA32 },
     { 0xAA35, 0xAA36 }, { 0xAA43, 0xAA43 }, { 0xAA4C, 0xAA4C },
     { 0xAA7C, 0xAA7C }, { 0xAAB0, 0xAAB0 }, { 0xAAB2, 0xAAB4 },
     { 0xAAB7, 0xAAB8 }, { 0xAABE, 0xAABF }, { 0xAAC1, 0xAAC1 },
     { 0xAAEC, 0xAAED }, { 0xAAF6, 0xAAF6 }, { 0xABE5, 0xABE5 },
     { 0xABE8, 0xABE8 }, { 0xABED, 0xABED }, { 0xFB1E, 0xFB1E },
     { 0xFE00, 0xFE0F }, { 0xFE20, 0xFE2F }, { 0xFEFF, 0xFEFF },
     { 0xFFF9, 0xFFFB }, { 0x01FD, 0x01FD }, { 0x02E0, 0x02E0 },
     { 0x0376, 0x037A }, { 0x0A01, 0x0A0F }, { 0x0A38, 0x0A3F },
     { 0x0AE5, 0x0AE6 }, { 0x0D24, 0x0D27 }, { 0x0EAB, 0x0EAC },
     { 0x0F46, 0x0F50 }, { 0x0F82, 0x0F85 }, { 0x1001, 0x1001 },
     { 0x1038, 0x1046 }, { 0x1070, 0x1070 }, { 0x1073, 0x1074 },
     { 0x107F, 0x1081 }, { 0x10B3, 0x10B6 }, { 0x10B9, 0x10BA },
     { 0x10BD, 0x10BD }, { 0x10C2, 0x10CD }, { 0x1100, 0x1102 },
     { 0x1127, 0x112B }, { 0x112D, 0x1134 }, { 0x1173, 0x1173 },
     { 0x1180, 0x1181 }, { 0x11B6, 0x11BE }, { 0x11C9, 0x11CC },
     { 0x11CF, 0x11CF }, { 0x122F, 0x1231 }, { 0x1234, 0x1234 },
     { 0x1236, 0x1237 }, { 0x123E, 0x123E }, { 0x12DF, 0x12DF },
     { 0x12E3, 0x12EA }, { 0x1300, 0x1301 }, { 0x133B, 0x133C },
     { 0x1340, 0x1340 }, { 0x1366, 0x1374 }, { 0x1438, 0x143F },
     { 0x1442, 0x1444 }, { 0x1446, 0x1446 }, { 0x145E, 0x145E },
     { 0x14B3, 0x14B8 }, { 0x14BA, 0x14BA }, { 0x14BF, 0x14C0 },
     { 0x14C2, 0x14C3 }, { 0x15B2, 0x15B5 }, { 0x15BC, 0x15BD },
     { 0x15BF, 0x15C0 }, { 0x15DC, 0x162F }, { 0x1633, 0x163A },
     { 0x163D, 0x163D }, { 0x163F, 0x1640 }, { 0x16AB, 0x16AB },
     { 0x16AD, 0x16AD }, { 0x16B0, 0x16B5 }, { 0x16B7, 0x16B7 },
     { 0x171D, 0x171F }, { 0x1722, 0x1725 }, { 0x1727, 0x172B },
     { 0x182F, 0x1837 }, { 0x1839, 0x183A }, { 0x193B, 0x193C },
     { 0x193E, 0x193E }, { 0x1943, 0x1943 }, { 0x19D4, 0x19DB },
     { 0x19E0, 0x19E0 }, { 0x1A01, 0x1A0A }, { 0x1A33, 0x1A38 },
     { 0x1A3B, 0x1A3E }, { 0x1A47, 0x1A47 }, { 0x1A51, 0x1A56 },
     { 0x1A59, 0x1A5B }, { 0x1A8A, 0x1A96 }, { 0x1A98, 0x1A99 },
     { 0x1C30, 0x1C3D }, { 0x1C3F, 0x1C3F }, { 0x1C92, 0x1CA7 },
     { 0x1CAA, 0x1CB0 }, { 0x1CB2, 0x1CB3 }, { 0x1CB5, 0x1CB6 },
     { 0x1D31, 0x1D45 }, { 0x1D47, 0x1D47 }, { 0x1D90, 0x1D91 },
     { 0x1D95, 0x1D95 }, { 0x1D97, 0x1D97 }, { 0x1EF3, 0x1EF4 },
     { 0x3430, 0x3438 }, { 0x6AF0, 0x6AF4 }, { 0x6B30, 0x6B36 },
     { 0x6F4F, 0x6F4F }, { 0x6F8F, 0x6F92 }, { 0x6FE4, 0x6FE4 },
     { 0xBC9D, 0xBC9E }, { 0xBCA0, 0xCF46 }, { 0xD167, 0xD169 },
     { 0xD173, 0xD182 }, { 0xD185, 0xD18B }, { 0xD1AA, 0xD1AD },
     { 0xD242, 0xD244 }, { 0xDA00, 0xDA36 }, { 0xDA3B, 0xDA6C },
     { 0xDA75, 0xDA75 }, { 0xDA84, 0xDA84 }, { 0xDA9B, 0xDAAF },
     { 0xE000, 0xE02A }, { 0xE130, 0xE136 }, { 0xE2AE, 0xE2AE },
     { 0xE2EC, 0xE2EF }, { 0xE8D0, 0xE8D6 }, { 0xE944, 0xE94A },
     { 0x0001, 0x01EF } };

const uint16_t tbl_for_fullwidth_chars[][2] = {
     { /* plane 0 */ 0, 46 },
     { /* plane 1 */ 46, 80 },
     { /* plane 2 */ 80, 81 },
     { /* plane 3 */ 81, 82 },
     { /* plane 4 */ 0, 0 },
     { /* plane 5 */ 0, 0 },
     { /* plane 6 */ 0, 0 },
     { /* plane 7 */ 0, 0 },
     { /* plane 8 */ 0, 0 },
     { /* plane 9 */ 0, 0 },
     { /* plane 10 */ 0, 0 },
     { /* plane 11 */ 0, 0 },
     { /* plane 12 */ 0, 0 },
     { /* plane 13 */ 0, 0 },
     { /* plane 14 */ 0, 0 },
     { /* plane 15 */ 0, 0 },
     { /* plane 16 */ 0, 0 },
     { 0x1100, 0x115F }, { 0x231A, 0x231B }, { 0x2329, 0x232A },
     { 0x23E9, 0x23EC }, { 0x23F0, 0x23F0 }, { 0x23F3, 0x23F3 },
     { 0x25FD, 0x25FE }, { 0x2614, 0x2615 }, { 0x2648, 0x2653 },
     { 0x267F, 0x267F }, { 0x2693, 0x2693 }, { 0x26A1, 0x26A1 },
     { 0x26AA, 0x26AB }, { 0x26BD, 0x26BE }, { 0x26C4, 0x26C5 },
     { 0x26CE, 0x26CE }, { 0x26D4, 0x26D4 }, { 0x26EA, 0x26EA },
     { 0x26F2, 0x26F3 }, { 0x26F5, 0x26F5 }, { 0x26FA, 0x26FA },
     { 0x26FD, 0x26FD }, { 0x2705, 0x2705 }, { 0x270A, 0x270B },
     { 0x2728, 0x2728 }, { 0x274C, 0x274C }, { 0x274E, 0x274E },
     { 0x2753, 0x2755 }, { 0x2757, 0x2757 }, { 0x2795, 0x2797 },
     { 0x27B0, 0x27B0 }, { 0x27BF, 0x27BF }, { 0x2B1B, 0x2B1C },
     { 0x2B50, 0x2B50 }, { 0x2B55, 0x2B55 }, { 0x2E80, 0x303E },
     { 0x3041, 0x3247 }, { 0x3250, 0x4DBF }, { 0x4E00, 0xA4C6 },
     { 0xA960, 0xA97C }, { 0xAC00, 0xD7A3 }, { 0xF900, 0xFAFF },
     { 0xFE10, 0xFE19 }, { 0xFE30, 0xFE6B }, { 0xFF01, 0xFF60 },
     { 0xFFE0, 0xFFE6 }, { 0x6FE0, 0xB2FB }, { 0xF004, 0xF004 },
     { 0xF0CF, 0xF0CF }, { 0xF18E, 0xF18E }, { 0xF191, 0xF19A },
     { 0xF200, 0xF320 }, { 0xF32D, 0xF335 }, { 0xF337, 0xF37C },
     { 0xF37E, 0xF393 }, { 0xF3A0, 0xF3CA }, { 0xF3CF, 0xF3D3 },
     { 0xF3E0, 0xF3F0 }, { 0xF3F4, 0xF3F4 }, { 0xF3F8, 0xF43E },
     { 0xF440, 0xF440 }, { 0xF442, 0xF4FC }, { 0xF4FF, 0xF53D },
     { 0xF54B, 0xF54E }, { 0xF550, 0xF567 }, { 0xF57A, 0xF57A },
     { 0xF595, 0xF596 }, { 0xF5A4, 0xF5A4 }, { 0xF5FB, 0xF64F },
     { 0xF680, 0xF6C5 }, { 0xF6CC, 0xF6CC }, { 0xF6D0, 0xF6D2 },
     { 0xF6D5, 0xF6DF }, { 0xF6EB, 0xF6EC }, { 0xF6F4, 0xF6FC },
     { 0xF7E0, 0xF7F0 }, { 0xF90C, 0xF93A }, { 0xF93C, 0xF945 },
     { 0xF947, 0xF9FF }, { 0xFA70, 0xFAF6 }, { 0x0000, 0xFFFF },
     { 0x0000, 0xFFFD } };

  /* test for 8-bit control characters */
    if (ucs == 0)
       return 0;
    if (ucs < 32 || (ucs >= 0x7f && ucs < 0xa0))
       return -1;

    if( ucs < 0x300)                /* everything else up to 0x300 is a */
       return( 1);                  /* plain old single-width character */
    if( ucs >= 0x110000)
       return( 1);
  /* binary search in table of non-spacing characters */
    if( _uniset_test( tbl_for_zero_width_chars, ucs))
       return 0;

  /* if we arrive here, ucs is not a combining or C0/C1 control character */
    if( _uniset_test( tbl_for_fullwidth_chars, ucs))
       return 2;
    else
       return 1;
}
#endif

#ifdef USING_COMBINING_CHARACTER_SCHEME

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

int PDC_find_combined_char_idx( const cchar_t root, const cchar_t added)
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

#define IS_LOW_SURROGATE( c) ((c) >= 0xdc00 && (c) < 0xe000)
#define IS_HIGH_SURROGATE( c) ((c) >= 0xd800 && (c) < 0xdc00)

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
    assert( (int)c >= COMBINED_CHAR_START && (int)c < COMBINED_CHAR_START + n_combos);
    *added = combos[c - COMBINED_CHAR_START].added;
    return( combos[c - COMBINED_CHAR_START].root);
}

#endif      /* #ifdef USING_COMBINING_CHARACTER_SCHEME  */
#endif      /* #ifdef PDC_WIDE                        */

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

    assert( SP);
    assert( win);
    if (!win || !SP)
        return ERR;

    x = win->_curx;
    y = win->_cury;

    if (y > win->_maxy || x > win->_maxx || y < 0 || x < 0)
        return ERR;

    xlat = !SP->raw_out && !(ch & A_ALTCHARSET);
    text = ch & A_CHARTEXT;
    attr = ch & A_ATTRIBUTES;
#ifdef USING_COMBINING_CHARACTER_SCHEME
    text_width = PDC_wcwidth( (int)text);

    if( x || y)
    {
        const bool is_combining = (text && !text_width);
        const bool is_low_surrogate = IS_LOW_SURROGATE( text);

        if( is_combining || is_low_surrogate)
        {
            chtype prev_char;

            if( x)
                x--;
            else
            {
                y--;
                x = win->_maxx - 1;
            }
            prev_char = win->_y[y][x] & A_CHARTEXT;
            if( is_combining)
                text = COMBINED_CHAR_START
                         + PDC_find_combined_char_idx( prev_char, text);
            else if( IS_HIGH_SURROGATE( prev_char))
                text = 0x10000 + ((prev_char - 0xd800) << 10) + (text - 0xdc00);
            else     /* low surrogate after a non-high surrogate;  not */
                text = prev_char;   /* supposed to happen */
        }
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

               /* Had this commented out.  I think it matters in */
               /* wide,  non-UTF8 mode on some platforms. */
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

        /* Only change 'dirty' cells if the character to be added is
           different from the character/attribute that is already in
           that position in the window. */

        if (win->_y[y][x] != text)
        {
            PDC_mark_cell_as_changed( win, y, x);
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

    assert( wch);
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

    assert( wch);
    if (!wch || (wadd_wch(win, wch) == ERR))
        return ERR;

    return wrefresh(win);
}
#endif
