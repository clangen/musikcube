/* Public Domain Curses */

#include "pdcwin.h"

#include <stdlib.h>
#include <string.h>

#ifdef CHTYPE_LONG

#ifdef PDC_WIDE
   #define USE_UNICODE_ACS_CHARS 1
#else
   #define USE_UNICODE_ACS_CHARS 0
#endif

#include "acs_defs.h"

#endif

/* position hardware cursor at (y, x) */

void PDC_gotoyx(int row, int col)
{
    COORD coord;

    PDC_LOG(("PDC_gotoyx() - called: row %d col %d from row %d col %d\n",
             row, col, SP->cursrow, SP->curscol));

    coord.X = col;
    coord.Y = row;

    SetConsoleCursorPosition(pdc_con_out, coord);
}

/* update the given physical line to look like the corresponding line in
   curscr */

/* NOTE:  the original indexing into pdc_atrtab[] relied on three or five
   attribute bits in 'chtype' being adjacent to the color bits.  Such is
   not the case for 64-bit chtypes (CHTYPE_LONG == 2),  so we have to do
   additional bit-fiddling for that situation.  Code is similar in Win32
   and DOS flavors.  (BJG) */
#define MAX_UNICODE                  0x10ffff
#define DUMMY_CHAR_NEXT_TO_FULLWIDTH (MAX_UNICODE + 1)

/* see 'addch.c' for an explanation of how combining chars are handled. */

#if defined( CHTYPE_LONG) && CHTYPE_LONG >= 2 && defined( PDC_WIDE)
   #define USING_COMBINING_CHARACTER_SCHEME
   int PDC_expand_combined_characters( const cchar_t c, cchar_t *added);  /* addch.c */
#endif

void PDC_transform_line(int lineno, int x, int len, const chtype *srcp)
{
    CHAR_INFO ci[512];
    int src, dst;
    COORD bufSize, bufPos;
    SMALL_RECT sr;

    PDC_LOG(("PDC_transform_line() - called: lineno=%d\n", lineno));

    bufPos.X = bufPos.Y = 0;

    sr.Top = lineno;
    sr.Bottom = lineno;
    sr.Left = x;
    sr.Right = x + len - 1;

    for (src = dst = 0; src < len; src++)
//      if( (srcp[src] & A_CHARTEXT) != DUMMY_CHAR_NEXT_TO_FULLWIDTH)
        {
            const chtype ch = srcp[src];
            chtype char_out = ch & A_CHARTEXT;

#if defined( CHTYPE_LONG) && (CHTYPE_LONG >= 2)
            ci[dst].Attributes = pdc_atrtab[((ch >> PDC_ATTR_SHIFT) & 0x1f)
                     | (((ch >> PDC_COLOR_SHIFT) & 0xff) << 5)];
#else
            ci[dst].Attributes = pdc_atrtab[ch >> PDC_ATTR_SHIFT];
#endif

#ifdef CHTYPE_LONG
            if( char_out == DUMMY_CHAR_NEXT_TO_FULLWIDTH)
                char_out = ' ';
            else if (ch & A_ALTCHARSET && !(ch & 0xff80))
                char_out = acs_map[ch & 0x7f];

#ifdef USING_COMBINING_CHARACTER_SCHEME
         /* We can't actually display combining characters in cmd.exe.  So
         show the 'base' character and throw away the modifying marks. */
            if( char_out > MAX_UNICODE)
            {
                cchar_t added;
                int n_combined = 0;

                while( (char_out = PDC_expand_combined_characters( char_out,
                                   &added)) > MAX_UNICODE)
                {
                    n_combined++;
                }
            }
#endif
            if( char_out > 0xffff)  /* SMP chars: use surrogates */
            {
                ci[dst].Char.UnicodeChar = (WCHAR)( 0xd800 | (char_out>>10));
                ci[dst + 1] = ci[dst];
                dst++;
                char_out = (chtype)( 0xdc00 | (char_out & 0x3ff));
            }
#endif
            ci[dst].Char.UnicodeChar = (WCHAR)char_out;

            dst++;
        }

    bufSize.X = dst;
    bufSize.Y = 1;

    WriteConsoleOutput(pdc_con_out, ci, bufSize, bufPos, &sr);
}
