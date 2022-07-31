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
   wch.  If opts is non-NULL,  it is treated as a pointer to an integer
   and the color pair is stored in it (this is an ncurses extension).
   When wch is NULL, getcchar() merely returns the number of wide
   characters in wcval.

   setcchar constructs a cchar_t at wcval from the wide-character text
   at wch, the attributes in attr and the color pair in color_pair.  If
   the opts argument is non-NULL,  it is treated as a pointer to an
   integer containing the desired color pair and color_pair is ignored.

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
    INTENTIONALLY_UNUSED_PARAMETER( x);
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

   if( code < 0)
       n_bytes_out = 0;
   else if (code < 0x80)
   {
       if( dest)
           dest[0] = (char)code;
       n_bytes_out = 1;
   }
   else
       if (code < 0x800)
       {
           if( dest)
           {
               dest[0] = (char) (((code >> 6) & 0x1f) | 0xc0);
               dest[1] = (char) ((code & 0x3f) | 0x80);
           }
           n_bytes_out = 2;
       }
       else if( code < 0x10000)
       {
           if( dest)
           {
               dest[0] = (char) (((code >> 12) & 0x0f) | 0xe0);
               dest[1] = (char) (((code >> 6) & 0x3f) | 0x80);
               dest[2] = (char) ((code & 0x3f) | 0x80);
           }
           n_bytes_out = 3;
       }
       else if( code < 0x110000)      /* Unicode past 64K,  i.e.,  SMP */
       {
           if( dest)
           {
               dest[0] = (char) (((code >> 18) & 0x0f) | 0xf0);
               dest[1] = (char) (((code >> 12) & 0x3f) | 0x80);
               dest[2] = (char) (((code >> 6) & 0x3f) | 0x80);
               dest[3] = (char) ((code & 0x3f) | 0x80);
           }
           n_bytes_out = 4;
       }
       else                      /* not valid Unicode */
           n_bytes_out = 0;
   return( n_bytes_out);
}

#ifdef PDC_WIDE

         /* I think that only under Windows is wchar_t 16 bits. */
#ifdef _WIN32
   #define WCHAR_T_IS_16_BITS
#endif

   /* This expands a string of wchar_t values,  possibly including surrogate
   pairs,  into an array of int32_t Unicode points.  The output array will
   contain exactly as many values as the input array,  _unless_ the input
   has Unicode surrogate pairs in it.  In that case,  each input pair will
   result in only one output value. */

#define IS_HIGH_SURROGATE( x)  ((x) >= 0xd800 && (x) < 0xdc00)
#define IS_LOW_SURROGATE( x)   ((x) >= 0xdc00 && (x) < 0xe000)
#define IS_SURROGATE( x)       ((x) >= 0xd800 && (x) < 0xe000)

static int _wchar_to_int32_array( int32_t *obuff, const int obuffsize, const wchar_t *wch)
{
    int i;

    for( i = 0; i < obuffsize && *wch; i++)
    {
        if( IS_SURROGATE( wch[0]))
        {
            if( IS_LOW_SURROGATE( wch[1]) && IS_HIGH_SURROGATE( wch[0]))
                obuff[i] = (((int32_t)wch[0] - 0xd800) << 10) + 0x10000
                       + (int32_t)wch[1] - 0xdc00;
            else         /* malformed surrogate pair */
                return( -1);
            wch++;
            wch++;
        }
        else
            obuff[i] = *wch++;
    }
    if( i < obuffsize)
        obuff[i] = (int32_t)0;
    else             /* no room for null terminator */
        i = -1;
    return( i);
}

/* Inverse of the above function : given a null-terminated array of Unicode
points,  encode them as a null-terminated array of wchar_t values.  On
strange systems where wchar_t does not handle all of Unicode (is 16 bits),
such as Microsoft Windows,  input values in the SMP will be converted to
a surrogate pair of wchar_t values.  On more modern systems,  the output
will essentially equal the input.  */

static int _int32_to_wchar_array( wchar_t *obuff, const int obuffsize, const int32_t *wint)
{
    int i = 0;

    if( !obuff)         /* just getting the size of the output array */
    {
#ifdef WCHAR_T_IS_16_BITS
        while( *wint)
            i += 1 + (*wint++ >= 0x10000 ? 1 : 0);
#else
        while( *wint++)
            i++;
#endif
        return( i + 1);    /* include the '\0' terminator */
    }
    while( i < obuffsize && *wint)
    {
#ifdef WCHAR_T_IS_16_BITS
        if( *wint >= 0x10000)    /* make surrogate pair */
        {
            obuff[i++] = (wchar_t)( 0xd800 + (*wint >> 10));
            if( i < obuffsize)
                obuff[i++] = (wchar_t)( 0xdc00 + (*wint & 0x3ff));
            wint++;
        }
        else
#endif
            obuff[i++] = (wchar_t)*wint++;
    }
    if( i < obuffsize)
        obuff[i++] = '\0';
    else       /* didn't fit in the buffer */
        i = -1;
    return( i);
}

#ifdef USING_COMBINING_CHARACTER_SCHEME
   int PDC_expand_combined_characters( const cchar_t c, cchar_t *added);
   int PDC_find_combined_char_idx( const cchar_t root, const cchar_t added);

   #define COMBINED_CHAR_START          0x110001
#endif

int getcchar(const cchar_t *wcval, wchar_t *wch, attr_t *attrs,
             short *color_pair, void *opts)
{
    int32_t c[20];
    int n = 0;

    assert( wcval);
    if (!wcval)
        return ERR;
    c[0] = (int32_t)( *wcval & A_CHARTEXT);
            /* TODO: if c[0] == 0x110000,  it's a placeholder with a
            fullwidth character to its left.  If c[0] > 0x110001,  it's
            a marker for a combining character string. */
#ifdef USING_COMBINING_CHARACTER_SCHEME
    while( n < 10 && c[n] >= COMBINED_CHAR_START)
    {
        cchar_t added;

        c[n + 1] = PDC_expand_combined_characters( c[n], &added);
        c[n] = (int32_t)added;
        n++;
    }
#endif
    c[++n] = 0;
    if( !wch)
        return( c[0] ? _int32_to_wchar_array( NULL, 0, c) : -1);
    else
    {
        int i, j;
        int32_t swap_val;

        for( i = 0, j = n - 1; i < j; i++, j--)
        {
            swap_val = c[i];
            c[i] = c[j];
            c[j] = swap_val;
        }
        _int32_to_wchar_array( wch, 20, c);
        assert( attrs);
        assert( color_pair);
        if (!attrs || !color_pair)
            return ERR;

        *attrs = (*wcval & (A_ATTRIBUTES & ~A_COLOR));
        *color_pair = (short)( PAIR_NUMBER(*wcval & A_COLOR));
        if( opts)
            *(int *)opts = (int)( PAIR_NUMBER(*wcval & A_COLOR));
        return OK;
    }
}

int setcchar(cchar_t *wcval, const wchar_t *wch, const attr_t attrs,
             short color_pair, const void *opts)
{
    int32_t ochar[20], rval;
#ifdef USING_COMBINING_CHARACTER_SCHEME
    int i;
#endif

    const int integer_color_pair = (opts ? *(int *)opts : (int)color_pair);
    assert( wcval);
    assert( wch);
    if (!wcval || !wch)
        return ERR;
    _wchar_to_int32_array( ochar, 20, wch);
    rval = ochar[0];
         /* If len_out > 1,  we have combining characters.  See */
         /* 'addch.c' for a discussion of how we handle those.  */
#ifdef USING_COMBINING_CHARACTER_SCHEME
    for( i = 1; ochar[i]; i++)
        rval = COMBINED_CHAR_START + PDC_find_combined_char_idx( rval, ochar[i]);
#endif
    *wcval = rval | attrs | COLOR_PAIR(integer_color_pair);
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
    uint32_t key;
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

    if( !(key & 0x80))      /* 'ordinary' 7-bit ASCII */
        i = 1;
    else if ((key & 0xc0) == 0xc0 && IS_CONTINUATION_BYTE( string[1]))
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

    if( i > 0)
       *pwc = (wchar_t)key;

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
            return (size_t)-1;

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
