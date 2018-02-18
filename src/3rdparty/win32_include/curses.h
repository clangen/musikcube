/* Public Domain Curses */

/*----------------------------------------------------------------------*
 *                              PDCurses                                *
 *----------------------------------------------------------------------*/

#ifndef __PDCURSES__
#define __PDCURSES__ 1

/*man-start**************************************************************

PDCurses definitions list:  (Only define those needed)

    XCURSES         True if compiling for X11.
    PDC_RGB         True if you want to use RGB color definitions
                    (Red = 1, Green = 2, Blue = 4) instead of BGR.
    PDC_WIDE        True if building wide-character support.
    PDC_DLL_BUILD   True if building a Win32 DLL.
    NCURSES_MOUSE_VERSION   Use the ncurses mouse API instead
                            of PDCurses' traditional mouse API.

PDCurses portable platform definitions list:

    PDC_BUILD       Defines API build version.
    PDCURSES        Enables access to PDCurses-only routines.
    XOPEN           Always true.
    SYSVcurses      True if you are compiling for SYSV portability.
    BSDcurses       True if you are compiling for BSD portability.

**man-end****************************************************************/

#define PDCURSES        1      /* PDCurses-only routines */
#define XOPEN           1      /* X/Open Curses routines */
#define SYSVcurses      1      /* System V Curses routines */
#define BSDcurses       1      /* BSD Curses routines */
#if defined( CHTYPE_32)
   #define CHTYPE_LONG     1      /* chtypes will be 32 bits */
#elif !defined( CHTYPE_16)
   #define CHTYPE_LONG     2      /* chtypes will be (default) 64 bits */
#endif

/*----------------------------------------------------------------------*/

#include <stdarg.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>             /* Required by X/Open usage below */

#ifdef PDC_WIDE
# include <wchar.h>
#endif

#if defined(__cplusplus) || defined(__cplusplus__) || defined(__CPLUSPLUS)
extern "C"
{
# define bool _bool
#endif

/*----------------------------------------------------------------------
 *
 *  PDCurses Manifest Constants
 *
 */

#ifndef FALSE
# define FALSE 0
#endif
#ifndef TRUE
# define TRUE 1
#endif
#ifndef NULL
# define NULL (void *)0
#endif
#ifndef ERR
# define ERR (-1)
#endif
#ifndef OK
# define OK 0
#endif

/*----------------------------------------------------------------------
 *
 *  PDCurses Type Declarations
 *
 */

typedef unsigned char bool;    /* PDCurses Boolean type */

#ifdef CHTYPE_LONG
    #if(CHTYPE_LONG >= 2)       /* "non-standard" 64-bit chtypes     */
        typedef uint64_t chtype;
    #else                        /* "Standard" CHTYPE_LONG case,  32-bit: */
        typedef uint32_t chtype;
    # endif
#else
typedef uint16_t chtype; /* 8-bit attr + 8-bit char */
#endif

#ifdef PDC_WIDE
typedef chtype cchar_t;
#endif

typedef chtype attr_t;

/* Version constants,  available as of version 4.0 : */
/* Don't forget to update 'version.mif' if MAJOR/MINOR changes! */

#define PDC_VER_MAJOR    4
#define PDC_VER_MINOR    0
#define PDC_VER_CHANGE   2
#define PDC_VER_YEAR   2017
#define PDC_VER_MONTH   05
#define PDC_VER_DAY     07

#define PDC_BUILD (PDC_VER_MAJOR*1000 + PDC_VER_MINOR *100 + PDC_VER_CHANGE)

/* When using PDCurses as a DLL (Windows) or shared library (BSD or *nix),
it's possible to switch the DLL or shared library.  One may therefore want
to inquire of the DLL/shared library the port,  version numbers,  and
chtype_size used, and make sure they're what one was expecting.  The
'PDC_version' structure lets you do just that. */

enum PDC_port
{
    PDC_PORT_X11 = 0,
    PDC_PORT_WIN32 = 1,
    PDC_PORT_WIN32A = 2,
    PDC_PORT_DOS = 3,
    PDC_PORT_OS2 = 4,
    PDC_PORT_SDL1 = 5,
    PDC_PORT_SDL2 = 6
};

/* Detailed PDC version information */

typedef struct
{
   const enum PDC_port port;
   const int ver_major;
   const int ver_minor;
   const int ver_change;
   const size_t chtype_size;
   const bool is_wide;
   const bool is_forced_utf8;
} PDC_version_info;

/*----------------------------------------------------------------------
 *
 *  PDCurses Mouse Interface -- SYSVR4, with extensions
 *
 */

/* Most flavors of PDCurses support three buttons.  Win32a supports    */
/* these plus two "extended" buttons.  But we'll set this macro to     */
/* six,  allowing future versions to support up to nine total buttons. */
/* (The button states are broken up into two arrays to allow for the   */
/* possibility of backward compatibility to DLLs compiled with only    */
/* three mouse buttons.)                                               */

#define PDC_MAX_MOUSE_BUTTONS          9
#define PDC_N_EXTENDED_MOUSE_BUTTONS   6

typedef struct
{
    int x;           /* absolute column, 0 based, measured in characters */
    int y;           /* absolute row, 0 based, measured in characters    */
    short button[3]; /* state of three "normal" buttons                  */
    int changes;     /* flags indicating what has changed with the mouse */
    short xbutton[PDC_N_EXTENDED_MOUSE_BUTTONS]; /* state of ext buttons */
} MOUSE_STATUS;

#define BUTTON_RELEASED         0x0000
#define BUTTON_PRESSED          0x0001
#define BUTTON_CLICKED          0x0002
#define BUTTON_DOUBLE_CLICKED   0x0003
#define BUTTON_TRIPLE_CLICKED   0x0004
#define BUTTON_MOVED            0x0005  /* PDCurses */
#define WHEEL_SCROLLED          0x0006  /* PDCurses */
#define BUTTON_ACTION_MASK      0x0007  /* PDCurses */

#define PDC_BUTTON_SHIFT        0x0008  /* PDCurses */
#define PDC_BUTTON_CONTROL      0x0010  /* PDCurses */
#define PDC_BUTTON_ALT          0x0020  /* PDCurses */
#define BUTTON_MODIFIER_MASK    0x0038  /* PDCurses */

#define MOUSE_X_POS             (Mouse_status.x)
#define MOUSE_Y_POS             (Mouse_status.y)

/*
 * Bits associated with the .changes field:
 *   3         2         1         0
 * 210987654321098765432109876543210
 *                                 1 <- button 1 has changed   0
 *                                10 <- button 2 has changed   1
 *                               100 <- button 3 has changed   2
 *                              1000 <- mouse has moved        3
 *                             10000 <- mouse position report  4
 *                            100000 <- mouse wheel up         5
 *                           1000000 <- mouse wheel down       6
 *                          10000000 <- mouse wheel left       7
 *                         100000000 <- mouse wheel right      8
 *                        1000000000 <- button 4 has changed   9
 * (NOTE: buttons 6 to   10000000000 <- button 5 has changed  10
 * 9 aren't implemented 100000000000 <- button 6 has changed  11
 * in any flavor of    1000000000000 <- button 7 has changed  12
 * PDCurses yet!)     10000000000000 <- button 8 has changed  13
 *                   100000000000000 <- button 9 has changed  14
 */

#define PDC_MOUSE_MOVED         0x0008
#define PDC_MOUSE_POSITION      0x0010
#define PDC_MOUSE_WHEEL_UP      0x0020
#define PDC_MOUSE_WHEEL_DOWN    0x0040
#define PDC_MOUSE_WHEEL_LEFT    0x0080
#define PDC_MOUSE_WHEEL_RIGHT   0x0100

#define A_BUTTON_CHANGED        (Mouse_status.changes & 7)
#define MOUSE_MOVED             (Mouse_status.changes & PDC_MOUSE_MOVED)
#define MOUSE_POS_REPORT        (Mouse_status.changes & PDC_MOUSE_POSITION)
#define BUTTON_CHANGED(x)       (Mouse_status.changes & (1 << ((x) - ((x)<4 ? 1 : -5))))
#define BUTTON_STATUS(x)        (Mouse_status.button[(x) - 1])
#define MOUSE_WHEEL_UP          (Mouse_status.changes & PDC_MOUSE_WHEEL_UP)
#define MOUSE_WHEEL_DOWN        (Mouse_status.changes & PDC_MOUSE_WHEEL_DOWN)
#define MOUSE_WHEEL_LEFT        (Mouse_status.changes & PDC_MOUSE_WHEEL_LEFT)
#define MOUSE_WHEEL_RIGHT       (Mouse_status.changes & PDC_MOUSE_WHEEL_RIGHT)

/* mouse bit-masks */

#define BUTTON1_RELEASED        0x00000001L
#define BUTTON1_PRESSED         0x00000002L
#define BUTTON1_CLICKED         0x00000004L
#define BUTTON1_DOUBLE_CLICKED  0x00000008L
#define BUTTON1_TRIPLE_CLICKED  0x00000010L
#define BUTTON1_MOVED           0x00000010L /* PDCurses */

#define BUTTON2_RELEASED        0x00000020L
#define BUTTON2_PRESSED         0x00000040L
#define BUTTON2_CLICKED         0x00000080L
#define BUTTON2_DOUBLE_CLICKED  0x00000100L
#define BUTTON2_TRIPLE_CLICKED  0x00000200L
#define BUTTON2_MOVED           0x00000200L /* PDCurses */

#define BUTTON3_RELEASED        0x00000400L
#define BUTTON3_PRESSED         0x00000800L
#define BUTTON3_CLICKED         0x00001000L
#define BUTTON3_DOUBLE_CLICKED  0x00002000L
#define BUTTON3_TRIPLE_CLICKED  0x00004000L
#define BUTTON3_MOVED           0x00004000L /* PDCurses */

/* For the ncurses-compatible functions only, BUTTON4_PRESSED and
   BUTTON5_PRESSED are returned for mouse scroll wheel up and down;
   otherwise PDCurses doesn't support buttons 4 and 5... except
   as described above for Win32a,  and perhaps to be extended to
   other PDCurses flavors  */

#define BUTTON4_RELEASED        0x00008000L
#define BUTTON4_PRESSED         0x00010000L
#define BUTTON4_CLICKED         0x00020000L
#define BUTTON4_DOUBLE_CLICKED  0x00040000L
#define BUTTON4_TRIPLE_CLICKED  0x00080000L

#define BUTTON5_RELEASED        0x00100000L
#define BUTTON5_PRESSED         0x00200000L
#define BUTTON5_CLICKED         0x00400000L
#define BUTTON5_DOUBLE_CLICKED  0x00800000L
#define BUTTON5_TRIPLE_CLICKED  0x01000000L

#define MOUSE_WHEEL_SCROLL      0x02000000L /* PDCurses */
#define BUTTON_MODIFIER_SHIFT   0x04000000L /* PDCurses */
#define BUTTON_MODIFIER_CONTROL 0x08000000L /* PDCurses */
#define BUTTON_MODIFIER_ALT     0x10000000L /* PDCurses */

#define ALL_MOUSE_EVENTS        0x1fffffffL
#define REPORT_MOUSE_POSITION   0x20000000L

/* ncurses mouse interface */

typedef unsigned long mmask_t;

typedef struct
{
    short id;       /* unused, always 0 */
    int x, y, z;    /* x, y same as MOUSE_STATUS; z unused */
    mmask_t bstate; /* equivalent to changes + button[], but
                       in the same format as used for mousemask() */
} MEVENT;

#ifdef NCURSES_MOUSE_VERSION
# define BUTTON_SHIFT   BUTTON_MODIFIER_SHIFT
# define BUTTON_CONTROL BUTTON_MODIFIER_CONTROL
# define BUTTON_CTRL    BUTTON_MODIFIER_CONTROL
# define BUTTON_ALT     BUTTON_MODIFIER_ALT
#else
# define BUTTON_SHIFT   PDC_BUTTON_SHIFT
# define BUTTON_CONTROL PDC_BUTTON_CONTROL
# define BUTTON_ALT     PDC_BUTTON_ALT
#endif

/*----------------------------------------------------------------------
 *
 *  PDCurses Structure Definitions
 *
 */

typedef struct _win       /* definition of a window */
{
    int   _cury;          /* current pseudo-cursor */
    int   _curx;
    int   _maxy;          /* max window coordinates */
    int   _maxx;
    int   _begy;          /* origin on screen */
    int   _begx;
    int   _flags;         /* window properties */
    chtype _attrs;        /* standard attributes and colors */
    chtype _bkgd;         /* background, normally blank */
    bool  _clear;         /* causes clear at next refresh */
    bool  _leaveit;       /* leaves cursor where it is */
    bool  _scroll;        /* allows window scrolling */
    bool  _nodelay;       /* input character wait flag */
    bool  _immed;         /* immediate update flag */
    bool  _sync;          /* synchronise window ancestors */
    bool  _use_keypad;    /* flags keypad key mode active */
    chtype **_y;          /* pointer to line pointer array */
    int   *_firstch;      /* first changed character in line */
    int   *_lastch;       /* last changed character in line */
    int   _tmarg;         /* top of scrolling region */
    int   _bmarg;         /* bottom of scrolling region */
    int   _delayms;       /* milliseconds of delay for getch() */
    int   _parx, _pary;   /* coords relative to parent (0,0) */
    struct _win *_parent; /* subwin's pointer to parent win */
} WINDOW;

/* Avoid using the SCREEN struct directly -- use the corresponding
   functions if possible. This struct may eventually be made private. */

typedef struct
{
    bool  alive;          /* if initscr() called, and not endwin() */
    bool  autocr;         /* if cr -> lf */
    bool  cbreak;         /* if terminal unbuffered */
    bool  echo;           /* if terminal echo */
    bool  raw_inp;        /* raw input mode (v. cooked input) */
    bool  raw_out;        /* raw output mode (7 v. 8 bits) */
    bool  audible;        /* FALSE if the bell is visual */
    bool  mono;           /* TRUE if current screen is mono */
    bool  resized;        /* TRUE if TERM has been resized */
    bool  orig_attr;      /* TRUE if we have the original colors */
    short orig_fore;      /* original screen foreground color */
    short orig_back;      /* original screen foreground color */
    int   cursrow;        /* position of physical cursor */
    int   curscol;        /* position of physical cursor */
    int   visibility;     /* visibility of cursor */
    int   orig_cursor;    /* original cursor size */
    int   lines;          /* new value for LINES */
    int   cols;           /* new value for COLS */
    unsigned long _trap_mbe;       /* trap these mouse button events */
    unsigned long _map_mbe_to_key; /* map mouse buttons to slk */
    int   mouse_wait;              /* time to wait (in ms) for a
                                      button release after a press, in
                                      order to count it as a click */
    int   slklines;                /* lines in use by slk_init() */
    WINDOW *slk_winptr;            /* window for slk */
    int   linesrippedoff;          /* lines ripped off via ripoffline() */
    int   linesrippedoffontop;     /* lines ripped off on
                                      top via ripoffline() */
    int   delaytenths;             /* 1/10ths second to wait block
                                      getch() for */
    bool  _preserve;               /* TRUE if screen background
                                      to be preserved */
    int   _restore;                /* specifies if screen background
                                      to be restored, and how */
    bool  save_key_modifiers;      /* TRUE if each key modifiers saved
                                      with each key press */
    bool  return_key_modifiers;    /* TRUE if modifier keys are
                                      returned as "real" keys */
    bool  key_code;                /* TRUE if last key is a special key;
                                      used internally by get_wch() */
#ifdef XCURSES
    int   XcurscrSize;    /* size of Xcurscr shared memory block */
    bool  sb_on;
    int   sb_viewport_y;
    int   sb_viewport_x;
    int   sb_total_y;
    int   sb_total_x;
    int   sb_cur_y;
    int   sb_cur_x;
    int   exit_key;
#endif
    short line_color;     /* color of line attributes - default -1 */
} SCREEN;

/*----------------------------------------------------------------------
 *
 *  PDCurses External Variables
 *
 */

#ifdef PDC_DLL_BUILD
# ifdef CURSES_LIBRARY
#  define PDCEX __declspec(dllexport) extern
# else
#  define PDCEX __declspec(dllimport)
# endif
#else
# define PDCEX extern
#endif

PDCEX  int          LINES;        /* terminal height */
PDCEX  int          COLS;         /* terminal width */
PDCEX  WINDOW       *stdscr;      /* the default screen window */
PDCEX  WINDOW       *curscr;      /* the current screen image */
PDCEX  SCREEN       *SP;          /* curses variables */
PDCEX  MOUSE_STATUS Mouse_status;
PDCEX  int          COLORS;
PDCEX  int          COLOR_PAIRS;
PDCEX  int          TABSIZE;
PDCEX  chtype       acs_map[];    /* alternate character set map */
PDCEX  char         ttytype[];    /* terminal name/description */
PDCEX const PDC_version_info PDC_version;

/*man-start**************************************************************

PDCurses Text Attributes
========================

Originally, PDCurses used a short (16 bits) for its chtype. To include
color, a number of things had to be sacrificed from the strict Unix and
System V support. The main problem was fitting all character attributes
and color into an unsigned char (all 8 bits!).

Today, PDCurses by default uses a long (32 bits) for its chtype, as in
System V. The short chtype is still available, by undefining CHTYPE_LONG
and rebuilding the library.

The following is the structure of a win->_attrs chtype:

short form:

    +-----------------------------------------------+
    |15|14|13|12|11|10| 9| 8| 7| 6| 5| 4| 3| 2| 1| 0|
    +-----------------------------------------------+
      color number |  attrs |   character eg 'a'

The available non-color attributes are bold, reverse and blink. Others
have no effect. The high order char is an index into an array of
physical colors (defined in color.c) -- 32 foreground/background color
pairs (5 bits) plus 3 bits for other attributes.

long form:

    +--------------------------------------------------------------------+
    |31|30|29|28|27|26|25|24|23|22|21|20|19|18|17|16|15|14|13|..| 2| 1| 0|
    +--------------------------------------------------------------------+
          color number      |     modifiers         |   character eg 'a'

The available non-color attributes are bold, underline, invisible,
right-line, left-line, protect, reverse and blink. 256 color pairs (8
bits), 8 bits for other attributes, and 16 bits for character data.

   Note that there is now a "super-long" 64-bit form,  available by
defining CHTYPE_LONG to be 2:

-------------------------------------------------------------------------------
|63|62|61|60|59|..|34|33|32|31|30|29|28|..|22|21|20|19|18|17|16|..| 3| 2| 1| 0|
-------------------------------------------------------------------------------
         color number   |        modifiers      |         character eg 'a'


   We take five more bits for the character (thus allowing Unicode values
past 64K;  UTF-16 can go up to 0x10ffff,  requiring 21 bits total),  and
four more bits for attributes.  Three are currently used as A_OVERLINE, A_DIM,
and A_STRIKEOUT;  one more is reserved for future use.  31 bits are then used
for color.  These are usually just treated as the usual palette
indices,  and range from 0 to 255.   However,  if bit 63 is
set,  the remaining 30 bits are interpreted as foreground RGB (first
fifteen bits,  five bits for each of the three channels) and background RGB
(same scheme using the remaining 15 bits.)

**man-end****************************************************************/

/*** Video attribute macros ***/

#define A_NORMAL      (chtype)0

#ifdef CHTYPE_LONG

# if(CHTYPE_LONG >= 2)     /* 64-bit chtypes */
    # define PDC_CHARTEXT_BITS   21
    # define A_CHARTEXT   (chtype)( ((chtype)0x1 << PDC_CHARTEXT_BITS) - 1)
    # define A_ALTCHARSET ((chtype)0x001 << PDC_CHARTEXT_BITS)
    # define A_RIGHTLINE  ((chtype)0x002 << PDC_CHARTEXT_BITS)
    # define A_LEFTLINE   ((chtype)0x004 << PDC_CHARTEXT_BITS)
    # define A_INVIS      ((chtype)0x008 << PDC_CHARTEXT_BITS)
    # define A_UNDERLINE  ((chtype)0x010 << PDC_CHARTEXT_BITS)
    # define A_REVERSE    ((chtype)0x020 << PDC_CHARTEXT_BITS)
    # define A_BLINK      ((chtype)0x040 << PDC_CHARTEXT_BITS)
    # define A_BOLD       ((chtype)0x080 << PDC_CHARTEXT_BITS)
    # define A_OVERLINE   ((chtype)0x100 << PDC_CHARTEXT_BITS)
    # define A_STRIKEOUT  ((chtype)0x200 << PDC_CHARTEXT_BITS)
    # define A_DIM        ((chtype)0x400 << PDC_CHARTEXT_BITS)
#if 0
                  /* May come up with a use for this bit    */
                  /* someday; reserved for the future: */
    # define A_FUTURE_2   ((chtype)0x800 << PDC_CHARTEXT_BITS)
#endif
    # define PDC_COLOR_SHIFT (PDC_CHARTEXT_BITS + 12)
    # define A_COLOR      ((chtype)0x7fffffff << PDC_COLOR_SHIFT)
    # define A_RGB_COLOR  ((chtype)0x40000000 << PDC_COLOR_SHIFT)
    # define A_ATTRIBUTES (((chtype)0xfff << PDC_CHARTEXT_BITS) | A_COLOR)
    # define A_RGB( rfore, gfore, bfore, rback, gback, bback)       \
        (( (((chtype)(bfore) << 25) \
         | ((chtype)(gfore) << 20) \
         | ((chtype)(rfore) << 15) \
         | ((chtype)(bback) << 10) \
         | ((chtype)(gback) <<  5) \
         | ((chtype)(rback)      )) << PDC_COLOR_SHIFT) | A_RGB_COLOR)
# else         /* plain ol' 32-bit chtypes */
    # define A_ALTCHARSET (chtype)0x00010000
    # define A_RIGHTLINE  (chtype)0x00020000
    # define A_LEFTLINE   (chtype)0x00040000
    # define A_INVIS      (chtype)0x00080000
    # define A_UNDERLINE  (chtype)0x00100000
    # define A_REVERSE    (chtype)0x00200000
    # define A_BLINK      (chtype)0x00400000
    # define A_BOLD       (chtype)0x00800000
    # define A_COLOR      (chtype)0xff000000
    # define A_RGB_COLOR  A_NORMAL
#ifdef PDC_WIDE
    # define A_CHARTEXT   (chtype)0x0000ffff
    # define A_ATTRIBUTES (chtype)0xffff0000
    # define A_DIM        A_NORMAL
    # define A_OVERLINE   A_NORMAL
    # define A_STRIKEOUT  A_NORMAL
#else          /* with 8-bit chars,  we have bits for these attribs : */
    # define A_CHARTEXT   (chtype)0x000000ff
    # define A_ATTRIBUTES (chtype)0xffffe000
    # define A_DIM        (chtype)0x00008000
    # define A_OVERLINE   (chtype)0x00004000
    # define A_STRIKEOUT  (chtype)0x00002000
#endif
    # define PDC_COLOR_SHIFT 24
#endif


# define A_ITALIC     A_INVIS
# define A_PROTECT    (A_UNDERLINE | A_LEFTLINE | A_RIGHTLINE)

#else                   /* 16-bit chtypes */
# define A_BOLD       (chtype)0x0100  /* X/Open */
# define A_REVERSE    (chtype)0x0200  /* X/Open */
# define A_BLINK      (chtype)0x0400  /* X/Open */

# define A_ATTRIBUTES (chtype)0xff00  /* X/Open */
# define A_CHARTEXT   (chtype)0x00ff  /* X/Open */
# define A_COLOR      (chtype)0xf800  /* System V */

# define A_ALTCHARSET A_NORMAL        /* X/Open */
# define A_PROTECT    A_NORMAL        /* X/Open */
# define A_UNDERLINE  A_NORMAL        /* X/Open */
# define A_OVERLINE   A_NORMAL        /* X/Open */
# define A_STRIKEOUT  A_NORMAL        /* X/Open */

# define A_LEFTLINE   A_NORMAL
# define A_RIGHTLINE  A_NORMAL
# define A_ITALIC     A_NORMAL
# define A_INVIS      A_NORMAL
# define A_RGB_COLOR  A_NORMAL
# define A_DIM        A_NORMAL

# define PDC_COLOR_SHIFT 11
#endif

#define A_STANDOUT    (A_REVERSE | A_BOLD) /* X/Open */

#define CHR_MSK       A_CHARTEXT           /* Obsolete */
#define ATR_MSK       A_ATTRIBUTES         /* Obsolete */
#define ATR_NRM       A_NORMAL             /* Obsolete */

/* For use with attr_t -- X/Open says, "these shall be distinct", so
   this is a non-conforming implementation. */

#define WA_NORMAL     A_NORMAL

#define WA_ALTCHARSET A_ALTCHARSET
#define WA_BLINK      A_BLINK
#define WA_BOLD       A_BOLD
#define WA_DIM        A_DIM
#define WA_INVIS      A_INVIS
#define WA_LEFT       A_LEFTLINE
#define WA_PROTECT    A_PROTECT
#define WA_REVERSE    A_REVERSE
#define WA_RIGHT      A_RIGHTLINE
#define WA_STANDOUT   A_STANDOUT
#define WA_UNDERLINE  A_UNDERLINE

#define WA_HORIZONTAL A_NORMAL
#define WA_LOW        A_NORMAL
#define WA_TOP        A_NORMAL
#define WA_VERTICAL   A_NORMAL

#define WA_ATTRIBUTES A_ATTRIBUTES

/*** Alternate character set macros ***/

/* 'w' = 32-bit chtype; acs_map[] index | A_ALTCHARSET
   'n' = 16-bit chtype; it gets the fallback set because no bit is
         available for A_ALTCHARSET */

#ifdef CHTYPE_LONG
# define ACS_PICK(w, n) ((chtype)w | A_ALTCHARSET)
#else
# define ACS_PICK(w, n) ((chtype)n)
#endif

/* VT100-compatible symbols -- box chars */

#define ACS_LRCORNER      ACS_PICK('V', '+')
#define ACS_URCORNER      ACS_PICK('W', '+')
#define ACS_ULCORNER      ACS_PICK('X', '+')
#define ACS_LLCORNER      ACS_PICK('Y', '+')
#define ACS_PLUS          ACS_PICK('Z', '+')
#define ACS_LTEE          ACS_PICK('[', '+')
#define ACS_RTEE          ACS_PICK('\\', '+')
#define ACS_BTEE          ACS_PICK(']', '+')
#define ACS_TTEE          ACS_PICK('^', '+')
#define ACS_HLINE         ACS_PICK('_', '-')
#define ACS_VLINE         ACS_PICK('`', '|')

/* PDCurses-only ACS chars.  Don't use if ncurses compatibility matters.
Some won't work in non-wide X11 builds (see 'acs_defs.h' for details). */

#define ACS_CENT          ACS_PICK('{', 'c')
#define ACS_YEN           ACS_PICK('|', 'y')
#define ACS_PESETA        ACS_PICK('}', 'p')
#define ACS_HALF          ACS_PICK('&', '/')
#define ACS_QUARTER       ACS_PICK('\'', '/')
#define ACS_LEFT_ANG_QU   ACS_PICK(')',  '<')
#define ACS_RIGHT_ANG_QU  ACS_PICK('*',  '>')
#define ACS_D_HLINE       ACS_PICK('a', '-')
#define ACS_D_VLINE       ACS_PICK('b', '|')
#define ACS_CLUB          ACS_PICK( 11, 'C')
#define ACS_HEART         ACS_PICK( 12, 'H')
#define ACS_SPADE         ACS_PICK( 13, 'S')
#define ACS_SMILE         ACS_PICK( 14, 'O')
#define ACS_REV_SMILE     ACS_PICK( 15, 'O')
#define ACS_MED_BULLET    ACS_PICK( 16, '.')
#define ACS_WHITE_BULLET  ACS_PICK( 17, 'O')
#define ACS_PILCROW       ACS_PICK( 18, 'O')
#define ACS_SECTION       ACS_PICK( 19, 'O')

#define ACS_SUP2          ACS_PICK(',', '2')
#define ACS_ALPHA         ACS_PICK('.', 'a')
#define ACS_BETA          ACS_PICK('/', 'b')
#define ACS_GAMMA         ACS_PICK('0', 'y')
#define ACS_UP_SIGMA      ACS_PICK('1', 'S')
#define ACS_LO_SIGMA      ACS_PICK('2', 's')
#define ACS_MU            ACS_PICK('4', 'u')
#define ACS_TAU           ACS_PICK('5', 't')
#define ACS_UP_PHI        ACS_PICK('6', 'F')
#define ACS_THETA         ACS_PICK('7', 't')
#define ACS_OMEGA         ACS_PICK('8', 'w')
#define ACS_DELTA         ACS_PICK('9', 'd')
#define ACS_INFINITY      ACS_PICK('-', 'i')
#define ACS_LO_PHI        ACS_PICK( 22, 'f')
#define ACS_EPSILON       ACS_PICK(':', 'e')
#define ACS_INTERSECT     ACS_PICK('e', 'u')
#define ACS_TRIPLE_BAR    ACS_PICK('f', '=')
#define ACS_DIVISION      ACS_PICK('c', '/')
#define ACS_APPROX_EQ     ACS_PICK('d', '~')
#define ACS_SM_BULLET     ACS_PICK('g', '.')
#define ACS_SQUARE_ROOT   ACS_PICK('i', '!')
#define ACS_UBLOCK        ACS_PICK('p', '^')
#define ACS_BBLOCK        ACS_PICK('q', '_')
#define ACS_LBLOCK        ACS_PICK('r', '<')
#define ACS_RBLOCK        ACS_PICK('s', '>')

#define ACS_A_ORDINAL     ACS_PICK(20,  'a')
#define ACS_O_ORDINAL     ACS_PICK(21,  'o')
#define ACS_INV_QUERY     ACS_PICK(24,  '?')
#define ACS_REV_NOT       ACS_PICK(25,  '!')
#define ACS_NOT           ACS_PICK(26,  '!')
#define ACS_INV_BANG      ACS_PICK(23,  '!')
#define ACS_UP_INTEGRAL   ACS_PICK(27,  '|')
#define ACS_LO_INTEGRAL   ACS_PICK(28,  '|')
#define ACS_SUP_N         ACS_PICK(29,  'n')
#define ACS_CENTER_SQU    ACS_PICK(30,  'x')
#define ACS_F_WITH_HOOK   ACS_PICK(31,  'f')

#define ACS_SD_LRCORNER   ACS_PICK(';', '+')
#define ACS_SD_URCORNER   ACS_PICK('<', '+')
#define ACS_SD_ULCORNER   ACS_PICK('=', '+')
#define ACS_SD_LLCORNER   ACS_PICK('>', '+')
#define ACS_SD_PLUS       ACS_PICK('?', '+')
#define ACS_SD_LTEE       ACS_PICK('@', '+')
#define ACS_SD_RTEE       ACS_PICK('A', '+')
#define ACS_SD_BTEE       ACS_PICK('B', '+')
#define ACS_SD_TTEE       ACS_PICK('C', '+')

#define ACS_D_LRCORNER    ACS_PICK('D', '+')
#define ACS_D_URCORNER    ACS_PICK('E', '+')
#define ACS_D_ULCORNER    ACS_PICK('F', '+')
#define ACS_D_LLCORNER    ACS_PICK('G', '+')
#define ACS_D_PLUS        ACS_PICK('H', '+')
#define ACS_D_LTEE        ACS_PICK('I', '+')
#define ACS_D_RTEE        ACS_PICK('J', '+')
#define ACS_D_BTEE        ACS_PICK('K', '+')
#define ACS_D_TTEE        ACS_PICK('L', '+')

#define ACS_DS_LRCORNER   ACS_PICK('M', '+')
#define ACS_DS_URCORNER   ACS_PICK('N', '+')
#define ACS_DS_ULCORNER   ACS_PICK('O', '+')
#define ACS_DS_LLCORNER   ACS_PICK('P', '+')
#define ACS_DS_PLUS       ACS_PICK('Q', '+')
#define ACS_DS_LTEE       ACS_PICK('R', '+')
#define ACS_DS_RTEE       ACS_PICK('S', '+')
#define ACS_DS_BTEE       ACS_PICK('T', '+')
#define ACS_DS_TTEE       ACS_PICK('U', '+')

/* VT100-compatible symbols -- other */

#define ACS_S1            ACS_PICK('l', '-')
#define ACS_S9            ACS_PICK('o', '_')
#define ACS_DIAMOND       ACS_PICK('j', '+')
#define ACS_CKBOARD       ACS_PICK('k', ':')
#define ACS_DEGREE        ACS_PICK('w', '\'')
#define ACS_PLMINUS       ACS_PICK('x', '#')
#define ACS_BULLET        ACS_PICK('h', 'o')

/* Teletype 5410v1 symbols -- these are defined in SysV curses, but
   are not well-supported by most terminals. Stick to VT100 characters
   for optimum portability. */

#define ACS_LARROW        ACS_PICK('!', '<')
#define ACS_RARROW        ACS_PICK(' ', '>')
#define ACS_DARROW        ACS_PICK('#', 'v')
#define ACS_UARROW        ACS_PICK('"', '^')
#define ACS_BOARD         ACS_PICK('+', '#')
#define ACS_LTBOARD       ACS_PICK('y', '#')
#define ACS_LANTERN       ACS_PICK('z', '*')
#define ACS_BLOCK         ACS_PICK('t', '#')

/* That goes double for these -- undocumented SysV symbols. Don't use
   them. */

#define ACS_S3            ACS_PICK('m', '-')
#define ACS_S7            ACS_PICK('n', '-')
#define ACS_LEQUAL        ACS_PICK('u', '<')
#define ACS_GEQUAL        ACS_PICK('v', '>')
#define ACS_PI            ACS_PICK('$', 'n')
#define ACS_NEQUAL        ACS_PICK('%', '+')
#define ACS_STERLING      ACS_PICK('~', 'L')

/* Box char aliases */

#define ACS_BSSB      ACS_ULCORNER
#define ACS_SSBB      ACS_LLCORNER
#define ACS_BBSS      ACS_URCORNER
#define ACS_SBBS      ACS_LRCORNER
#define ACS_SBSS      ACS_RTEE
#define ACS_SSSB      ACS_LTEE
#define ACS_SSBS      ACS_BTEE
#define ACS_BSSS      ACS_TTEE
#define ACS_BSBS      ACS_HLINE
#define ACS_SBSB      ACS_VLINE
#define ACS_SSSS      ACS_PLUS

/* cchar_t aliases */

#ifdef PDC_WIDE
# define WACS_LRCORNER      (&(acs_map['V']))
# define WACS_URCORNER      (&(acs_map['W']))
# define WACS_ULCORNER      (&(acs_map['X']))
# define WACS_LLCORNER      (&(acs_map['Y']))
# define WACS_PLUS          (&(acs_map['Z']))
# define WACS_LTEE          (&(acs_map['[']))
# define WACS_RTEE          (&(acs_map['\\']))
# define WACS_BTEE          (&(acs_map[']']))
# define WACS_TTEE          (&(acs_map['^']))
# define WACS_HLINE         (&(acs_map['_']))
# define WACS_VLINE         (&(acs_map['`']))

# define WACS_CENT          (&(acs_map['{']))
# define WACS_YEN           (&(acs_map['|']))
# define WACS_PESETA        (&(acs_map['}']))
# define WACS_HALF          (&(acs_map['&']))
# define WACS_QUARTER       (&(acs_map['\'']))
# define WACS_LEFT_ANG_QU   (&(acs_map[')']))
# define WACS_RIGHT_ANG_QU  (&(acs_map['*']))
# define WACS_D_HLINE       (&(acs_map['a']))
# define WACS_D_VLINE       (&(acs_map['b']))
# define WACS_CLUB          (&(acs_map[ 11]))
# define WACS_HEART         (&(acs_map[ 12]))
# define WACS_SPADE         (&(acs_map[ 13]))
# define WACS_SMILE         (&(acs_map[ 14]))
# define WACS_REV_SMILE     (&(acs_map[ 15]))
# define WACS_MED_BULLET    (&(acs_map[ 16]))
# define WACS_WHITE_BULLET  (&(acs_map[ 17]))
# define WACS_PILCROW       (&(acs_map[ 18]))
# define WACS_SECTION       (&(acs_map[ 19]))

# define WACS_SUP2          (&(acs_map[',']))
# define WACS_ALPHA         (&(acs_map['.']))
# define WACS_BETA          (&(acs_map['/']))
# define WACS_GAMMA         (&(acs_map['0']))
# define WACS_UP_SIGMA      (&(acs_map['1']))
# define WACS_LO_SIGMA      (&(acs_map['2']))
# define WACS_MU            (&(acs_map['4']))
# define WACS_TAU           (&(acs_map['5']))
# define WACS_UP_PHI        (&(acs_map['6']))
# define WACS_THETA         (&(acs_map['7']))
# define WACS_OMEGA         (&(acs_map['8']))
# define WACS_DELTA         (&(acs_map['9']))
# define WACS_INFINITY      (&(acs_map['-']))
# define WACS_LO_PHI        (&(acs_map[ 22]))
# define WACS_EPSILON       (&(acs_map[':']))
# define WACS_INTERSECT     (&(acs_map['e']))
# define WACS_TRIPLE_BAR    (&(acs_map['f']))
# define WACS_DIVISION      (&(acs_map['c']))
# define WACS_APPROX_EQ     (&(acs_map['d']))
# define WACS_SM_BULLET     (&(acs_map['g']))
# define WACS_SQUARE_ROOT   (&(acs_map['i']))
# define WACS_UBLOCK        (&(acs_map['p']))
# define WACS_BBLOCK        (&(acs_map['q']))
# define WACS_LBLOCK        (&(acs_map['r']))
# define WACS_RBLOCK        (&(acs_map['s']))

# define WACS_A_ORDINAL     (&(acs_map[20]))
# define WACS_O_ORDINAL     (&(acs_map[21]))
# define WACS_INV_QUERY     (&(acs_map[24]))
# define WACS_REV_NOT       (&(acs_map[25]))
# define WACS_NOT           (&(acs_map[26]))
# define WACS_INV_BANG      (&(acs_map[23]))
# define WACS_UP_INTEGRAL   (&(acs_map[27]))
# define WACS_LO_INTEGRAL   (&(acs_map[28]))
# define WACS_SUP_N         (&(acs_map[29]))
# define WACS_CENTER_SQU    (&(acs_map[30]))
# define WACS_F_WITH_HOOK   (&(acs_map[31]))

# define WACS_SD_LRCORNER   (&(acs_map[';']))
# define WACS_SD_URCORNER   (&(acs_map['<']))
# define WACS_SD_ULCORNER   (&(acs_map['=']))
# define WACS_SD_LLCORNER   (&(acs_map['>']))
# define WACS_SD_PLUS       (&(acs_map['?']))
# define WACS_SD_LTEE       (&(acs_map['@']))
# define WACS_SD_RTEE       (&(acs_map['A']))
# define WACS_SD_BTEE       (&(acs_map['B']))
# define WACS_SD_TTEE       (&(acs_map['C']))

# define WACS_D_LRCORNER    (&(acs_map['D']))
# define WACS_D_URCORNER    (&(acs_map['E']))
# define WACS_D_ULCORNER    (&(acs_map['F']))
# define WACS_D_LLCORNER    (&(acs_map['G']))
# define WACS_D_PLUS        (&(acs_map['H']))
# define WACS_D_LTEE        (&(acs_map['I']))
# define WACS_D_RTEE        (&(acs_map['J']))
# define WACS_D_BTEE        (&(acs_map['K']))
# define WACS_D_TTEE        (&(acs_map['L']))

# define WACS_DS_LRCORNER   (&(acs_map['M']))
# define WACS_DS_URCORNER   (&(acs_map['N']))
# define WACS_DS_ULCORNER   (&(acs_map['O']))
# define WACS_DS_LLCORNER   (&(acs_map['P']))
# define WACS_DS_PLUS       (&(acs_map['Q']))
# define WACS_DS_LTEE       (&(acs_map['R']))
# define WACS_DS_RTEE       (&(acs_map['S']))
# define WACS_DS_BTEE       (&(acs_map['T']))
# define WACS_DS_TTEE       (&(acs_map['U']))

# define WACS_S1            (&(acs_map['l']))
# define WACS_S9            (&(acs_map['o']))
# define WACS_DIAMOND       (&(acs_map['j']))
# define WACS_CKBOARD       (&(acs_map['k']))
# define WACS_DEGREE        (&(acs_map['w']))
# define WACS_PLMINUS       (&(acs_map['x']))
# define WACS_BULLET        (&(acs_map['h']))


# define WACS_LARROW        (&(acs_map['!']))
# define WACS_RARROW        (&(acs_map[' ']))
# define WACS_DARROW        (&(acs_map['#']))
# define WACS_UARROW        (&(acs_map['"']))
# define WACS_BOARD         (&(acs_map['+']))
# define WACS_LTBOARD       (&(acs_map['y']))
# define WACS_LANTERN       (&(acs_map['z']))
# define WACS_BLOCK         (&(acs_map['t']))

# define WACS_S3            (&(acs_map['m']))
# define WACS_S7            (&(acs_map['n']))
# define WACS_LEQUAL        (&(acs_map['u']))
# define WACS_GEQUAL        (&(acs_map['v']))
# define WACS_PI            (&(acs_map['$']))
# define WACS_NEQUAL        (&(acs_map['%']))
# define WACS_STERLING      (&(acs_map['~']))

# define WACS_BSSB     WACS_ULCORNER
# define WACS_SSBB     WACS_LLCORNER
# define WACS_BBSS     WACS_URCORNER
# define WACS_SBBS     WACS_LRCORNER
# define WACS_SBSS     WACS_RTEE
# define WACS_SSSB     WACS_LTEE
# define WACS_SSBS     WACS_BTEE
# define WACS_BSSS     WACS_TTEE
# define WACS_BSBS     WACS_HLINE
# define WACS_SBSB     WACS_VLINE
# define WACS_SSSS     WACS_PLUS
#endif

/*** Color macros ***/

#define COLOR_BLACK   0

#ifdef PDC_RGB        /* RGB */
# define COLOR_RED    1
# define COLOR_GREEN  2
# define COLOR_BLUE   4
#else                 /* BGR */
# define COLOR_BLUE   1
# define COLOR_GREEN  2
# define COLOR_RED    4
#endif

#define COLOR_CYAN    (COLOR_BLUE | COLOR_GREEN)
#define COLOR_MAGENTA (COLOR_RED | COLOR_BLUE)
#define COLOR_YELLOW  (COLOR_RED | COLOR_GREEN)

#define COLOR_WHITE   7

/*----------------------------------------------------------------------
 *
 *  Function and Keypad Key Definitions.
 *  Many are just for compatibility.
 *
 */

#ifdef PDC_WIDE
   #define KEY_OFFSET 0xec00
#else
   #define KEY_OFFSET 0x100
#endif

#define KEY_CODE_YES     (KEY_OFFSET + 0x00) /* If get_wch() gives a key code */

#define KEY_BREAK        (KEY_OFFSET + 0x01) /* Not on PC KBD */
#define KEY_DOWN         (KEY_OFFSET + 0x02) /* Down arrow key */
#define KEY_UP           (KEY_OFFSET + 0x03) /* Up arrow key */
#define KEY_LEFT         (KEY_OFFSET + 0x04) /* Left arrow key */
#define KEY_RIGHT        (KEY_OFFSET + 0x05) /* Right arrow key */
#define KEY_HOME         (KEY_OFFSET + 0x06) /* home key */
#define KEY_BACKSPACE    (KEY_OFFSET + 0x07) /* not on pc */
#define KEY_F0           (KEY_OFFSET + 0x08) /* function keys; 64 reserved */

#define KEY_DL           (KEY_OFFSET + 0x48) /* delete line */
#define KEY_IL           (KEY_OFFSET + 0x49) /* insert line */
#define KEY_DC           (KEY_OFFSET + 0x4a) /* delete character */
#define KEY_IC           (KEY_OFFSET + 0x4b) /* insert char or enter ins mode */
#define KEY_EIC          (KEY_OFFSET + 0x4c) /* exit insert char mode */
#define KEY_CLEAR        (KEY_OFFSET + 0x4d) /* clear screen */
#define KEY_EOS          (KEY_OFFSET + 0x4e) /* clear to end of screen */
#define KEY_EOL          (KEY_OFFSET + 0x4f) /* clear to end of line */
#define KEY_SF           (KEY_OFFSET + 0x50) /* scroll 1 line forward */
#define KEY_SR           (KEY_OFFSET + 0x51) /* scroll 1 line back (reverse) */
#define KEY_NPAGE        (KEY_OFFSET + 0x52) /* next page */
#define KEY_PPAGE        (KEY_OFFSET + 0x53) /* previous page */
#define KEY_STAB         (KEY_OFFSET + 0x54) /* set tab */
#define KEY_CTAB         (KEY_OFFSET + 0x55) /* clear tab */
#define KEY_CATAB        (KEY_OFFSET + 0x56) /* clear all tabs */
#define KEY_ENTER        (KEY_OFFSET + 0x57) /* enter or send (unreliable) */
#define KEY_SRESET       (KEY_OFFSET + 0x58) /* soft/reset (partial/unreliable) */
#define KEY_RESET        (KEY_OFFSET + 0x59) /* reset/hard reset (unreliable) */
#define KEY_PRINT        (KEY_OFFSET + 0x5a) /* print/copy */
#define KEY_LL           (KEY_OFFSET + 0x5b) /* home down/bottom (lower left) */
#define KEY_ABORT        (KEY_OFFSET + 0x5c) /* abort/terminate key (any) */
#define KEY_SHELP        (KEY_OFFSET + 0x5d) /* short help */
#define KEY_LHELP        (KEY_OFFSET + 0x5e) /* long help */
#define KEY_BTAB         (KEY_OFFSET + 0x5f) /* Back tab key */
#define KEY_BEG          (KEY_OFFSET + 0x60) /* beg(inning) key */
#define KEY_CANCEL       (KEY_OFFSET + 0x61) /* cancel key */
#define KEY_CLOSE        (KEY_OFFSET + 0x62) /* close key */
#define KEY_COMMAND      (KEY_OFFSET + 0x63) /* cmd (command) key */
#define KEY_COPY         (KEY_OFFSET + 0x64) /* copy key */
#define KEY_CREATE       (KEY_OFFSET + 0x65) /* create key */
#define KEY_END          (KEY_OFFSET + 0x66) /* end key */
#define KEY_EXIT         (KEY_OFFSET + 0x67) /* exit key */
#define KEY_FIND         (KEY_OFFSET + 0x68) /* find key */
#define KEY_HELP         (KEY_OFFSET + 0x69) /* help key */
#define KEY_MARK         (KEY_OFFSET + 0x6a) /* mark key */
#define KEY_MESSAGE      (KEY_OFFSET + 0x6b) /* message key */
#define KEY_MOVE         (KEY_OFFSET + 0x6c) /* move key */
#define KEY_NEXT         (KEY_OFFSET + 0x6d) /* next object key */
#define KEY_OPEN         (KEY_OFFSET + 0x6e) /* open key */
#define KEY_OPTIONS      (KEY_OFFSET + 0x6f) /* options key */
#define KEY_PREVIOUS     (KEY_OFFSET + 0x70) /* previous object key */
#define KEY_REDO         (KEY_OFFSET + 0x71) /* redo key */
#define KEY_REFERENCE    (KEY_OFFSET + 0x72) /* ref(erence) key */
#define KEY_REFRESH      (KEY_OFFSET + 0x73) /* refresh key */
#define KEY_REPLACE      (KEY_OFFSET + 0x74) /* replace key */
#define KEY_RESTART      (KEY_OFFSET + 0x75) /* restart key */
#define KEY_RESUME       (KEY_OFFSET + 0x76) /* resume key */
#define KEY_SAVE         (KEY_OFFSET + 0x77) /* save key */
#define KEY_SBEG         (KEY_OFFSET + 0x78) /* shifted beginning key */
#define KEY_SCANCEL      (KEY_OFFSET + 0x79) /* shifted cancel key */
#define KEY_SCOMMAND     (KEY_OFFSET + 0x7a) /* shifted command key */
#define KEY_SCOPY        (KEY_OFFSET + 0x7b) /* shifted copy key */
#define KEY_SCREATE      (KEY_OFFSET + 0x7c) /* shifted create key */
#define KEY_SDC          (KEY_OFFSET + 0x7d) /* shifted delete char key */
#define KEY_SDL          (KEY_OFFSET + 0x7e) /* shifted delete line key */
#define KEY_SELECT       (KEY_OFFSET + 0x7f) /* select key */
#define KEY_SEND         (KEY_OFFSET + 0x80) /* shifted end key */
#define KEY_SEOL         (KEY_OFFSET + 0x81) /* shifted clear line key */
#define KEY_SEXIT        (KEY_OFFSET + 0x82) /* shifted exit key */
#define KEY_SFIND        (KEY_OFFSET + 0x83) /* shifted find key */
#define KEY_SHOME        (KEY_OFFSET + 0x84) /* shifted home key */
#define KEY_SIC          (KEY_OFFSET + 0x85) /* shifted input key */

#define KEY_SLEFT        (KEY_OFFSET + 0x87) /* shifted left arrow key */
#define KEY_SMESSAGE     (KEY_OFFSET + 0x88) /* shifted message key */
#define KEY_SMOVE        (KEY_OFFSET + 0x89) /* shifted move key */
#define KEY_SNEXT        (KEY_OFFSET + 0x8a) /* shifted next key */
#define KEY_SOPTIONS     (KEY_OFFSET + 0x8b) /* shifted options key */
#define KEY_SPREVIOUS    (KEY_OFFSET + 0x8c) /* shifted prev key */
#define KEY_SPRINT       (KEY_OFFSET + 0x8d) /* shifted print key */
#define KEY_SREDO        (KEY_OFFSET + 0x8e) /* shifted redo key */
#define KEY_SREPLACE     (KEY_OFFSET + 0x8f) /* shifted replace key */
#define KEY_SRIGHT       (KEY_OFFSET + 0x90) /* shifted right arrow */
#define KEY_SRSUME       (KEY_OFFSET + 0x91) /* shifted resume key */
#define KEY_SSAVE        (KEY_OFFSET + 0x92) /* shifted save key */
#define KEY_SSUSPEND     (KEY_OFFSET + 0x93) /* shifted suspend key */
#define KEY_SUNDO        (KEY_OFFSET + 0x94) /* shifted undo key */
#define KEY_SUSPEND      (KEY_OFFSET + 0x95) /* suspend key */
#define KEY_UNDO         (KEY_OFFSET + 0x96) /* undo key */

/* PDCurses-specific key definitions -- PC only */

#define ALT_0                 (KEY_OFFSET + 0x97)
#define ALT_1                 (KEY_OFFSET + 0x98)
#define ALT_2                 (KEY_OFFSET + 0x99)
#define ALT_3                 (KEY_OFFSET + 0x9a)
#define ALT_4                 (KEY_OFFSET + 0x9b)
#define ALT_5                 (KEY_OFFSET + 0x9c)
#define ALT_6                 (KEY_OFFSET + 0x9d)
#define ALT_7                 (KEY_OFFSET + 0x9e)
#define ALT_8                 (KEY_OFFSET + 0x9f)
#define ALT_9                 (KEY_OFFSET + 0xa0)
#define ALT_A                 (KEY_OFFSET + 0xa1)
#define ALT_B                 (KEY_OFFSET + 0xa2)
#define ALT_C                 (KEY_OFFSET + 0xa3)
#define ALT_D                 (KEY_OFFSET + 0xa4)
#define ALT_E                 (KEY_OFFSET + 0xa5)
#define ALT_F                 (KEY_OFFSET + 0xa6)
#define ALT_G                 (KEY_OFFSET + 0xa7)
#define ALT_H                 (KEY_OFFSET + 0xa8)
#define ALT_I                 (KEY_OFFSET + 0xa9)
#define ALT_J                 (KEY_OFFSET + 0xaa)
#define ALT_K                 (KEY_OFFSET + 0xab)
#define ALT_L                 (KEY_OFFSET + 0xac)
#define ALT_M                 (KEY_OFFSET + 0xad)
#define ALT_N                 (KEY_OFFSET + 0xae)
#define ALT_O                 (KEY_OFFSET + 0xaf)
#define ALT_P                 (KEY_OFFSET + 0xb0)
#define ALT_Q                 (KEY_OFFSET + 0xb1)
#define ALT_R                 (KEY_OFFSET + 0xb2)
#define ALT_S                 (KEY_OFFSET + 0xb3)
#define ALT_T                 (KEY_OFFSET + 0xb4)
#define ALT_U                 (KEY_OFFSET + 0xb5)
#define ALT_V                 (KEY_OFFSET + 0xb6)
#define ALT_W                 (KEY_OFFSET + 0xb7)
#define ALT_X                 (KEY_OFFSET + 0xb8)
#define ALT_Y                 (KEY_OFFSET + 0xb9)
#define ALT_Z                 (KEY_OFFSET + 0xba)

#define CTL_LEFT              (KEY_OFFSET + 0xbb) /* Control-Left-Arrow */
#define CTL_RIGHT             (KEY_OFFSET + 0xbc)
#define CTL_PGUP              (KEY_OFFSET + 0xbd)
#define CTL_PGDN              (KEY_OFFSET + 0xbe)
#define CTL_HOME              (KEY_OFFSET + 0xbf)
#define CTL_END               (KEY_OFFSET + 0xc0)

#define KEY_A1                (KEY_OFFSET + 0xc1) /* upper left on Virtual keypad */
#define KEY_A2                (KEY_OFFSET + 0xc2) /* upper middle on Virt. keypad */
#define KEY_A3                (KEY_OFFSET + 0xc3) /* upper right on Vir. keypad */
#define KEY_B1                (KEY_OFFSET + 0xc4) /* middle left on Virt. keypad */
#define KEY_B2                (KEY_OFFSET + 0xc5) /* center on Virt. keypad */
#define KEY_B3                (KEY_OFFSET + 0xc6) /* middle right on Vir. keypad */
#define KEY_C1                (KEY_OFFSET + 0xc7) /* lower left on Virt. keypad */
#define KEY_C2                (KEY_OFFSET + 0xc8) /* lower middle on Virt. keypad */
#define KEY_C3                (KEY_OFFSET + 0xc9) /* lower right on Vir. keypad */

#define PADSLASH              (KEY_OFFSET + 0xca) /* slash on keypad */
#define PADENTER              (KEY_OFFSET + 0xcb) /* enter on keypad */
#define CTL_PADENTER          (KEY_OFFSET + 0xcc) /* ctl-enter on keypad */
#define ALT_PADENTER          (KEY_OFFSET + 0xcd) /* alt-enter on keypad */
#define PADSTOP               (KEY_OFFSET + 0xce) /* stop on keypad */
#define PADSTAR               (KEY_OFFSET + 0xcf) /* star on keypad */
#define PADMINUS              (KEY_OFFSET + 0xd0) /* minus on keypad */
#define PADPLUS               (KEY_OFFSET + 0xd1) /* plus on keypad */
#define CTL_PADSTOP           (KEY_OFFSET + 0xd2) /* ctl-stop on keypad */
#define CTL_PADCENTER         (KEY_OFFSET + 0xd3) /* ctl-enter on keypad */
#define CTL_PADPLUS           (KEY_OFFSET + 0xd4) /* ctl-plus on keypad */
#define CTL_PADMINUS          (KEY_OFFSET + 0xd5) /* ctl-minus on keypad */
#define CTL_PADSLASH          (KEY_OFFSET + 0xd6) /* ctl-slash on keypad */
#define CTL_PADSTAR           (KEY_OFFSET + 0xd7) /* ctl-star on keypad */
#define ALT_PADPLUS           (KEY_OFFSET + 0xd8) /* alt-plus on keypad */
#define ALT_PADMINUS          (KEY_OFFSET + 0xd9) /* alt-minus on keypad */
#define ALT_PADSLASH          (KEY_OFFSET + 0xda) /* alt-slash on keypad */
#define ALT_PADSTAR           (KEY_OFFSET + 0xdb) /* alt-star on keypad */
#define ALT_PADSTOP           (KEY_OFFSET + 0xdc) /* alt-stop on keypad */
#define CTL_INS               (KEY_OFFSET + 0xdd) /* ctl-insert */
#define ALT_DEL               (KEY_OFFSET + 0xde) /* alt-delete */
#define ALT_INS               (KEY_OFFSET + 0xdf) /* alt-insert */
#define CTL_UP                (KEY_OFFSET + 0xe0) /* ctl-up arrow */
#define CTL_DOWN              (KEY_OFFSET + 0xe1) /* ctl-down arrow */
#define CTL_TAB               (KEY_OFFSET + 0xe2) /* ctl-tab */
#define ALT_TAB               (KEY_OFFSET + 0xe3)
#define ALT_MINUS             (KEY_OFFSET + 0xe4)
#define ALT_EQUAL             (KEY_OFFSET + 0xe5)
#define ALT_HOME              (KEY_OFFSET + 0xe6)
#define ALT_PGUP              (KEY_OFFSET + 0xe7)
#define ALT_PGDN              (KEY_OFFSET + 0xe8)
#define ALT_END               (KEY_OFFSET + 0xe9)
#define ALT_UP                (KEY_OFFSET + 0xea) /* alt-up arrow */
#define ALT_DOWN              (KEY_OFFSET + 0xeb) /* alt-down arrow */
#define ALT_RIGHT             (KEY_OFFSET + 0xec) /* alt-right arrow */
#define ALT_LEFT              (KEY_OFFSET + 0xed) /* alt-left arrow */
#define ALT_ENTER             (KEY_OFFSET + 0xee) /* alt-enter */
#define ALT_ESC               (KEY_OFFSET + 0xef) /* alt-escape */
#define ALT_BQUOTE            (KEY_OFFSET + 0xf0) /* alt-back quote */
#define ALT_LBRACKET          (KEY_OFFSET + 0xf1) /* alt-left bracket */
#define ALT_RBRACKET          (KEY_OFFSET + 0xf2) /* alt-right bracket */
#define ALT_SEMICOLON         (KEY_OFFSET + 0xf3) /* alt-semi-colon */
#define ALT_FQUOTE            (KEY_OFFSET + 0xf4) /* alt-forward quote */
#define ALT_COMMA             (KEY_OFFSET + 0xf5) /* alt-comma */
#define ALT_STOP              (KEY_OFFSET + 0xf6) /* alt-stop */
#define ALT_FSLASH            (KEY_OFFSET + 0xf7) /* alt-forward slash */
#define ALT_BKSP              (KEY_OFFSET + 0xf8) /* alt-backspace */
#define CTL_BKSP              (KEY_OFFSET + 0xf9) /* ctl-backspace */
#define PAD0                  (KEY_OFFSET + 0xfa) /* keypad 0 */

#define CTL_PAD0              (KEY_OFFSET + 0xfb) /* ctl-keypad 0 */
#define CTL_PAD1              (KEY_OFFSET + 0xfc)
#define CTL_PAD2              (KEY_OFFSET + 0xfd)
#define CTL_PAD3              (KEY_OFFSET + 0xfe)
#define CTL_PAD4              (KEY_OFFSET + 0xff)
#define CTL_PAD5              (KEY_OFFSET + 0x100)
#define CTL_PAD6              (KEY_OFFSET + 0x101)
#define CTL_PAD7              (KEY_OFFSET + 0x102)
#define CTL_PAD8              (KEY_OFFSET + 0x103)
#define CTL_PAD9              (KEY_OFFSET + 0x104)

#define ALT_PAD0              (KEY_OFFSET + 0x105) /* alt-keypad 0 */
#define ALT_PAD1              (KEY_OFFSET + 0x106)
#define ALT_PAD2              (KEY_OFFSET + 0x107)
#define ALT_PAD3              (KEY_OFFSET + 0x108)
#define ALT_PAD4              (KEY_OFFSET + 0x109)
#define ALT_PAD5              (KEY_OFFSET + 0x10a)
#define ALT_PAD6              (KEY_OFFSET + 0x10b)
#define ALT_PAD7              (KEY_OFFSET + 0x10c)
#define ALT_PAD8              (KEY_OFFSET + 0x10d)
#define ALT_PAD9              (KEY_OFFSET + 0x10e)

#define CTL_DEL               (KEY_OFFSET + 0x10f) /* clt-delete */
#define ALT_BSLASH            (KEY_OFFSET + 0x110) /* alt-back slash */
#define CTL_ENTER             (KEY_OFFSET + 0x111) /* ctl-enter */

#define SHF_PADENTER          (KEY_OFFSET + 0x112) /* shift-enter on keypad */
#define SHF_PADSLASH          (KEY_OFFSET + 0x113) /* shift-slash on keypad */
#define SHF_PADSTAR           (KEY_OFFSET + 0x114) /* shift-star  on keypad */
#define SHF_PADPLUS           (KEY_OFFSET + 0x115) /* shift-plus  on keypad */
#define SHF_PADMINUS          (KEY_OFFSET + 0x116) /* shift-minus on keypad */
#define SHF_UP                (KEY_OFFSET + 0x117) /* shift-up on keypad */
#define SHF_DOWN              (KEY_OFFSET + 0x118) /* shift-down on keypad */
#define SHF_IC                (KEY_OFFSET + 0x119) /* shift-insert on keypad */
#define SHF_DC                (KEY_OFFSET + 0x11a) /* shift-delete on keypad */

#define KEY_MOUSE             (KEY_OFFSET + 0x11b) /* "mouse" key */
#define KEY_SHIFT_L           (KEY_OFFSET + 0x11c) /* Left-shift */
#define KEY_SHIFT_R           (KEY_OFFSET + 0x11d) /* Right-shift */
#define KEY_CONTROL_L         (KEY_OFFSET + 0x11e) /* Left-control */
#define KEY_CONTROL_R         (KEY_OFFSET + 0x11f) /* Right-control */
#define KEY_ALT_L             (KEY_OFFSET + 0x120) /* Left-alt */
#define KEY_ALT_R             (KEY_OFFSET + 0x121) /* Right-alt */
#define KEY_RESIZE            (KEY_OFFSET + 0x122) /* Window resize */
#define KEY_SUP               (KEY_OFFSET + 0x123) /* Shifted up arrow */
#define KEY_SDOWN             (KEY_OFFSET + 0x124) /* Shifted down arrow */

         /* The following were added 2011 Sep 14,  and are */
         /* not returned by most flavors of PDCurses:      */

#define CTL_SEMICOLON         (KEY_OFFSET + 0x125)
#define CTL_EQUAL             (KEY_OFFSET + 0x126)
#define CTL_COMMA             (KEY_OFFSET + 0x127)
#define CTL_MINUS             (KEY_OFFSET + 0x128)
#define CTL_STOP              (KEY_OFFSET + 0x129)
#define CTL_FSLASH            (KEY_OFFSET + 0x12a)
#define CTL_BQUOTE            (KEY_OFFSET + 0x12b)

#define KEY_APPS              (KEY_OFFSET + 0x12c)
#define KEY_SAPPS             (KEY_OFFSET + 0x12d)
#define CTL_APPS              (KEY_OFFSET + 0x12e)
#define ALT_APPS              (KEY_OFFSET + 0x12f)

#define KEY_PAUSE             (KEY_OFFSET + 0x130)
#define KEY_SPAUSE            (KEY_OFFSET + 0x131)
#define CTL_PAUSE             (KEY_OFFSET + 0x132)

#define KEY_PRINTSCREEN       (KEY_OFFSET + 0x133)
#define ALT_PRINTSCREEN       (KEY_OFFSET + 0x134)
#define KEY_SCROLLLOCK        (KEY_OFFSET + 0x135)
#define ALT_SCROLLLOCK        (KEY_OFFSET + 0x136)

#define CTL_0                 (KEY_OFFSET + 0x137)
#define CTL_1                 (KEY_OFFSET + 0x138)
#define CTL_2                 (KEY_OFFSET + 0x139)
#define CTL_3                 (KEY_OFFSET + 0x13a)
#define CTL_4                 (KEY_OFFSET + 0x13b)
#define CTL_5                 (KEY_OFFSET + 0x13c)
#define CTL_6                 (KEY_OFFSET + 0x13d)
#define CTL_7                 (KEY_OFFSET + 0x13e)
#define CTL_8                 (KEY_OFFSET + 0x13f)
#define CTL_9                 (KEY_OFFSET + 0x140)

#define KEY_BROWSER_BACK      (KEY_OFFSET + 0x141)
#define KEY_SBROWSER_BACK     (KEY_OFFSET + 0x142)
#define KEY_CBROWSER_BACK     (KEY_OFFSET + 0x143)
#define KEY_ABROWSER_BACK     (KEY_OFFSET + 0x144)
#define KEY_BROWSER_FWD       (KEY_OFFSET + 0x145)
#define KEY_SBROWSER_FWD      (KEY_OFFSET + 0x146)
#define KEY_CBROWSER_FWD      (KEY_OFFSET + 0x147)
#define KEY_ABROWSER_FWD      (KEY_OFFSET + 0x148)
#define KEY_BROWSER_REF       (KEY_OFFSET + 0x149)
#define KEY_SBROWSER_REF      (KEY_OFFSET + 0x14A)
#define KEY_CBROWSER_REF      (KEY_OFFSET + 0x14B)
#define KEY_ABROWSER_REF      (KEY_OFFSET + 0x14C)
#define KEY_BROWSER_STOP      (KEY_OFFSET + 0x14D)
#define KEY_SBROWSER_STOP     (KEY_OFFSET + 0x14E)
#define KEY_CBROWSER_STOP     (KEY_OFFSET + 0x14F)
#define KEY_ABROWSER_STOP     (KEY_OFFSET + 0x150)
#define KEY_SEARCH            (KEY_OFFSET + 0x151)
#define KEY_SSEARCH           (KEY_OFFSET + 0x152)
#define KEY_CSEARCH           (KEY_OFFSET + 0x153)
#define KEY_ASEARCH           (KEY_OFFSET + 0x154)
#define KEY_FAVORITES         (KEY_OFFSET + 0x155)
#define KEY_SFAVORITES        (KEY_OFFSET + 0x156)
#define KEY_CFAVORITES        (KEY_OFFSET + 0x157)
#define KEY_AFAVORITES        (KEY_OFFSET + 0x158)
#define KEY_BROWSER_HOME      (KEY_OFFSET + 0x159)
#define KEY_SBROWSER_HOME     (KEY_OFFSET + 0x15A)
#define KEY_CBROWSER_HOME     (KEY_OFFSET + 0x15B)
#define KEY_ABROWSER_HOME     (KEY_OFFSET + 0x15C)
#define KEY_VOLUME_MUTE       (KEY_OFFSET + 0x15D)
#define KEY_SVOLUME_MUTE      (KEY_OFFSET + 0x15E)
#define KEY_CVOLUME_MUTE      (KEY_OFFSET + 0x15F)
#define KEY_AVOLUME_MUTE      (KEY_OFFSET + 0x160)
#define KEY_VOLUME_DOWN       (KEY_OFFSET + 0x161)
#define KEY_SVOLUME_DOWN      (KEY_OFFSET + 0x162)
#define KEY_CVOLUME_DOWN      (KEY_OFFSET + 0x163)
#define KEY_AVOLUME_DOWN      (KEY_OFFSET + 0x164)
#define KEY_VOLUME_UP         (KEY_OFFSET + 0x165)
#define KEY_SVOLUME_UP        (KEY_OFFSET + 0x166)
#define KEY_CVOLUME_UP        (KEY_OFFSET + 0x167)
#define KEY_AVOLUME_UP        (KEY_OFFSET + 0x168)
#define KEY_NEXT_TRACK        (KEY_OFFSET + 0x169)
#define KEY_SNEXT_TRACK       (KEY_OFFSET + 0x16A)
#define KEY_CNEXT_TRACK       (KEY_OFFSET + 0x16B)
#define KEY_ANEXT_TRACK       (KEY_OFFSET + 0x16C)
#define KEY_PREV_TRACK        (KEY_OFFSET + 0x16D)
#define KEY_SPREV_TRACK       (KEY_OFFSET + 0x16E)
#define KEY_CPREV_TRACK       (KEY_OFFSET + 0x16F)
#define KEY_APREV_TRACK       (KEY_OFFSET + 0x170)
#define KEY_MEDIA_STOP        (KEY_OFFSET + 0x171)
#define KEY_SMEDIA_STOP       (KEY_OFFSET + 0x172)
#define KEY_CMEDIA_STOP       (KEY_OFFSET + 0x173)
#define KEY_AMEDIA_STOP       (KEY_OFFSET + 0x174)
#define KEY_PLAY_PAUSE        (KEY_OFFSET + 0x175)
#define KEY_SPLAY_PAUSE       (KEY_OFFSET + 0x176)
#define KEY_CPLAY_PAUSE       (KEY_OFFSET + 0x177)
#define KEY_APLAY_PAUSE       (KEY_OFFSET + 0x178)
#define KEY_LAUNCH_MAIL       (KEY_OFFSET + 0x179)
#define KEY_SLAUNCH_MAIL      (KEY_OFFSET + 0x17A)
#define KEY_CLAUNCH_MAIL      (KEY_OFFSET + 0x17B)
#define KEY_ALAUNCH_MAIL      (KEY_OFFSET + 0x17C)
#define KEY_MEDIA_SELECT      (KEY_OFFSET + 0x17D)
#define KEY_SMEDIA_SELECT     (KEY_OFFSET + 0x17E)
#define KEY_CMEDIA_SELECT     (KEY_OFFSET + 0x17F)
#define KEY_AMEDIA_SELECT     (KEY_OFFSET + 0x180)
#define KEY_LAUNCH_APP1       (KEY_OFFSET + 0x181)
#define KEY_SLAUNCH_APP1      (KEY_OFFSET + 0x182)
#define KEY_CLAUNCH_APP1      (KEY_OFFSET + 0x183)
#define KEY_ALAUNCH_APP1      (KEY_OFFSET + 0x184)
#define KEY_LAUNCH_APP2       (KEY_OFFSET + 0x185)
#define KEY_SLAUNCH_APP2      (KEY_OFFSET + 0x186)
#define KEY_CLAUNCH_APP2      (KEY_OFFSET + 0x187)
#define KEY_ALAUNCH_APP2      (KEY_OFFSET + 0x188)

#define KEY_MIN       KEY_BREAK         /* Minimum curses key value */
#define KEY_MAX       KEY_ALAUNCH_APP2  /* Maximum curses key */

#define KEY_F(n)      (KEY_F0 + (n))

/*----------------------------------------------------------------------
 *
 *  PDCurses Function Declarations
 *
 */

/* Standard */

int     addch(const chtype);
int     addchnstr(const chtype *, int);
int     addchstr(const chtype *);
int     addnstr(const char *, int);
int     addstr(const char *);
int     attroff(chtype);
int     attron(chtype);
int     attrset(chtype);
int     attr_get(attr_t *, short *, void *);
int     attr_off(attr_t, void *);
int     attr_on(attr_t, void *);
int     attr_set(attr_t, short, void *);
int     baudrate(void);
int     beep(void);
int     bkgd(chtype);
void    bkgdset(chtype);
int     border(chtype, chtype, chtype, chtype, chtype, chtype, chtype, chtype);
int     box(WINDOW *, chtype, chtype);
bool    can_change_color(void);
int     cbreak(void);
int     chgat(int, attr_t, short, const void *);
int     clearok(WINDOW *, bool);
int     clear(void);
int     clrtobot(void);
int     clrtoeol(void);
int     color_content(short, short *, short *, short *);
int     color_set(short, void *);
int     copywin(const WINDOW *, WINDOW *, int, int, int, int, int, int, int);
int     curs_set(int);
int     def_prog_mode(void);
int     def_shell_mode(void);
int     delay_output(int);
int     delch(void);
int     deleteln(void);
void    delscreen(SCREEN *);
int     delwin(WINDOW *);
WINDOW *derwin(WINDOW *, int, int, int, int);
int     doupdate(void);
WINDOW *dupwin(WINDOW *);
int     echochar(const chtype);
int     echo(void);
int     endwin(void);
char    erasechar(void);
int     erase(void);
void    filter(void);
int     flash(void);
int     flushinp(void);
chtype  getbkgd(WINDOW *);
int     getnstr(char *, int);
int     getstr(char *);
WINDOW *getwin(FILE *);
int     halfdelay(int);
bool    has_colors(void);
bool    has_ic(void);
bool    has_il(void);
int     hline(chtype, int);
void    idcok(WINDOW *, bool);
int     idlok(WINDOW *, bool);
void    immedok(WINDOW *, bool);
int     inchnstr(chtype *, int);
int     inchstr(chtype *);
chtype  inch(void);
int     init_color(short, short, short, short);
int     init_pair(short, short, short);
WINDOW *initscr(void);
int     innstr(char *, int);
int     insch(chtype);
int     insdelln(int);
int     insertln(void);
int     insnstr(const char *, int);
int     insstr(const char *);
int     instr(char *);
int     intrflush(WINDOW *, bool);
bool    isendwin(void);
bool    is_linetouched(WINDOW *, int);
bool    is_wintouched(WINDOW *);
char   *keyname(int);
int     keypad(WINDOW *, bool);
char    killchar(void);
int     leaveok(WINDOW *, bool);
char   *longname(void);
int     meta(WINDOW *, bool);
int     move(int, int);
int     mvaddch(int, int, const chtype);
int     mvaddchnstr(int, int, const chtype *, int);
int     mvaddchstr(int, int, const chtype *);
int     mvaddnstr(int, int, const char *, int);
int     mvaddstr(int, int, const char *);
int     mvchgat(int, int, int, attr_t, short, const void *);
int     mvcur(int, int, int, int);
int     mvdelch(int, int);
int     mvderwin(WINDOW *, int, int);
int     mvgetch(int, int);
int     mvgetnstr(int, int, char *, int);
int     mvgetstr(int, int, char *);
int     mvhline(int, int, chtype, int);
chtype  mvinch(int, int);
int     mvinchnstr(int, int, chtype *, int);
int     mvinchstr(int, int, chtype *);
int     mvinnstr(int, int, char *, int);
int     mvinsch(int, int, chtype);
int     mvinsnstr(int, int, const char *, int);
int     mvinsstr(int, int, const char *);
int     mvinstr(int, int, char *);
int     mvprintw(int, int, const char *, ...);
int     mvscanw(int, int, const char *, ...);
int     mvvline(int, int, chtype, int);
int     mvwaddchnstr(WINDOW *, int, int, const chtype *, int);
int     mvwaddchstr(WINDOW *, int, int, const chtype *);
int     mvwaddch(WINDOW *, int, int, const chtype);
int     mvwaddnstr(WINDOW *, int, int, const char *, int);
int     mvwaddstr(WINDOW *, int, int, const char *);
int     mvwchgat(WINDOW *, int, int, int, attr_t, short, const void *);
int     mvwdelch(WINDOW *, int, int);
int     mvwgetch(WINDOW *, int, int);
int     mvwgetnstr(WINDOW *, int, int, char *, int);
int     mvwgetstr(WINDOW *, int, int, char *);
int     mvwhline(WINDOW *, int, int, chtype, int);
int     mvwinchnstr(WINDOW *, int, int, chtype *, int);
int     mvwinchstr(WINDOW *, int, int, chtype *);
chtype  mvwinch(WINDOW *, int, int);
int     mvwinnstr(WINDOW *, int, int, char *, int);
int     mvwinsch(WINDOW *, int, int, chtype);
int     mvwinsnstr(WINDOW *, int, int, const char *, int);
int     mvwinsstr(WINDOW *, int, int, const char *);
int     mvwinstr(WINDOW *, int, int, char *);
int     mvwin(WINDOW *, int, int);
int     mvwprintw(WINDOW *, int, int, const char *, ...);
int     mvwscanw(WINDOW *, int, int, const char *, ...);
int     mvwvline(WINDOW *, int, int, chtype, int);
int     napms(int);
WINDOW *newpad(int, int);
SCREEN *newterm(const char *, FILE *, FILE *);
WINDOW *newwin(int, int, int, int);
int     nl(void);
int     nocbreak(void);
int     nodelay(WINDOW *, bool);
int     noecho(void);
int     nonl(void);
void    noqiflush(void);
int     noraw(void);
int     notimeout(WINDOW *, bool);
int     overlay(const WINDOW *, WINDOW *);
int     overwrite(const WINDOW *, WINDOW *);
int     pair_content(short, short *, short *);
int     pechochar(WINDOW *, chtype);
int     pnoutrefresh(WINDOW *, int, int, int, int, int, int);
int     prefresh(WINDOW *, int, int, int, int, int, int);
int     printw(const char *, ...);
int     putwin(WINDOW *, FILE *);
void    qiflush(void);
int     raw(void);
int     redrawwin(WINDOW *);
int     refresh(void);
int     reset_prog_mode(void);
int     reset_shell_mode(void);
int     resetty(void);
int     ripoffline(int, int (*)(WINDOW *, int));
int     savetty(void);
int     scanw(const char *, ...);
int     scr_dump(const char *);
int     scr_init(const char *);
int     scr_restore(const char *);
int     scr_set(const char *);
int     scrl(int);
int     scroll(WINDOW *);
int     scrollok(WINDOW *, bool);
SCREEN *set_term(SCREEN *);
int     setscrreg(int, int);
int     slk_attroff(const chtype);
int     slk_attr_off(const attr_t, void *);
int     slk_attron(const chtype);
int     slk_attr_on(const attr_t, void *);
int     slk_attrset(const chtype);
int     slk_attr_set(const attr_t, short, void *);
int     slk_clear(void);
int     slk_color(short);
int     slk_init(int);
char   *slk_label(int);
int     slk_noutrefresh(void);
int     slk_refresh(void);
int     slk_restore(void);
int     slk_set(int, const char *, int);
int     slk_touch(void);
int     standend(void);
int     standout(void);
int     start_color(void);
WINDOW *subpad(WINDOW *, int, int, int, int);
WINDOW *subwin(WINDOW *, int, int, int, int);
int     syncok(WINDOW *, bool);
chtype  termattrs(void);
attr_t  term_attrs(void);
char   *termname(void);
void    timeout(int);
int     touchline(WINDOW *, int, int);
int     touchwin(WINDOW *);
int     typeahead(int);
int     untouchwin(WINDOW *);
void    use_env(bool);
int     vidattr(chtype);
int     vid_attr(attr_t, short, void *);
int     vidputs(chtype, int (*)(int));
int     vid_puts(attr_t, short, void *, int (*)(int));
int     vline(chtype, int);
int     vw_printw(WINDOW *, const char *, va_list);
int     vwprintw(WINDOW *, const char *, va_list);
int     vw_scanw(WINDOW *, const char *, va_list);
int     vwscanw(WINDOW *, const char *, va_list);
int     waddchnstr(WINDOW *, const chtype *, int);
int     waddchstr(WINDOW *, const chtype *);
int     waddch(WINDOW *, const chtype);
int     waddnstr(WINDOW *, const char *, int);
int     waddstr(WINDOW *, const char *);
int     wattroff(WINDOW *, chtype);
int     wattron(WINDOW *, chtype);
int     wattrset(WINDOW *, chtype);
int     wattr_get(WINDOW *, attr_t *, short *, void *);
int     wattr_off(WINDOW *, attr_t, void *);
int     wattr_on(WINDOW *, attr_t, void *);
int     wattr_set(WINDOW *, attr_t, short, void *);
void    wbkgdset(WINDOW *, chtype);
int     wbkgd(WINDOW *, chtype);
int     wborder(WINDOW *, chtype, chtype, chtype, chtype,
                chtype, chtype, chtype, chtype);
int     wchgat(WINDOW *, int, attr_t, short, const void *);
int     wclear(WINDOW *);
int     wclrtobot(WINDOW *);
int     wclrtoeol(WINDOW *);
int     wcolor_set(WINDOW *, short, void *);
void    wcursyncup(WINDOW *);
int     wdelch(WINDOW *);
int     wdeleteln(WINDOW *);
int     wechochar(WINDOW *, const chtype);
int     werase(WINDOW *);
int     wgetch(WINDOW *);
int     wgetnstr(WINDOW *, char *, int);
int     wgetstr(WINDOW *, char *);
int     whline(WINDOW *, chtype, int);
int     winchnstr(WINDOW *, chtype *, int);
int     winchstr(WINDOW *, chtype *);
chtype  winch(WINDOW *);
int     winnstr(WINDOW *, char *, int);
int     winsch(WINDOW *, chtype);
int     winsdelln(WINDOW *, int);
int     winsertln(WINDOW *);
int     winsnstr(WINDOW *, const char *, int);
int     winsstr(WINDOW *, const char *);
int     winstr(WINDOW *, char *);
int     wmove(WINDOW *, int, int);
int     wnoutrefresh(WINDOW *);
int     wprintw(WINDOW *, const char *, ...);
int     wredrawln(WINDOW *, int, int);
int     wrefresh(WINDOW *);
int     wscanw(WINDOW *, const char *, ...);
int     wscrl(WINDOW *, int);
int     wsetscrreg(WINDOW *, int, int);
int     wstandend(WINDOW *);
int     wstandout(WINDOW *);
void    wsyncdown(WINDOW *);
void    wsyncup(WINDOW *);
void    wtimeout(WINDOW *, int);
int     wtouchln(WINDOW *, int, int, int);
int     wvline(WINDOW *, chtype, int);

/* Wide-character functions */

#ifdef PDC_WIDE
int     addnwstr(const wchar_t *, int);
int     addwstr(const wchar_t *);
int     add_wch(const cchar_t *);
int     add_wchnstr(const cchar_t *, int);
int     add_wchstr(const cchar_t *);
int     border_set(const cchar_t *, const cchar_t *, const cchar_t *,
                   const cchar_t *, const cchar_t *, const cchar_t *,
                   const cchar_t *, const cchar_t *);
int     box_set(WINDOW *, const cchar_t *, const cchar_t *);
int     echo_wchar(const cchar_t *);
int     erasewchar(wchar_t *);
int     getbkgrnd(cchar_t *);
int     getcchar(const cchar_t *, wchar_t *, attr_t *, short *, void *);
int     getn_wstr(wint_t *, int);
int     get_wch(wint_t *);
int     get_wstr(wint_t *);
int     hline_set(const cchar_t *, int);
int     innwstr(wchar_t *, int);
int     ins_nwstr(const wchar_t *, int);
int     ins_wch(const cchar_t *);
int     ins_wstr(const wchar_t *);
int     inwstr(wchar_t *);
int     in_wch(cchar_t *);
int     in_wchnstr(cchar_t *, int);
int     in_wchstr(cchar_t *);
char   *key_name(wchar_t);
int     killwchar(wchar_t *);
int     mvaddnwstr(int, int, const wchar_t *, int);
int     mvaddwstr(int, int, const wchar_t *);
int     mvadd_wch(int, int, const cchar_t *);
int     mvadd_wchnstr(int, int, const cchar_t *, int);
int     mvadd_wchstr(int, int, const cchar_t *);
int     mvgetn_wstr(int, int, wint_t *, int);
int     mvget_wch(int, int, wint_t *);
int     mvget_wstr(int, int, wint_t *);
int     mvhline_set(int, int, const cchar_t *, int);
int     mvinnwstr(int, int, wchar_t *, int);
int     mvins_nwstr(int, int, const wchar_t *, int);
int     mvins_wch(int, int, const cchar_t *);
int     mvins_wstr(int, int, const wchar_t *);
int     mvinwstr(int, int, wchar_t *);
int     mvin_wch(int, int, cchar_t *);
int     mvin_wchnstr(int, int, cchar_t *, int);
int     mvin_wchstr(int, int, cchar_t *);
int     mvvline_set(int, int, const cchar_t *, int);
int     mvwaddnwstr(WINDOW *, int, int, const wchar_t *, int);
int     mvwaddwstr(WINDOW *, int, int, const wchar_t *);
int     mvwadd_wch(WINDOW *, int, int, const cchar_t *);
int     mvwadd_wchnstr(WINDOW *, int, int, const cchar_t *, int);
int     mvwadd_wchstr(WINDOW *, int, int, const cchar_t *);
int     mvwgetn_wstr(WINDOW *, int, int, wint_t *, int);
int     mvwget_wch(WINDOW *, int, int, wint_t *);
int     mvwget_wstr(WINDOW *, int, int, wint_t *);
int     mvwhline_set(WINDOW *, int, int, const cchar_t *, int);
int     mvwinnwstr(WINDOW *, int, int, wchar_t *, int);
int     mvwins_nwstr(WINDOW *, int, int, const wchar_t *, int);
int     mvwins_wch(WINDOW *, int, int, const cchar_t *);
int     mvwins_wstr(WINDOW *, int, int, const wchar_t *);
int     mvwin_wch(WINDOW *, int, int, cchar_t *);
int     mvwin_wchnstr(WINDOW *, int, int, cchar_t *, int);
int     mvwin_wchstr(WINDOW *, int, int, cchar_t *);
int     mvwinwstr(WINDOW *, int, int, wchar_t *);
int     mvwvline_set(WINDOW *, int, int, const cchar_t *, int);
int     pecho_wchar(WINDOW *, const cchar_t*);
int     setcchar(cchar_t*, const wchar_t*, const attr_t, short, const void*);
int     slk_wset(int, const wchar_t *, int);
int     unget_wch(const wchar_t);
int     vline_set(const cchar_t *, int);
int     waddnwstr(WINDOW *, const wchar_t *, int);
int     waddwstr(WINDOW *, const wchar_t *);
int     wadd_wch(WINDOW *, const cchar_t *);
int     wadd_wchnstr(WINDOW *, const cchar_t *, int);
int     wadd_wchstr(WINDOW *, const cchar_t *);
int     wbkgrnd(WINDOW *, const cchar_t *);
void    wbkgrndset(WINDOW *, const cchar_t *);
int     wborder_set(WINDOW *, const cchar_t *, const cchar_t *,
                    const cchar_t *, const cchar_t *, const cchar_t *,
                    const cchar_t *, const cchar_t *, const cchar_t *);
int     wecho_wchar(WINDOW *, const cchar_t *);
int     wgetbkgrnd(WINDOW *, cchar_t *);
int     wgetn_wstr(WINDOW *, wint_t *, int);
int     wget_wch(WINDOW *, wint_t *);
int     wget_wstr(WINDOW *, wint_t *);
int     whline_set(WINDOW *, const cchar_t *, int);
int     winnwstr(WINDOW *, wchar_t *, int);
int     wins_nwstr(WINDOW *, const wchar_t *, int);
int     wins_wch(WINDOW *, const cchar_t *);
int     wins_wstr(WINDOW *, const wchar_t *);
int     winwstr(WINDOW *, wchar_t *);
int     win_wch(WINDOW *, cchar_t *);
int     win_wchnstr(WINDOW *, cchar_t *, int);
int     win_wchstr(WINDOW *, cchar_t *);
wchar_t *wunctrl(cchar_t *);
int     wvline_set(WINDOW *, const cchar_t *, int);
#endif

/* Quasi-standard */

chtype  getattrs(WINDOW *);
int     getbegx(WINDOW *);
int     getbegy(WINDOW *);
int     getmaxx(WINDOW *);
int     getmaxy(WINDOW *);
int     getparx(WINDOW *);
int     getpary(WINDOW *);
int     getcurx(WINDOW *);
int     getcury(WINDOW *);
void    traceoff(void);
void    traceon(void);
char   *unctrl(chtype);

int     crmode(void);
int     nocrmode(void);
int     draino(int);
int     resetterm(void);
int     fixterm(void);
int     saveterm(void);
int     setsyx(int, int);

int     mouse_set(unsigned long);
int     mouse_on(unsigned long);
int     mouse_off(unsigned long);
int     request_mouse_pos(void);
int     map_button(unsigned long);
void    wmouse_position(WINDOW *, int *, int *);
unsigned long getmouse(void);
unsigned long getbmap(void);

/* ncurses */

int     assume_default_colors(int, int);
const char *curses_version(void);
bool    has_key(int);
int     use_default_colors(void);
int     wresize(WINDOW *, int, int);

int     mouseinterval(int);
mmask_t mousemask(mmask_t, mmask_t *);
bool    mouse_trafo(int *, int *, bool);
int     nc_getmouse(MEVENT *);
int     ungetmouse(MEVENT *);
bool    wenclose(const WINDOW *, int, int);
bool    wmouse_trafo(const WINDOW *, int *, int *, bool);

/* PDCurses */

int     addrawch(chtype);
int     insrawch(chtype);
bool    is_termresized(void);
int     mvaddrawch(int, int, chtype);
int     mvdeleteln(int, int);
int     mvinsertln(int, int);
int     mvinsrawch(int, int, chtype);
int     mvwaddrawch(WINDOW *, int, int, chtype);
int     mvwdeleteln(WINDOW *, int, int);
int     mvwinsertln(WINDOW *, int, int);
int     mvwinsrawch(WINDOW *, int, int, chtype);
int     raw_output(bool);
int     resize_term(int, int);
WINDOW *resize_window(WINDOW *, int, int);
int     waddrawch(WINDOW *, chtype);
int     winsrawch(WINDOW *, chtype);
char    wordchar(void);

#ifdef PDC_WIDE
wchar_t *slk_wlabel(int);
#endif

void    PDC_debug(const char *, ...);
int     PDC_ungetch(int);
int     PDC_set_blink(bool);
int     PDC_set_line_color(short);
void    PDC_set_title(const char *);

int     PDC_clearclipboard(void);
int     PDC_freeclipboard(char *);
int     PDC_getclipboard(char **, long *);
int     PDC_setclipboard(const char *, long);

unsigned long PDC_get_input_fd(void);
unsigned long PDC_get_key_modifiers(void);
int     PDC_return_key_modifiers(bool);
int     PDC_save_key_modifiers(bool);
void    PDC_set_resize_limits( const int new_min_lines,
                               const int new_max_lines,
                               const int new_min_cols,
                               const int new_max_cols);

#define FUNCTION_KEY_SHUT_DOWN        0
#define FUNCTION_KEY_PASTE            1
#define FUNCTION_KEY_ENLARGE_FONT     2
#define FUNCTION_KEY_SHRINK_FONT      3
#define FUNCTION_KEY_CHOOSE_FONT      4
#define FUNCTION_KEY_ABORT            5
#define PDC_MAX_FUNCTION_KEYS         6

int     PDC_set_function_key( const unsigned function,
                              const int new_key);

WINDOW *Xinitscr(int, char **);
#ifdef XCURSES
void    XCursesExit(void);
int     sb_init(void);
int     sb_set_horz(int, int, int);
int     sb_set_vert(int, int, int);
int     sb_get_horz(int *, int *, int *);
int     sb_get_vert(int *, int *, int *);
int     sb_refresh(void);
#endif

/*** Functions defined as macros ***/

/* getch() and ungetch() conflict with some DOS libraries */

#define getch()            wgetch(stdscr)
#define ungetch(ch)        PDC_ungetch(ch)

#define COLOR_PAIR(n)      (((chtype)(n) << PDC_COLOR_SHIFT) & A_COLOR)
#define PAIR_NUMBER(n)     ((((n) & A_COLOR) >> PDC_COLOR_SHIFT) & 0xff)

/* These will _only_ work as macros */

#define getbegyx(w, y, x)  (y = getbegy(w), x = getbegx(w))
#define getmaxyx(w, y, x)  (y = getmaxy(w), x = getmaxx(w))
#define getparyx(w, y, x)  (y = getpary(w), x = getparx(w))
#define getyx(w, y, x)     (y = getcury(w), x = getcurx(w))

#define getsyx(y, x)       { if (curscr->_leaveit) (y)=(x)=-1; \
                             else getyx(curscr,(y),(x)); }

#ifdef NCURSES_MOUSE_VERSION
# define getmouse(x) nc_getmouse(x)
#endif

/* return codes from PDC_getclipboard() and PDC_setclipboard() calls */

#define PDC_CLIP_SUCCESS         0
#define PDC_CLIP_ACCESS_ERROR    1
#define PDC_CLIP_EMPTY           2
#define PDC_CLIP_MEMORY_ERROR    3

/* PDCurses key modifier masks */

#define PDC_KEY_MODIFIER_SHIFT   1
#define PDC_KEY_MODIFIER_CONTROL 2
#define PDC_KEY_MODIFIER_ALT     4
#define PDC_KEY_MODIFIER_NUMLOCK 8

#if defined(__cplusplus) || defined(__cplusplus__) || defined(__CPLUSPLUS)
# undef bool
}
#endif

#endif  /* __PDCURSES__ */
