/* PDCurses */

#include <curspriv.h>
#include <assert.h>

/*man-start**************************************************************

util
----

### Synopsis

    char *unctrl(chtype c);
    void filter(void);
    void use_env(bool x);
    int delay_output(int ms);

    int getcchar(const cchar_t *wcval, wchar_t *wch, attr_t *attrs,
                 short *color_pair, void *opts);
    int setcchar(cchar_t *wcval, const wchar_t *wch, const attr_t attrs,
                 short color_pair, const void *opts);
    wchar_t *wunctrl(cchar_t *wc);

    int PDC_mbtowc(wchar_t *pwc, const char *s, size_t n);
    size_t PDC_mbstowcs(wchar_t *dest, const char *src, size_t n);
    size_t PDC_wcstombs(char *dest, const wchar_t *src, size_t n);

### Description

   unctrl() expands the text portion of the chtype c into a printable
   string. Control characters are changed to the "^X" notation; others
   are passed through. wunctrl() is the wide-character version of the
   function.

   filter() and use_env() are no-ops in PDCurses.

   delay_output() inserts an ms millisecond pause in output.

   getcchar() works in two modes: When wch is not NULL, it reads the
   cchar_t pointed to by wcval and stores the attributes in attrs, the
   color pair in color_pair, and the text in the wide-character string
   wch. When wch is NULL, getcchar() merely returns the number of wide
   characters in wcval. In either mode, the opts argument is unused.

   setcchar constructs a cchar_t at wcval from the wide-character text
   at wch, the attributes in attr and the color pair in color_pair. The
   opts argument is unused.

   Currently, the length returned by getcchar() is always 1 or 0.
   Similarly, setcchar() will only take the first wide character from
   wch, and ignore any others that it "should" take (i.e., combining
   characters). Nor will it correctly handle any character outside the
   basic multilingual plane (UCS-2).

### Return Value

   wunctrl() returns NULL on failure. delay_output() always returns OK.

   getcchar() returns the number of wide characters wcval points to when
   wch is NULL; when it's not, getcchar() returns OK or ERR.

   setcchar() returns OK or ERR.

### Portability
                             X/Open  ncurses  NetBSD
    unctrl                      Y       Y       Y
    filter                      Y       Y       Y
    use_env                     Y       Y       Y
    delay_output                Y       Y       Y
    getcchar                    Y       Y       Y
    setcchar                    Y       Y       Y
    wunctrl                     Y       Y       Y
    PDC_mbtowc                  -       -       -
    PDC_mbstowcs                -       -       -
    PDC_wcstombs                -       -       -

**man-end****************************************************************/

#ifdef PDC_WIDE
# ifdef PDC_FORCE_UTF8
#  include <string.h>
# else
#  include <stdlib.h>
# endif
#endif

char *unctrl(chtype c)
{
    static char strbuf[3] = {0, 0, 0};

    chtype ic;

    PDC_LOG(("unctrl() - called\n"));

    ic = c & A_CHARTEXT;

    if (ic >= 0x20 && ic != 0x7f)       /* normal characters */
    {
        strbuf[0] = (char)ic;
        strbuf[1] = '\0';
        return strbuf;
    }

    strbuf[0] = '^';            /* '^' prefix */

    if (ic == 0x7f)             /* 0x7f == DEL */
        strbuf[1] = '?';
    else                    /* other control */
        strbuf[1] = (char)(ic + '@');

    return strbuf;
}

void filter(void)
{
    PDC_LOG(("filter() - called\n"));
}

void use_env(bool x)
{
    PDC_LOG(("use_env() - called: x %d\n", x));
}

int delay_output(int ms)
{
    PDC_LOG(("delay_output() - called: ms %d\n", ms));

    return napms(ms);
}

int PDC_wc_to_utf8( char *dest, const int32_t code)
{
   int n_bytes_out;

   if (code < 0x80)
   {
       dest[0] = (char)code;
       n_bytes_out = 1;
   }
   else
       if (code < 0x800)
       {
           dest[0] = (char) (((code >> 6) & 0x1f) | 0xc0);
           dest[1] = (char) ((code & 0x3f) | 0x80);
           n_bytes_out = 2;
       }
       else if( code < 0x10000)
       {
           dest[0] = (char) (((code >> 12) & 0x0f) | 0xe0);
           dest[1] = (char) (((code >> 6) & 0x3f) | 0x80);
           dest[2] = (char) ((code & 0x3f) | 0x80);
           n_bytes_out = 3;
       }
       else       /* Unicode past 64K,  i.e.,  SMP */
       {
           dest[0] = (char) (((code >> 18) & 0x0f) | 0xf0);
           dest[1] = (char) (((code >> 12) & 0x3f) | 0x80);
           dest[2] = (char) (((code >> 6) & 0x3f) | 0x80);
           dest[3] = (char) ((code & 0x3f) | 0x80);
           n_bytes_out = 4;
       }
   return( n_bytes_out);
}

#ifdef PDC_WIDE
int getcchar(const cchar_t *wcval, wchar_t *wch, attr_t *attrs,
             short *color_pair, void *opts)
{
    assert( wcval);
    if (!wcval)
        return ERR;

    if (wch)
    {
        assert( attrs);
        assert( color_pair);
        if (!attrs || !color_pair)
            return ERR;

        *wch = (wchar_t)(*wcval & A_CHARTEXT);
        *attrs = (*wcval & (A_ATTRIBUTES & ~A_COLOR));
        *color_pair = (short)( PAIR_NUMBER(*wcval & A_COLOR));

        if (*wch)
            *++wch = L'\0';

        return OK;
    }
    else
        return ((*wcval & A_CHARTEXT) != L'\0');
}

int setcchar(cchar_t *wcval, const wchar_t *wch, const attr_t attrs,
             short color_pair, const void *opts)
{
    assert( wcval);
    assert( wch);
    if (!wcval || !wch)
        return ERR;

    *wcval = *wch | attrs | COLOR_PAIR(color_pair);

    return OK;
}

wchar_t *wunctrl(cchar_t *wc)
{
    static wchar_t strbuf[3] = {0, 0, 0};

    cchar_t ic;

    PDC_LOG(("wunctrl() - called\n"));

    assert( wc);
    if (!wc)
        return NULL;

    ic = *wc & A_CHARTEXT;

    if (ic >= 0x20 && ic != 0x7f)       /* normal characters */
    {
        strbuf[0] = (wchar_t)ic;
        strbuf[1] = L'\0';
        return strbuf;
    }

    strbuf[0] = '^';            /* '^' prefix */

    if (ic == 0x7f)             /* 0x7f == DEL */
        strbuf[1] = '?';
    else                    /* other control */
        strbuf[1] = (wchar_t)(ic + '@');

    return strbuf;
}

#define IS_CONTINUATION_BYTE( c) (((c) & 0xc0) == 0x80)

int PDC_mbtowc(wchar_t *pwc, const char *s, size_t n)
{
# ifdef PDC_FORCE_UTF8
    wchar_t key;
    int i = -1;
    const unsigned char *string;

    assert( s);
    assert( pwc);
    if (!s || (n < 1))
        return -1;

    if (!*s)
        return 0;

    string = (const unsigned char *)s;

    key = string[0];

    /* Simplistic UTF-8 decoder -- a little validation */

    if ((key & 0xc0) == 0xc0 && IS_CONTINUATION_BYTE( string[1]))
    {
        if ((key & 0xe0) == 0xc0 && 1 < n)
        {
            key = ((key & 0x1f) << 6) | (string[1] & 0x3f);
            i = 2;      /* two-byte sequence : U+0080 to U+07FF */
        }
        else if ((key & 0xf0) == 0xe0 && 2 < n
                  && IS_CONTINUATION_BYTE( string[2]))
        {
            key = ((key & 0x0f) << 12) | ((string[1] & 0x3f) << 6) |
                  (string[2] & 0x3f);
            i = 3;      /* three-byte sequence : U+0800 to U+FFFF */
        }
        else if ((key & 0xf8) == 0xf0 && 3 < n    /* SMP:  Unicode past 64K */
                  && IS_CONTINUATION_BYTE( string[2])
                  && IS_CONTINUATION_BYTE( string[3]))
        {
            key = ((key & 0x07) << 18) | ((string[1] & 0x3f) << 12) |
                  ((string[2] & 0x3f) << 6) | (string[2] & 0x3f);
            if( key <= 0x10ffff)
                i = 4;     /* four-byte sequence : U+10000 to U+10FFFF */
        }
    }
    else             /* 'ordinary' 7-bit ASCII */
        i = 1;

    *pwc = key;

    return i;
# else
    assert( s);
    assert( pwc);
    return mbtowc(pwc, s, n);
# endif
}

size_t PDC_mbstowcs(wchar_t *dest, const char *src, size_t n)
{
# ifdef PDC_FORCE_UTF8
    size_t i = 0, len;

    assert( src);
    assert( dest);
    if (!src || !dest)
        return 0;

    len = strlen(src);

    while (*src && i < n)
    {
        int retval = PDC_mbtowc(dest + i, src, len);

        if (retval < 1)
            return -1;

        src += retval;
        len -= retval;
        i++;
    }
# else
    size_t i = mbstowcs(dest, src, n);
# endif
    dest[i] = 0;
    return i;
}

size_t PDC_wcstombs(char *dest, const wchar_t *src, size_t n)
{
# ifdef PDC_FORCE_UTF8
    size_t i = 0;

    assert( src);
    assert( dest);
    if (!src || !dest)
        return 0;

    while (*src && i < n)
       i += PDC_wc_to_utf8( dest + i, *src++);
# else
    size_t i = wcstombs(dest, src, n);
# endif
    dest[i] = '\0';
    return i;
}
#endif
