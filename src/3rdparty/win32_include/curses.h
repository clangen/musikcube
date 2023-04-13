/*----------------------------------------------------------------------*
 *                              PDCursesMod                             *
 *----------------------------------------------------------------------*/

#ifndef __PDCURSES__
#define __PDCURSES__ 1
#define __PDCURSESMOD__ 1

/*man-start**************************************************************

Define before inclusion (only those needed):

    XCURSES         if building / built for X11
    PDC_RGB         if you want to use RGB color definitions
                    (Red = 1, Green = 2, Blue = 4) instead of BGR
    PDC_WIDE        if building / built with wide-character support
    PDC_FORCE_UTF8  if forcing use of UTF8 (implies PDC_WIDE)
    PDC_DLL_BUILD   if building / built as a Windows DLL
    PDC_NCMOUSE     to use the ncurses mouse API instead
                    of PDCurses' traditional mouse API

Defined by this header:

    PDCURSES        PDCurses-only features are available
    PDC_BUILD       API build version
    PDC_VER_MAJOR   major version number
    PDC_VER_MINOR   minor version number
    PDC_VER_CHANGE  version change number
    PDC_VER_YEAR    year of version
    PDC_VER_MONTH   month of version
    PDC_VER_DAY     day of month of version
    PDC_VERDOT      version string

**man-end****************************************************************/

#define PDCURSES        1
#define PDC_BUILD (PDC_VER_MAJOR*1000 + PDC_VER_MINOR *100 + PDC_VER_CHANGE)
         /* NOTE : For version changes that are not backward compatible, */
         /* the 'endwin_*' #defines below should be updated.             */
#define PDC_VER_MAJOR    4
#define PDC_VER_MINOR    3
#define PDC_VER_CHANGE   6
#define PDC_VER_YEAR   2023
#define PDC_VER_MONTH    04
#define PDC_VER_DAY      12

#define PDC_STRINGIZE( x) #x
#define PDC_stringize( x) PDC_STRINGIZE( x)

#define PDC_VERDOT PDC_stringize( PDC_VER_MAJOR) "." \
                   PDC_stringize( PDC_VER_MINOR) "." \
                   PDC_stringize( PDC_VER_CHANGE)

#define PDC_VER_YMD PDC_stringize( PDC_VER_YEAR) "-" \
                    PDC_stringize( PDC_VER_MONTH) "-" \
                    PDC_stringize( PDC_VER_DAY)

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
# define PDC_99         1
#endif

#if defined(__cplusplus) && __cplusplus >= 199711L
# define PDC_PP98       1
#endif

/*----------------------------------------------------------------------*/

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>

#if defined( PDC_FORCE_UTF8) && !defined( PDC_WIDE)
   #define PDC_WIDE
#endif

#ifdef PDC_WIDE
# include <wchar.h>
#endif

#if defined(PDC_99) && !defined(__bool_true_false_are_defined)
# include <stdbool.h>
#endif

#ifdef __cplusplus
extern "C"
{
# ifndef PDC_PP98
#  define bool _bool
# endif
#endif

#ifdef NO_STDINT_H
   #define uint64_t unsigned __int64
   #define uint32_t unsigned long
   #define uint16_t unsigned short
   #define int32_t  long
   #define int16_t  short
#else
   #include <stdint.h>
   #ifdef __DMC__
      #define uint64_t unsigned long long
   #endif
#endif

/*----------------------------------------------------------------------
 *
 *  Constants and Types
 *
 */

#undef FALSE
#define FALSE 0

#undef TRUE
#define TRUE 1

#undef ERR
#define ERR (-1)

#undef OK
#define OK 0

#if !defined(PDC_PP98) && !defined(__bool_true_false_are_defined)
typedef unsigned char bool;
#endif

#if defined( CHTYPE_32)
   typedef uint32_t chtype;       /* chtypes will be 32 bits */
   typedef uint32_t mmask_t;
#else
   typedef uint64_t chtype;       /* chtypes will be 64 bits */
   typedef uint64_t mmask_t;
   #define PDC_LONG_MMASK
   #ifdef PDC_WIDE
      #define USING_COMBINING_CHARACTER_SCHEME
   #endif
#endif

#ifdef PDC_WIDE
typedef chtype cchar_t;
#endif

typedef chtype attr_t;

/*----------------------------------------------------------------------
 *
 *  Version Info
 *
 */

enum PDC_port
{
    PDC_PORT_X11 = 0,
    PDC_PORT_WINCON = 1,
    PDC_PORT_WINGUI = 2,
    PDC_PORT_DOS = 3,
    PDC_PORT_OS2 = 4,
    PDC_PORT_SDL1 = 5,
    PDC_PORT_SDL2 = 6,
    PDC_PORT_VT = 7,
    PDC_PORT_DOSVGA = 8,
    PDC_PORT_PLAN9 = 9,
    PDC_PORT_LINUX_FB = 10
};

/* Use this structure with PDC_get_version() for run-time info about the
   way the library was built, in case it doesn't match the header. */

typedef struct
{
    short flags;          /* flags OR'd together (see below) */
    short build;          /* PDC_BUILD at compile time */
    unsigned char major;  /* PDC_VER_MAJOR */
    unsigned char minor;  /* PDC_VER_MINOR */
    unsigned char change; /* PDC_VER_CHANGE */
    unsigned char csize;  /* sizeof chtype */
    unsigned char bsize;  /* sizeof bool */
    enum PDC_port port;
} PDC_VERSION;

enum
{
    PDC_VFLAG_DEBUG = 1,  /* set if built with -DPDCDEBUG */
    PDC_VFLAG_WIDE  = 2,  /* -DPDC_WIDE */
    PDC_VFLAG_UTF8  = 4,  /* -DPDC_FORCE_UTF8 */
    PDC_VFLAG_DLL   = 8,  /* -DPDC_DLL_BUILD */
    PDC_VFLAG_RGB   = 16  /* -DPDC_RGB */
};

/*----------------------------------------------------------------------
 *
 *  Mouse Interface -- SYSVR4, with extensions
 *
 */

#define PDC_MAX_MOUSE_BUTTONS          9

typedef struct
{
    int x;           /* absolute column, 0 based, measured in characters */
    int y;           /* absolute row, 0 based, measured in characters */
    short button[PDC_MAX_MOUSE_BUTTONS];         /* state of each button */
    int changes;     /* flags indicating what has changed with the mouse */
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
 * (Not actually used!)        10000 <- mouse position report  4
 *                            100000 <- mouse wheel up         5
 *                           1000000 <- mouse wheel down       6
 *                          10000000 <- mouse wheel left       7
 *                         100000000 <- mouse wheel right      8
 * (Buttons 4 and up are  1000000000 <- button 4 has changed   9
 * PDCursesMod-only,     10000000000 <- button 5 has changed  10
 * and only 4 & 5 are   100000000000 <- button 6 has changed  11
 * currently used)     1000000000000 <- button 7 has changed  12
 *                    10000000000000 <- button 8 has changed  13
 *                   100000000000000 <- button 9 has changed  14
 */

#define PDC_MOUSE_MOVED         0x0008
#define PDC_MOUSE_UNUSED_BIT    0x0010
#define PDC_MOUSE_WHEEL_UP      0x0020
#define PDC_MOUSE_WHEEL_DOWN    0x0040
#define PDC_MOUSE_WHEEL_LEFT    0x0080
#define PDC_MOUSE_WHEEL_RIGHT   0x0100

#define A_BUTTON_CHANGED        (Mouse_status.changes & 7)
#define MOUSE_MOVED             (Mouse_status.changes & PDC_MOUSE_MOVED)
#define BUTTON_CHANGED(x)       (Mouse_status.changes & (1 << ((x) - ((x)<4 ? 1 : -5))))
#define BUTTON_STATUS(x)        (Mouse_status.button[(x) - 1])
#define MOUSE_WHEEL_UP          (Mouse_status.changes & PDC_MOUSE_WHEEL_UP)
#define MOUSE_WHEEL_DOWN        (Mouse_status.changes & PDC_MOUSE_WHEEL_DOWN)
#define MOUSE_WHEEL_LEFT        (Mouse_status.changes & PDC_MOUSE_WHEEL_LEFT)
#define MOUSE_WHEEL_RIGHT       (Mouse_status.changes & PDC_MOUSE_WHEEL_RIGHT)

/* mouse bit-masks */

#define BUTTON1_RELEASED        (mmask_t)0x01
#define BUTTON1_PRESSED         (mmask_t)0x02
#define BUTTON1_CLICKED         (mmask_t)0x04
#define BUTTON1_DOUBLE_CLICKED  (mmask_t)0x08
#define BUTTON1_TRIPLE_CLICKED  (mmask_t)0x10

/* With the "traditional" 32-bit mmask_t,  mouse move and triple-clicks
share the same bit and can't be distinguished.  64-bit mmask_ts allow us
to make the distinction,  and will allow other events to be added later. */

#ifdef PDC_LONG_MMASK
   #define BUTTON1_MOVED           (mmask_t)0x20      /* PDCurses */
   #define PDC_BITS_PER_BUTTON     6
#else
   #define BUTTON1_MOVED           (mmask_t)0x10      /* PDCurses */
   #define PDC_BITS_PER_BUTTON     5
#endif

#define PDC_SHIFTED_BUTTON( button, n)  ((mmask_t)(button) << (((n) - 1) * PDC_BITS_PER_BUTTON))

#define BUTTON2_RELEASED       PDC_SHIFTED_BUTTON( BUTTON1_RELEASED,       2)
#define BUTTON2_PRESSED        PDC_SHIFTED_BUTTON( BUTTON1_PRESSED,        2)
#define BUTTON2_CLICKED        PDC_SHIFTED_BUTTON( BUTTON1_CLICKED,        2)
#define BUTTON2_DOUBLE_CLICKED PDC_SHIFTED_BUTTON( BUTTON1_DOUBLE_CLICKED, 2)
#define BUTTON2_TRIPLE_CLICKED PDC_SHIFTED_BUTTON( BUTTON1_TRIPLE_CLICKED, 2)
#define BUTTON2_MOVED          PDC_SHIFTED_BUTTON( BUTTON1_MOVED,          2)

#define BUTTON3_RELEASED       PDC_SHIFTED_BUTTON( BUTTON1_RELEASED,       3)
#define BUTTON3_PRESSED        PDC_SHIFTED_BUTTON( BUTTON1_PRESSED,        3)
#define BUTTON3_CLICKED        PDC_SHIFTED_BUTTON( BUTTON1_CLICKED,        3)
#define BUTTON3_DOUBLE_CLICKED PDC_SHIFTED_BUTTON( BUTTON1_DOUBLE_CLICKED, 3)
#define BUTTON3_TRIPLE_CLICKED PDC_SHIFTED_BUTTON( BUTTON1_TRIPLE_CLICKED, 3)
#define BUTTON3_MOVED          PDC_SHIFTED_BUTTON( BUTTON1_MOVED,          3)

/* For the ncurses-compatible functions only, BUTTON4_PRESSED and
   BUTTON5_PRESSED are returned for mouse scroll wheel up and down;
   otherwise PDCurses doesn't support buttons 4 and 5... except
   as described above for WinGUI,  and perhaps to be extended to
   other PDCurses flavors  */

#define BUTTON4_RELEASED       PDC_SHIFTED_BUTTON( BUTTON1_RELEASED,       4)
#define BUTTON4_PRESSED        PDC_SHIFTED_BUTTON( BUTTON1_PRESSED,        4)
#define BUTTON4_CLICKED        PDC_SHIFTED_BUTTON( BUTTON1_CLICKED,        4)
#define BUTTON4_DOUBLE_CLICKED PDC_SHIFTED_BUTTON( BUTTON1_DOUBLE_CLICKED, 4)
#define BUTTON4_TRIPLE_CLICKED PDC_SHIFTED_BUTTON( BUTTON1_TRIPLE_CLICKED, 4)
#define BUTTON4_MOVED          PDC_SHIFTED_BUTTON( BUTTON1_MOVED,          4)

#define BUTTON5_RELEASED       PDC_SHIFTED_BUTTON( BUTTON1_RELEASED,       5)
#define BUTTON5_PRESSED        PDC_SHIFTED_BUTTON( BUTTON1_PRESSED,        5)
#define BUTTON5_CLICKED        PDC_SHIFTED_BUTTON( BUTTON1_CLICKED,        5)
#define BUTTON5_DOUBLE_CLICKED PDC_SHIFTED_BUTTON( BUTTON1_DOUBLE_CLICKED, 5)
#define BUTTON5_TRIPLE_CLICKED PDC_SHIFTED_BUTTON( BUTTON1_TRIPLE_CLICKED, 5)
#define BUTTON5_MOVED          PDC_SHIFTED_BUTTON( BUTTON1_MOVED,          5)

#define MOUSE_WHEEL_SCROLL      PDC_SHIFTED_BUTTON( BUTTON1_RELEASED,       6)
#define BUTTON_MODIFIER_SHIFT   (MOUSE_WHEEL_SCROLL << 1)
#define BUTTON_MODIFIER_CONTROL (MOUSE_WHEEL_SCROLL << 2)
#define BUTTON_MODIFIER_ALT     (MOUSE_WHEEL_SCROLL << 3)
#define REPORT_MOUSE_POSITION   (MOUSE_WHEEL_SCROLL << 4)

#define ALL_MOUSE_EVENTS        (REPORT_MOUSE_POSITION - 1)

/* ncurses mouse interface */

typedef struct
{
    short id;       /* unused, always 0 */
    int x, y, z;    /* x, y same as MOUSE_STATUS; z unused */
    mmask_t bstate; /* equivalent to changes + button[], but
                       in the same format as used for mousemask() */
} MEVENT;

#if defined(PDC_NCMOUSE) && !defined(NCURSES_MOUSE_VERSION)
# define NCURSES_MOUSE_VERSION 2
#endif

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
 *  Window and Screen Structures
 *
 */

/* Avoid using the WINDOW or SCREEN structs directly -- doing so
   makes your code PDCurses*-only and may result in future binary
   incompatibility;  use the corresponding functions if possible.
   These structs may eventually be made private. */

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
    int   _pminrow, _pmincol;    /* saved position used only for pads */
    int   _sminrow, _smaxrow;    /* saved position used only for pads */
    int   _smincol, _smaxcol;    /* saved position used only for pads */
} WINDOW;

/* See above warning against directly accessing SCREEN elements. */

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
    mmask_t _trap_mbe;             /* trap these mouse button events */
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
    unsigned long key_modifiers;   /* key modifiers (SHIFT, CONTROL, etc.)
                                      on last key press */
    bool  return_key_modifiers;    /* TRUE if modifier keys are
                                      returned as "real" keys */
    bool  in_endwin;               /* if we're in endwin(),  we should use
                                      only signal-safe code */
    MOUSE_STATUS mouse_status;     /* last returned mouse status */
    short line_color;     /* color of line attributes - default -1 */
    attr_t termattrs;     /* attribute capabilities */
    WINDOW *lastscr;      /* the last screen image */
    FILE *dbfp;           /* debug trace file pointer */
    bool  color_started;  /* TRUE after start_color() */
    bool  dirty;          /* redraw on napms() after init_color() */
    int   sel_start;      /* start of selection (y * COLS + x) */
    int   sel_end;        /* end of selection */
    int  *c_buffer;       /* character buffer */
    int   c_pindex;       /* putter index */
    int   c_gindex;       /* getter index */
    int  *c_ungch;        /* array of ungotten chars */
    int   c_ungind;       /* ungetch() push index */
    int   c_ungmax;       /* allocated size of ungetch() buffer */
    struct _opaque_screen_t *opaque;    /* internal library variables */
} SCREEN;

/*----------------------------------------------------------------------
 *
 *  External Variables
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

/*man-start**************************************************************

Text Attributes
===============

By default,  PDCurses uses 64-bit integers for its chtype.  All chtypes
have bits devoted to character data,  attribute data,  and color pair data.
There are three configurations supported :

Default, 64-bit chtype,  both wide- and 8-bit character builds:
-------------------------------------------------------------------------------
|63|62|..|53|52|..|34|33|32|31|30|29|28|..|22|21|20|19|18|17|16|..| 3| 2| 1| 0|
-------------------------------------------------------------------------------
  unused    |color pair |        modifiers      |         character eg 'a'

   21 character bits (0-20),  enough for full Unicode coverage
   12 attribute bits (21-32)
   20 color pair bits (33-52),  enough for 1048576 color pairs
   11 currently unused bits (53-63)

32-bit chtypes with wide characters (CHTYPE_32 and PDC_WIDE are #defined):
    +--------------------------------------------------------------------+
    |31|30|29|28|27|26|25|24|23|22|21|20|19|18|17|16|15|14|13|..| 2| 1| 0|
    +--------------------------------------------------------------------+
          color pair        |     modifiers         |   character eg 'a'
   16 character bits (0-16),  enough for BMP (Unicode below 64K)
   8 attribute bits (16-23)
   8 color pair bits (24-31),  for 256 color pairs

32-bit chtypes with narrow characters (CHTYPE_32 #defined,  PDC_WIDE is not):
    +--------------------------------------------------------------------+
    |31|30|29|28|..|22|21|20|19|18|17|16|..|12|11|10| 9| 8| 7| 6|..| 1| 0|
    +--------------------------------------------------------------------+
          color pair        |     modifiers               |character
   8 character bits (0-7);  only 8-bit charsets will work
   12 attribute bits (8-19)
   12 color pair bits (20-31),  for 4096 pairs

All attribute modifier schemes include eight "basic" bits:  bold, underline,
right-line, left-line, italic, reverse and blink attributes,  plus the
alternate character set indicator. For default and 32-bit narrow builds,
three more bits are used for overlined, dimmed, and strikeout attributes;
a fourth bit is reserved.

Default chtypes have enough character bits to support the full range of
Unicode,  all attributes,  and 2^20 = 1048576 color pairs.  Note,  though,
that as of 2022 Jun 17,  only WinGUI,  VT,  X11,  Linux framebuffer,  and
SDLn have COLOR_PAIRS = 1048576.  Other platforms (DOSVGA,  Plan9, WinCon)
may join them.  Some (DOS,  OS/2) simply do not have full-color
capability.

**man-end****************************************************************/

/*** Video attribute macros ***/

#define A_NORMAL      (chtype)0

#ifndef CHTYPE_32
            /* 64-bit chtypes,  both wide- and narrow */
    # define PDC_CHARTEXT_BITS   21
    # define PDC_ATTRIBUTE_BITS  12
    # define PDC_COLOR_BITS      20
# else
#ifdef PDC_WIDE
            /* 32-bit chtypes,  wide character */
    # define PDC_CHARTEXT_BITS      16
    # define PDC_ATTRIBUTE_BITS      8
    # define PDC_COLOR_BITS          8
#else
            /* 32-bit chtypes,  narrow (8-bit) characters */
    # define PDC_CHARTEXT_BITS      8
    # define PDC_ATTRIBUTE_BITS    12
    # define PDC_COLOR_BITS        12
#endif
#endif

# define PDC_COLOR_SHIFT (PDC_CHARTEXT_BITS + PDC_ATTRIBUTE_BITS)
# define A_COLOR       ((((chtype)1 << PDC_COLOR_BITS) - 1) << PDC_COLOR_SHIFT)
# define A_ATTRIBUTES (((((chtype)1 << PDC_ATTRIBUTE_BITS) - 1) << PDC_CHARTEXT_BITS) | A_COLOR)
# define A_CHARTEXT     (((chtype)1 << PDC_CHARTEXT_BITS) - 1)

#define PDC_ATTRIBUTE_BIT( N)  ((chtype)1 << (N))
# define A_ALTCHARSET   PDC_ATTRIBUTE_BIT( PDC_CHARTEXT_BITS)
# define A_RIGHT        PDC_ATTRIBUTE_BIT( PDC_CHARTEXT_BITS + 1)
# define A_LEFT         PDC_ATTRIBUTE_BIT( PDC_CHARTEXT_BITS + 2)
# define A_INVIS        PDC_ATTRIBUTE_BIT( PDC_CHARTEXT_BITS + 3)
# define A_UNDERLINE    PDC_ATTRIBUTE_BIT( PDC_CHARTEXT_BITS + 4)
# define A_REVERSE      PDC_ATTRIBUTE_BIT( PDC_CHARTEXT_BITS + 5)
# define A_BLINK        PDC_ATTRIBUTE_BIT( PDC_CHARTEXT_BITS + 6)
# define A_BOLD         PDC_ATTRIBUTE_BIT( PDC_CHARTEXT_BITS + 7)
#if PDC_COLOR_BITS >= 11
    # define A_OVERLINE   PDC_ATTRIBUTE_BIT( PDC_CHARTEXT_BITS + 8)
    # define A_STRIKEOUT  PDC_ATTRIBUTE_BIT( PDC_CHARTEXT_BITS + 9)
    # define A_DIM        PDC_ATTRIBUTE_BIT( PDC_CHARTEXT_BITS + 10)
/*  Reserved bit :        PDC_ATTRIBUTE_BIT( PDC_CHARTEXT_BITS + 11) */
#else
    # define A_DIM        A_NORMAL
    # define A_OVERLINE   A_NORMAL
    # define A_STRIKEOUT  A_NORMAL
#endif

#define A_ITALIC      A_INVIS
#define A_PROTECT    (A_UNDERLINE | A_LEFT | A_RIGHT | A_OVERLINE)
#define A_STANDOUT    (A_REVERSE | A_BOLD) /* X/Open */

#define A_HORIZONTAL  A_NORMAL
#define A_LOW         A_NORMAL
#define A_TOP         A_NORMAL
#define A_VERTICAL    A_NORMAL

#define CHR_MSK       A_CHARTEXT           /* Obsolete */
#define ATR_MSK       A_ATTRIBUTES         /* Obsolete */
#define ATR_NRM       A_NORMAL             /* Obsolete */

#define A_LEFTLINE    A_LEFT
#define A_RIGHTLINE   A_RIGHT

/* For use with attr_t -- X/Open says, "these shall be distinct", so
   this is a non-conforming implementation. */

#define WA_NORMAL     A_NORMAL

#define WA_ALTCHARSET A_ALTCHARSET
#define WA_BLINK      A_BLINK
#define WA_BOLD       A_BOLD
#define WA_DIM        A_DIM
#define WA_INVIS      A_INVIS
#define WA_ITALIC     A_ITALIC
#define WA_LEFT       A_LEFT
#define WA_PROTECT    A_PROTECT
#define WA_REVERSE    A_REVERSE
#define WA_RIGHT      A_RIGHT
#define WA_STANDOUT   A_STANDOUT
#define WA_UNDERLINE  A_UNDERLINE

#define WA_HORIZONTAL A_HORIZONTAL
#define WA_LOW        A_LOW
#define WA_TOP        A_TOP
#define WA_VERTICAL   A_VERTICAL

#define WA_ATTRIBUTES A_ATTRIBUTES

/*** Alternate character set macros ***/

#define PDC_ACS(w) ((chtype)w | A_ALTCHARSET)

/* VT100-compatible symbols -- box chars */

#define ACS_LRCORNER      PDC_ACS('V')
#define ACS_URCORNER      PDC_ACS('W')
#define ACS_ULCORNER      PDC_ACS('X')
#define ACS_LLCORNER      PDC_ACS('Y')
#define ACS_PLUS          PDC_ACS('Z')
#define ACS_LTEE          PDC_ACS('[')
#define ACS_RTEE          PDC_ACS('\\')
#define ACS_BTEE          PDC_ACS(']')
#define ACS_TTEE          PDC_ACS('^')
#define ACS_HLINE         PDC_ACS('_')
#define ACS_VLINE         PDC_ACS('`')

/* PDCurses-only ACS chars.  Don't use if ncurses compatibility matters.
Some won't work in non-wide X11 builds (see 'acs_defs.h' for details). */

#define ACS_CENT          PDC_ACS('{')
#define ACS_YEN           PDC_ACS('|')
#define ACS_PESETA        PDC_ACS('}')
#define ACS_HALF          PDC_ACS('&')
#define ACS_QUARTER       PDC_ACS('\'')
#define ACS_LEFT_ANG_QU   PDC_ACS(')')
#define ACS_RIGHT_ANG_QU  PDC_ACS('*')
#define ACS_D_HLINE       PDC_ACS('a')
#define ACS_D_VLINE       PDC_ACS('b')
#define ACS_CLUB          PDC_ACS( 11)
#define ACS_HEART         PDC_ACS( 12)
#define ACS_SPADE         PDC_ACS( 13)
#define ACS_SMILE         PDC_ACS( 14)
#define ACS_REV_SMILE     PDC_ACS( 15)
#define ACS_MED_BULLET    PDC_ACS( 16)
#define ACS_WHITE_BULLET  PDC_ACS( 17)
#define ACS_PILCROW       PDC_ACS( 18)
#define ACS_SECTION       PDC_ACS( 19)

#define ACS_SUP2          PDC_ACS(',')
#define ACS_ALPHA         PDC_ACS('.')
#define ACS_BETA          PDC_ACS('/')
#define ACS_GAMMA         PDC_ACS('0')
#define ACS_UP_SIGMA      PDC_ACS('1')
#define ACS_LO_SIGMA      PDC_ACS('2')
#define ACS_MU            PDC_ACS('4')
#define ACS_TAU           PDC_ACS('5')
#define ACS_UP_PHI        PDC_ACS('6')
#define ACS_THETA         PDC_ACS('7')
#define ACS_OMEGA         PDC_ACS('8')
#define ACS_DELTA         PDC_ACS('9')
#define ACS_INFINITY      PDC_ACS('-')
#define ACS_LO_PHI        PDC_ACS( 22)
#define ACS_EPSILON       PDC_ACS(':')
#define ACS_INTERSECT     PDC_ACS('e')
#define ACS_TRIPLE_BAR    PDC_ACS('f')
#define ACS_DIVISION      PDC_ACS('c')
#define ACS_APPROX_EQ     PDC_ACS('d')
#define ACS_SM_BULLET     PDC_ACS('g')
#define ACS_SQUARE_ROOT   PDC_ACS('i')
#define ACS_UBLOCK        PDC_ACS('p')
#define ACS_BBLOCK        PDC_ACS('q')
#define ACS_LBLOCK        PDC_ACS('r')
#define ACS_RBLOCK        PDC_ACS('s')

#define ACS_A_ORDINAL     PDC_ACS(20)
#define ACS_O_ORDINAL     PDC_ACS(21)
#define ACS_INV_QUERY     PDC_ACS(24)
#define ACS_REV_NOT       PDC_ACS(25)
#define ACS_NOT           PDC_ACS(26)
#define ACS_INV_BANG      PDC_ACS(23)
#define ACS_UP_INTEGRAL   PDC_ACS(27)
#define ACS_LO_INTEGRAL   PDC_ACS(28)
#define ACS_SUP_N         PDC_ACS(29)
#define ACS_CENTER_SQU    PDC_ACS(30)
#define ACS_F_WITH_HOOK   PDC_ACS(31)

#define ACS_SD_LRCORNER   PDC_ACS(';')
#define ACS_SD_URCORNER   PDC_ACS('<')
#define ACS_SD_ULCORNER   PDC_ACS('=')
#define ACS_SD_LLCORNER   PDC_ACS('>')
#define ACS_SD_PLUS       PDC_ACS('?')
#define ACS_SD_LTEE       PDC_ACS('@')
#define ACS_SD_RTEE       PDC_ACS('A')
#define ACS_SD_BTEE       PDC_ACS('B')
#define ACS_SD_TTEE       PDC_ACS('C')

#define ACS_D_LRCORNER    PDC_ACS('D')
#define ACS_D_URCORNER    PDC_ACS('E')
#define ACS_D_ULCORNER    PDC_ACS('F')
#define ACS_D_LLCORNER    PDC_ACS('G')
#define ACS_D_PLUS        PDC_ACS('H')
#define ACS_D_LTEE        PDC_ACS('I')
#define ACS_D_RTEE        PDC_ACS('J')
#define ACS_D_BTEE        PDC_ACS('K')
#define ACS_D_TTEE        PDC_ACS('L')

#define ACS_DS_LRCORNER   PDC_ACS('M')
#define ACS_DS_URCORNER   PDC_ACS('N')
#define ACS_DS_ULCORNER   PDC_ACS('O')
#define ACS_DS_LLCORNER   PDC_ACS('P')
#define ACS_DS_PLUS       PDC_ACS('Q')
#define ACS_DS_LTEE       PDC_ACS('R')
#define ACS_DS_RTEE       PDC_ACS('S')
#define ACS_DS_BTEE       PDC_ACS('T')
#define ACS_DS_TTEE       PDC_ACS('U')

/* VT100-compatible symbols -- other */

#define ACS_S1            PDC_ACS('l')
#define ACS_S9            PDC_ACS('o')
#define ACS_DIAMOND       PDC_ACS('j')
#define ACS_CKBOARD       PDC_ACS('k')
#define ACS_DEGREE        PDC_ACS('w')
#define ACS_PLMINUS       PDC_ACS('x')
#define ACS_BULLET        PDC_ACS('h')

/* Teletype 5410v1 symbols -- these are defined in SysV curses, but
   are not well-supported by most terminals. Stick to VT100 characters
   for optimum portability. */

#define ACS_LARROW        PDC_ACS('!')
#define ACS_RARROW        PDC_ACS(' ')
#define ACS_DARROW        PDC_ACS('#')
#define ACS_UARROW        PDC_ACS('"')
#define ACS_BOARD         PDC_ACS('+')
#define ACS_LTBOARD       PDC_ACS('y')
#define ACS_LANTERN       PDC_ACS('z')
#define ACS_BLOCK         PDC_ACS('t')

/* That goes double for these -- undocumented SysV symbols. Don't use
   them. */

#define ACS_S3            PDC_ACS('m')
#define ACS_S7            PDC_ACS('n')
#define ACS_LEQUAL        PDC_ACS('u')
#define ACS_GEQUAL        PDC_ACS('v')
#define ACS_PI            PDC_ACS('$')
#define ACS_NEQUAL        PDC_ACS('%')
#define ACS_STERLING      PDC_ACS('~')

/* Box char aliases.  The four characters tell you if a Single
line points up, right, down,  and/or left from the center;
or if it's Blank;  or if it's Thick or Double.  The Thick
ones are an ncurses extension;  the Double and Single/Double
ones are a PDCursesMod extension. */

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

# define WACS_T_LRCORNER    (&(acs_map[0]))
# define WACS_T_URCORNER    (&(acs_map[1]))
# define WACS_T_ULCORNER    (&(acs_map[2]))
# define WACS_T_LLCORNER    (&(acs_map[3]))
# define WACS_T_PLUS        (&(acs_map[4]))
# define WACS_T_LTEE        (&(acs_map[5]))
# define WACS_T_RTEE        (&(acs_map[6]))
# define WACS_T_BTEE        (&(acs_map[7]))
# define WACS_T_TTEE        (&(acs_map[8]))
# define WACS_T_HLINE       (&(acs_map[9]))
# define WACS_T_VLINE       (&(acs_map[10]))

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

# define WACS_BTTB     WACS_T_ULCORNER
# define WACS_TTBB     WACS_T_LLCORNER
# define WACS_BBTT     WACS_T_URCORNER
# define WACS_TBBT     WACS_T_LRCORNER
# define WACS_TBTT     WACS_T_RTEE
# define WACS_TTTB     WACS_T_LTEE
# define WACS_TTBT     WACS_T_BTEE
# define WACS_BTTS     WACS_T_TTEE
# define WACS_BTBT     WACS_T_HLINE
# define WACS_TBTB     WACS_T_VLINE
# define WACS_TTTT     WACS_T_PLUS
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
 *  Function and Keypad Key Definitions
 *  Many are just for compatibility
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
#define CTL_DOWN              (KEY_OFFSET + 0xe1) /* ctl-down arrow: orig PDCurses def */
#define CTL_DN                (KEY_OFFSET + 0xe1) /* ctl-down arrow: ncurses def */
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

      /* The following are PDCursesMod extensions.  Even there,  not all
         platforms support them. */

#define KEY_APPS              (KEY_OFFSET + 0x125)

#define KEY_PAUSE             (KEY_OFFSET + 0x126)

#define KEY_PRINTSCREEN       (KEY_OFFSET + 0x127)
#define KEY_SCROLLLOCK        (KEY_OFFSET + 0x128)

#define KEY_BROWSER_BACK      (KEY_OFFSET + 0x129)
#define KEY_BROWSER_FWD       (KEY_OFFSET + 0x12a)
#define KEY_BROWSER_REF       (KEY_OFFSET + 0x12b)
#define KEY_BROWSER_STOP      (KEY_OFFSET + 0x12c)
#define KEY_SEARCH            (KEY_OFFSET + 0x12d)
#define KEY_FAVORITES         (KEY_OFFSET + 0x12e)
#define KEY_BROWSER_HOME      (KEY_OFFSET + 0x12f)
#define KEY_VOLUME_MUTE       (KEY_OFFSET + 0x130)
#define KEY_VOLUME_DOWN       (KEY_OFFSET + 0x131)
#define KEY_VOLUME_UP         (KEY_OFFSET + 0x132)
#define KEY_NEXT_TRACK        (KEY_OFFSET + 0x133)
#define KEY_PREV_TRACK        (KEY_OFFSET + 0x134)
#define KEY_MEDIA_STOP        (KEY_OFFSET + 0x135)
#define KEY_PLAY_PAUSE        (KEY_OFFSET + 0x136)
#define KEY_LAUNCH_MAIL       (KEY_OFFSET + 0x137)
#define KEY_MEDIA_SELECT      (KEY_OFFSET + 0x138)
#define KEY_LAUNCH_APP1       (KEY_OFFSET + 0x139)
#define KEY_LAUNCH_APP2       (KEY_OFFSET + 0x13a)
#define KEY_LAUNCH_APP3       (KEY_OFFSET + 0x13b)
#define KEY_LAUNCH_APP4       (KEY_OFFSET + 0x13c)
#define KEY_LAUNCH_APP5       (KEY_OFFSET + 0x13d)
#define KEY_LAUNCH_APP6       (KEY_OFFSET + 0x13e)
#define KEY_LAUNCH_APP7       (KEY_OFFSET + 0x13f)
#define KEY_LAUNCH_APP8       (KEY_OFFSET + 0x140)
#define KEY_LAUNCH_APP9       (KEY_OFFSET + 0x141)
#define KEY_LAUNCH_APP10      (KEY_OFFSET + 0x142)

#define KEY_MIN       KEY_BREAK         /* Minimum curses key value */
#define KEY_MAX       KEY_LAUNCH_APP10  /* Maximum curses key */

#define KEY_F(n)      (KEY_F0 + (n))

/*----------------------------------------------------------------------
 *
 *  PDCurses Function Declarations
 *
 */

/* Standard */

PDCEX  int     addch(const chtype);
PDCEX  int     addchnstr(const chtype *, int);
PDCEX  int     addchstr(const chtype *);
PDCEX  int     addnstr(const char *, int);
PDCEX  int     addstr(const char *);
PDCEX  int     attroff(chtype);
PDCEX  int     attron(chtype);
PDCEX  int     attrset(chtype);
PDCEX  int     attr_get(attr_t *, short *, void *);
PDCEX  int     attr_off(attr_t, void *);
PDCEX  int     attr_on(attr_t, void *);
PDCEX  int     attr_set(attr_t, short, void *);
PDCEX  int     baudrate(void);
PDCEX  int     beep(void);
PDCEX  int     bkgd(chtype);
PDCEX  void    bkgdset(chtype);
PDCEX  int     border(chtype, chtype, chtype, chtype,
                      chtype, chtype, chtype, chtype);
PDCEX  int     box(WINDOW *, chtype, chtype);
PDCEX  bool    can_change_color(void);
PDCEX  int     cbreak(void);
PDCEX  int     chgat(int, attr_t, short, const void *);
PDCEX  int     clearok(WINDOW *, bool);
PDCEX  int     clear(void);
PDCEX  int     clrtobot(void);
PDCEX  int     clrtoeol(void);
PDCEX  int     color_content(short, short *, short *, short *);
PDCEX  int     color_set(short, void *);
PDCEX  int     copywin(const WINDOW *, WINDOW *, int, int, int,
                       int, int, int, int);
PDCEX  int     curs_set(int);
PDCEX  int     def_prog_mode(void);
PDCEX  int     def_shell_mode(void);
PDCEX  int     delay_output(int);
PDCEX  int     delch(void);
PDCEX  int     deleteln(void);
PDCEX  void    delscreen(SCREEN *);
PDCEX  int     delwin(WINDOW *);
PDCEX  WINDOW *derwin(WINDOW *, int, int, int, int);
PDCEX  int     doupdate(void);
PDCEX  WINDOW *dupwin(WINDOW *);
PDCEX  int     echochar(const chtype);
PDCEX  int     echo(void);

#ifdef PDC_WIDE
   #ifdef PDC_FORCE_UTF8
      #ifdef CHTYPE_32
         #define endwin endwin_u32_4302
      #else
         #define endwin endwin_u64_4302
      #endif
   #else
      #ifdef CHTYPE_32
         #define endwin endwin_w32_4302
      #else
         #define endwin endwin_w64_4302
      #endif
   #endif
#else       /* 8-bit chtypes */
   #ifdef CHTYPE_32
      #define endwin endwin_x32_4302
   #else
      #define endwin endwin_x64_4302
   #endif
#endif

PDCEX  int     endwin(void);
PDCEX  char    erasechar(void);
PDCEX  int     erase(void);
PDCEX  int     extended_color_content(int, int *, int *, int *);
PDCEX  int     extended_pair_content(int, int *, int *);
PDCEX  void    filter(void);
PDCEX  int     flash(void);
PDCEX  int     flushinp(void);
PDCEX  chtype  getbkgd(WINDOW *);
PDCEX  int     getnstr(char *, int);
PDCEX  int     getstr(char *);
PDCEX  WINDOW *getwin(FILE *);
PDCEX  int     halfdelay(int);
PDCEX  bool    has_colors(void);
PDCEX  bool    has_ic(void);
PDCEX  bool    has_il(void);
PDCEX  int     hline(chtype, int);
PDCEX  void    idcok(WINDOW *, bool);
PDCEX  int     idlok(WINDOW *, bool);
PDCEX  void    immedok(WINDOW *, bool);
PDCEX  int     inchnstr(chtype *, int);
PDCEX  int     inchstr(chtype *);
PDCEX  chtype  inch(void);
PDCEX  int     init_color(short, short, short, short);
PDCEX  int     init_extended_color(int, int, int, int);
PDCEX  int     init_extended_pair(int, int, int);
PDCEX  int     init_pair(short, short, short);
PDCEX  WINDOW *initscr(void);
PDCEX  int     innstr(char *, int);
PDCEX  int     insch(chtype);
PDCEX  int     insdelln(int);
PDCEX  int     insertln(void);
PDCEX  int     insnstr(const char *, int);
PDCEX  int     insstr(const char *);
PDCEX  int     instr(char *);
PDCEX  int     intrflush(WINDOW *, bool);
PDCEX  bool    isendwin(void);
PDCEX  bool    is_linetouched(WINDOW *, int);
PDCEX  bool    is_wintouched(WINDOW *);
PDCEX  char   *keyname(int);
PDCEX  int     keypad(WINDOW *, bool);
PDCEX  char    killchar(void);
PDCEX  int     leaveok(WINDOW *, bool);
PDCEX  char   *longname(void);
PDCEX  int     meta(WINDOW *, bool);
PDCEX  int     move(int, int);
PDCEX  int     mvaddch(int, int, const chtype);
PDCEX  int     mvaddchnstr(int, int, const chtype *, int);
PDCEX  int     mvaddchstr(int, int, const chtype *);
PDCEX  int     mvaddnstr(int, int, const char *, int);
PDCEX  int     mvaddstr(int, int, const char *);
PDCEX  int     mvchgat(int, int, int, attr_t, short, const void *);
PDCEX  int     mvcur(int, int, int, int);
PDCEX  int     mvdelch(int, int);
PDCEX  int     mvderwin(WINDOW *, int, int);
PDCEX  int     mvgetch(int, int);
PDCEX  int     mvgetnstr(int, int, char *, int);
PDCEX  int     mvgetstr(int, int, char *);
PDCEX  int     mvhline(int, int, chtype, int);
PDCEX  chtype  mvinch(int, int);
PDCEX  int     mvinchnstr(int, int, chtype *, int);
PDCEX  int     mvinchstr(int, int, chtype *);
PDCEX  int     mvinnstr(int, int, char *, int);
PDCEX  int     mvinsch(int, int, chtype);
PDCEX  int     mvinsnstr(int, int, const char *, int);
PDCEX  int     mvinsstr(int, int, const char *);
PDCEX  int     mvinstr(int, int, char *);
PDCEX  int     mvprintw(int, int, const char *, ...);
PDCEX  int     mvscanw(int, int, const char *, ...);
PDCEX  int     mvvline(int, int, chtype, int);
PDCEX  int     mvwaddchnstr(WINDOW *, int, int, const chtype *, int);
PDCEX  int     mvwaddchstr(WINDOW *, int, int, const chtype *);
PDCEX  int     mvwaddch(WINDOW *, int, int, const chtype);
PDCEX  int     mvwaddnstr(WINDOW *, int, int, const char *, int);
PDCEX  int     mvwaddstr(WINDOW *, int, int, const char *);
PDCEX  int     mvwchgat(WINDOW *, int, int, int, attr_t, short, const void *);
PDCEX  int     mvwdelch(WINDOW *, int, int);
PDCEX  int     mvwgetch(WINDOW *, int, int);
PDCEX  int     mvwgetnstr(WINDOW *, int, int, char *, int);
PDCEX  int     mvwgetstr(WINDOW *, int, int, char *);
PDCEX  int     mvwhline(WINDOW *, int, int, chtype, int);
PDCEX  int     mvwinchnstr(WINDOW *, int, int, chtype *, int);
PDCEX  int     mvwinchstr(WINDOW *, int, int, chtype *);
PDCEX  chtype  mvwinch(WINDOW *, int, int);
PDCEX  int     mvwinnstr(WINDOW *, int, int, char *, int);
PDCEX  int     mvwinsch(WINDOW *, int, int, chtype);
PDCEX  int     mvwinsnstr(WINDOW *, int, int, const char *, int);
PDCEX  int     mvwinsstr(WINDOW *, int, int, const char *);
PDCEX  int     mvwinstr(WINDOW *, int, int, char *);
PDCEX  int     mvwin(WINDOW *, int, int);
PDCEX  int     mvwprintw(WINDOW *, int, int, const char *, ...);
PDCEX  int     mvwscanw(WINDOW *, int, int, const char *, ...);
PDCEX  int     mvwvline(WINDOW *, int, int, chtype, int);
PDCEX  int     napms(int);
PDCEX  WINDOW *newpad(int, int);
PDCEX  SCREEN *newterm(const char *, FILE *, FILE *);
PDCEX  WINDOW *newwin(int, int, int, int);
PDCEX  int     nl(void);
PDCEX  int     nocbreak(void);
PDCEX  int     nodelay(WINDOW *, bool);
PDCEX  int     noecho(void);
PDCEX  int     nonl(void);
PDCEX  void    noqiflush(void);
PDCEX  int     noraw(void);
PDCEX  int     notimeout(WINDOW *, bool);
PDCEX  int     overlay(const WINDOW *, WINDOW *);
PDCEX  int     overwrite(const WINDOW *, WINDOW *);
PDCEX  int     pair_content(short, short *, short *);
PDCEX  int     pechochar(WINDOW *, chtype);
PDCEX  int     pnoutrefresh(WINDOW *, int, int, int, int, int, int);
PDCEX  int     prefresh(WINDOW *, int, int, int, int, int, int);
PDCEX  int     printw(const char *, ...);
PDCEX  int     putwin(WINDOW *, FILE *);
PDCEX  void    qiflush(void);
PDCEX  int     raw(void);
PDCEX  int     redrawwin(WINDOW *);
PDCEX  int     refresh(void);
PDCEX  int     reset_prog_mode(void);
PDCEX  int     reset_shell_mode(void);
PDCEX  int     resetty(void);
PDCEX  int     ripoffline(int, int (*)(WINDOW *, int));
PDCEX  int     savetty(void);
PDCEX  int     scanw(const char *, ...);
PDCEX  int     scr_dump(const char *);
PDCEX  int     scr_init(const char *);
PDCEX  int     scr_restore(const char *);
PDCEX  int     scr_set(const char *);
PDCEX  int     scrl(int);
PDCEX  int     scroll(WINDOW *);
PDCEX  int     scrollok(WINDOW *, bool);
PDCEX  SCREEN *set_term(SCREEN *);
PDCEX  int     setscrreg(int, int);
PDCEX  attr_t  slk_attr(void);
PDCEX  int     slk_attroff(const chtype);
PDCEX  int     slk_attr_off(const attr_t, void *);
PDCEX  int     slk_attron(const chtype);
PDCEX  int     slk_attr_on(const attr_t, void *);
PDCEX  int     slk_attrset(const chtype);
PDCEX  int     slk_attr_set(const attr_t, short, void *);
PDCEX  int     slk_clear(void);
PDCEX  int     extended_slk_color(int);
PDCEX  int     slk_color(short);
PDCEX  int     slk_init(int);
PDCEX  char   *slk_label(int);
PDCEX  int     slk_noutrefresh(void);
PDCEX  int     slk_refresh(void);
PDCEX  int     slk_restore(void);
PDCEX  int     slk_set(int, const char *, int);
PDCEX  int     slk_touch(void);
PDCEX  int     standend(void);
PDCEX  int     standout(void);
PDCEX  int     start_color(void);
PDCEX  WINDOW *subpad(WINDOW *, int, int, int, int);
PDCEX  WINDOW *subwin(WINDOW *, int, int, int, int);
PDCEX  int     syncok(WINDOW *, bool);
PDCEX  chtype  termattrs(void);
PDCEX  attr_t  term_attrs(void);
PDCEX  char   *termname(void);
PDCEX  void    timeout(int);
PDCEX  int     touchline(WINDOW *, int, int);
PDCEX  int     touchwin(WINDOW *);
PDCEX  int     typeahead(int);
PDCEX  int     untouchwin(WINDOW *);
PDCEX  void    use_env(bool);
PDCEX  int     vidattr(chtype);
PDCEX  int     vid_attr(attr_t, short, void *);
PDCEX  int     vidputs(chtype, int (*)(int));
PDCEX  int     vid_puts(attr_t, short, void *, int (*)(int));
PDCEX  int     vline(chtype, int);
PDCEX  int     vw_printw(WINDOW *, const char *, va_list);
PDCEX  int     vwprintw(WINDOW *, const char *, va_list);
PDCEX  int     vw_scanw(WINDOW *, const char *, va_list);
PDCEX  int     vwscanw(WINDOW *, const char *, va_list);
PDCEX  int     waddchnstr(WINDOW *, const chtype *, int);
PDCEX  int     waddchstr(WINDOW *, const chtype *);
PDCEX  int     waddch(WINDOW *, const chtype);
PDCEX  int     waddnstr(WINDOW *, const char *, int);
PDCEX  int     waddstr(WINDOW *, const char *);
PDCEX  int     wattroff(WINDOW *, chtype);
PDCEX  int     wattron(WINDOW *, chtype);
PDCEX  int     wattrset(WINDOW *, chtype);
PDCEX  int     wattr_get(WINDOW *, attr_t *, short *, void *);
PDCEX  int     wattr_off(WINDOW *, attr_t, void *);
PDCEX  int     wattr_on(WINDOW *, attr_t, void *);
PDCEX  int     wattr_set(WINDOW *, attr_t, short, void *);
PDCEX  void    wbkgdset(WINDOW *, chtype);
PDCEX  int     wbkgd(WINDOW *, chtype);
PDCEX  int     wborder(WINDOW *, chtype, chtype, chtype, chtype,
                        chtype, chtype, chtype, chtype);
PDCEX  int     wchgat(WINDOW *, int, attr_t, short, const void *);
PDCEX  int     wclear(WINDOW *);
PDCEX  int     wclrtobot(WINDOW *);
PDCEX  int     wclrtoeol(WINDOW *);
PDCEX  int     wcolor_set(WINDOW *, short, void *);
PDCEX  void    wcursyncup(WINDOW *);
PDCEX  int     wdelch(WINDOW *);
PDCEX  int     wdeleteln(WINDOW *);
PDCEX  int     wechochar(WINDOW *, const chtype);
PDCEX  int     werase(WINDOW *);
PDCEX  int     wgetch(WINDOW *);
PDCEX  int     wgetnstr(WINDOW *, char *, int);
PDCEX  int     wgetstr(WINDOW *, char *);
PDCEX  int     whline(WINDOW *, chtype, int);
PDCEX  int     winchnstr(WINDOW *, chtype *, int);
PDCEX  int     winchstr(WINDOW *, chtype *);
PDCEX  chtype  winch(WINDOW *);
PDCEX  int     winnstr(WINDOW *, char *, int);
PDCEX  int     winsch(WINDOW *, chtype);
PDCEX  int     winsdelln(WINDOW *, int);
PDCEX  int     winsertln(WINDOW *);
PDCEX  int     winsnstr(WINDOW *, const char *, int);
PDCEX  int     winsstr(WINDOW *, const char *);
PDCEX  int     winstr(WINDOW *, char *);
PDCEX  int     wmove(WINDOW *, int, int);
PDCEX  int     wnoutrefresh(WINDOW *);
PDCEX  int     wprintw(WINDOW *, const char *, ...);
PDCEX  int     wredrawln(WINDOW *, int, int);
PDCEX  int     wrefresh(WINDOW *);
PDCEX  int     wscanw(WINDOW *, const char *, ...);
PDCEX  int     wscrl(WINDOW *, int);
PDCEX  int     wsetscrreg(WINDOW *, int, int);
PDCEX  int     wstandend(WINDOW *);
PDCEX  int     wstandout(WINDOW *);
PDCEX  void    wsyncdown(WINDOW *);
PDCEX  void    wsyncup(WINDOW *);
PDCEX  void    wtimeout(WINDOW *, int);
PDCEX  int     wtouchln(WINDOW *, int, int, int);
PDCEX  int     wvline(WINDOW *, chtype, int);

/* Wide-character functions */

#ifdef PDC_WIDE
PDCEX  int     addnwstr(const wchar_t *, int);
PDCEX  int     addwstr(const wchar_t *);
PDCEX  int     add_wch(const cchar_t *);
PDCEX  int     add_wchnstr(const cchar_t *, int);
PDCEX  int     add_wchstr(const cchar_t *);
PDCEX  int     bkgrnd(const cchar_t *);
PDCEX  void    bkgrndset(const cchar_t *);
PDCEX  int     border_set(const cchar_t *, const cchar_t *, const cchar_t *,
                          const cchar_t *, const cchar_t *, const cchar_t *,
                          const cchar_t *, const cchar_t *);
PDCEX  int     box_set(WINDOW *, const cchar_t *, const cchar_t *);
PDCEX  int     echo_wchar(const cchar_t *);
PDCEX  int     erasewchar(wchar_t *);
PDCEX  int     getbkgrnd(cchar_t *);
PDCEX  int     getcchar(const cchar_t *, wchar_t *, attr_t *, short *, void *);
PDCEX  int     getn_wstr(wint_t *, int);
PDCEX  int     get_wch(wint_t *);
PDCEX  int     get_wstr(wint_t *);
PDCEX  int     hline_set(const cchar_t *, int);
PDCEX  int     innwstr(wchar_t *, int);
PDCEX  int     ins_nwstr(const wchar_t *, int);
PDCEX  int     ins_wch(const cchar_t *);
PDCEX  int     ins_wstr(const wchar_t *);
PDCEX  int     inwstr(wchar_t *);
PDCEX  int     in_wch(cchar_t *);
PDCEX  int     in_wchnstr(cchar_t *, int);
PDCEX  int     in_wchstr(cchar_t *);
PDCEX  char   *key_name(wchar_t);
PDCEX  int     killwchar(wchar_t *);
PDCEX  int     mvaddnwstr(int, int, const wchar_t *, int);
PDCEX  int     mvaddwstr(int, int, const wchar_t *);
PDCEX  int     mvadd_wch(int, int, const cchar_t *);
PDCEX  int     mvadd_wchnstr(int, int, const cchar_t *, int);
PDCEX  int     mvadd_wchstr(int, int, const cchar_t *);
PDCEX  int     mvgetn_wstr(int, int, wint_t *, int);
PDCEX  int     mvget_wch(int, int, wint_t *);
PDCEX  int     mvget_wstr(int, int, wint_t *);
PDCEX  int     mvhline_set(int, int, const cchar_t *, int);
PDCEX  int     mvinnwstr(int, int, wchar_t *, int);
PDCEX  int     mvins_nwstr(int, int, const wchar_t *, int);
PDCEX  int     mvins_wch(int, int, const cchar_t *);
PDCEX  int     mvins_wstr(int, int, const wchar_t *);
PDCEX  int     mvinwstr(int, int, wchar_t *);
PDCEX  int     mvin_wch(int, int, cchar_t *);
PDCEX  int     mvin_wchnstr(int, int, cchar_t *, int);
PDCEX  int     mvin_wchstr(int, int, cchar_t *);
PDCEX  int     mvvline_set(int, int, const cchar_t *, int);
PDCEX  int     mvwaddnwstr(WINDOW *, int, int, const wchar_t *, int);
PDCEX  int     mvwaddwstr(WINDOW *, int, int, const wchar_t *);
PDCEX  int     mvwadd_wch(WINDOW *, int, int, const cchar_t *);
PDCEX  int     mvwadd_wchnstr(WINDOW *, int, int, const cchar_t *, int);
PDCEX  int     mvwadd_wchstr(WINDOW *, int, int, const cchar_t *);
PDCEX  int     mvwgetn_wstr(WINDOW *, int, int, wint_t *, int);
PDCEX  int     mvwget_wch(WINDOW *, int, int, wint_t *);
PDCEX  int     mvwget_wstr(WINDOW *, int, int, wint_t *);
PDCEX  int     mvwhline_set(WINDOW *, int, int, const cchar_t *, int);
PDCEX  int     mvwinnwstr(WINDOW *, int, int, wchar_t *, int);
PDCEX  int     mvwins_nwstr(WINDOW *, int, int, const wchar_t *, int);
PDCEX  int     mvwins_wch(WINDOW *, int, int, const cchar_t *);
PDCEX  int     mvwins_wstr(WINDOW *, int, int, const wchar_t *);
PDCEX  int     mvwin_wch(WINDOW *, int, int, cchar_t *);
PDCEX  int     mvwin_wchnstr(WINDOW *, int, int, cchar_t *, int);
PDCEX  int     mvwin_wchstr(WINDOW *, int, int, cchar_t *);
PDCEX  int     mvwinwstr(WINDOW *, int, int, wchar_t *);
PDCEX  int     mvwvline_set(WINDOW *, int, int, const cchar_t *, int);
PDCEX  int     pecho_wchar(WINDOW *, const cchar_t*);
PDCEX  int     setcchar(cchar_t*, const wchar_t*, const attr_t,
                        short, const void*);
PDCEX  int     slk_wset(int, const wchar_t *, int);
PDCEX  int     unget_wch(const wchar_t);
PDCEX  int     vline_set(const cchar_t *, int);
PDCEX  int     waddnwstr(WINDOW *, const wchar_t *, int);
PDCEX  int     waddwstr(WINDOW *, const wchar_t *);
PDCEX  int     wadd_wch(WINDOW *, const cchar_t *);
PDCEX  int     wadd_wchnstr(WINDOW *, const cchar_t *, int);
PDCEX  int     wadd_wchstr(WINDOW *, const cchar_t *);
PDCEX  int     wbkgrnd(WINDOW *, const cchar_t *);
PDCEX  void    wbkgrndset(WINDOW *, const cchar_t *);
PDCEX  int     wborder_set(WINDOW *, const cchar_t *, const cchar_t *,
                           const cchar_t *, const cchar_t *, const cchar_t *,
                           const cchar_t *, const cchar_t *, const cchar_t *);
PDCEX  int     wecho_wchar(WINDOW *, const cchar_t *);
PDCEX  int     wgetbkgrnd(WINDOW *, cchar_t *);
PDCEX  int     wgetn_wstr(WINDOW *, wint_t *, int);
PDCEX  int     wget_wch(WINDOW *, wint_t *);
PDCEX  int     wget_wstr(WINDOW *, wint_t *);
PDCEX  int     whline_set(WINDOW *, const cchar_t *, int);
PDCEX  int     winnwstr(WINDOW *, wchar_t *, int);
PDCEX  int     wins_nwstr(WINDOW *, const wchar_t *, int);
PDCEX  int     wins_wch(WINDOW *, const cchar_t *);
PDCEX  int     wins_wstr(WINDOW *, const wchar_t *);
PDCEX  int     winwstr(WINDOW *, wchar_t *);
PDCEX  int     win_wch(WINDOW *, cchar_t *);
PDCEX  int     win_wchnstr(WINDOW *, cchar_t *, int);
PDCEX  int     win_wchstr(WINDOW *, cchar_t *);
PDCEX  wchar_t *wunctrl(cchar_t *);
PDCEX  int     wvline_set(WINDOW *, const cchar_t *, int);
#endif

/* Quasi-standard */

PDCEX  chtype  getattrs(WINDOW *);
PDCEX  int     getbegx(WINDOW *);
PDCEX  int     getbegy(WINDOW *);
PDCEX  int     getmaxx(WINDOW *);
PDCEX  int     getmaxy(WINDOW *);
PDCEX  int     getparx(WINDOW *);
PDCEX  int     getpary(WINDOW *);
PDCEX  int     getcurx(WINDOW *);
PDCEX  int     getcury(WINDOW *);
PDCEX  void    traceoff(void);
PDCEX  void    traceon(void);
PDCEX  void    trace( const unsigned);
PDCEX  unsigned curses_trace( const unsigned);
PDCEX  char   *unctrl(chtype);

PDCEX  int     crmode(void);
PDCEX  int     nocrmode(void);
PDCEX  int     draino(int);
PDCEX  int     resetterm(void);
PDCEX  int     fixterm(void);
PDCEX  int     saveterm(void);
PDCEX  void    setsyx(int, int);

PDCEX  int     mouse_set(mmask_t);
PDCEX  int     mouse_on(mmask_t);
PDCEX  int     mouse_off(mmask_t);
PDCEX  int     request_mouse_pos(void);
PDCEX  void    wmouse_position(WINDOW *, int *, int *);
PDCEX  mmask_t getmouse(void);

/* ncurses */

PDCEX  int     alloc_pair(int, int);
PDCEX  int     assume_default_colors(int, int);
PDCEX  const char *curses_version(void);
PDCEX  int     find_pair(int, int);
PDCEX  int     free_pair( int);
PDCEX  bool    has_key(int);
PDCEX  bool    is_cleared(const WINDOW *);
PDCEX  bool    is_idcok(const WINDOW *);
PDCEX  bool    is_idlok(const WINDOW *);
PDCEX  bool    is_immedok(const WINDOW *);
PDCEX  bool    is_keypad(const WINDOW *);
PDCEX  bool    is_leaveok(const WINDOW *);
PDCEX  bool    is_leaveok(const WINDOW *);
PDCEX  bool    is_nodelay(const WINDOW *);
PDCEX  bool    is_notimeout(const WINDOW *);
PDCEX  bool    is_pad(const WINDOW *);
PDCEX  void    reset_color_pairs( void);
PDCEX  bool    is_scrollok(const WINDOW *);
PDCEX  bool    is_subwin(const WINDOW *);
PDCEX  bool    is_syncok(const WINDOW *);
PDCEX  int     set_tabsize(int);
PDCEX  int     use_default_colors(void);
PDCEX  int     wgetdelay(const WINDOW *);
PDCEX  WINDOW *wgetparent(const WINDOW *);
PDCEX  int     wgetscrreg(const WINDOW *, int *, int *);
PDCEX  int     wresize(WINDOW *, int, int);

PDCEX  bool    has_mouse(void);
PDCEX  int     mouseinterval(int);
PDCEX  mmask_t mousemask(mmask_t, mmask_t *);
PDCEX  bool    mouse_trafo(int *, int *, bool);
PDCEX  int     nc_getmouse(MEVENT *);
PDCEX  int     ungetmouse(MEVENT *);
PDCEX  bool    wenclose(const WINDOW *, int, int);
PDCEX  bool    wmouse_trafo(const WINDOW *, int *, int *, bool);

/* PDCurses */

PDCEX  int     addrawch(chtype);
PDCEX  int     insrawch(chtype);
PDCEX  bool    is_termresized(void);
PDCEX  int     mvaddrawch(int, int, chtype);
PDCEX  int     mvdeleteln(int, int);
PDCEX  int     mvinsertln(int, int);
PDCEX  int     mvinsrawch(int, int, chtype);
PDCEX  int     mvwaddrawch(WINDOW *, int, int, chtype);
PDCEX  int     mvwdeleteln(WINDOW *, int, int);
PDCEX  int     mvwinsertln(WINDOW *, int, int);
PDCEX  int     mvwinsrawch(WINDOW *, int, int, chtype);
PDCEX  int     raw_output(bool);
PDCEX  int     resize_term(int, int);
PDCEX  WINDOW *resize_window(WINDOW *, int, int);
PDCEX  int     waddrawch(WINDOW *, chtype);
PDCEX  int     winsrawch(WINDOW *, chtype);
PDCEX  char    wordchar(void);

#ifdef PDC_WIDE
PDCEX  wchar_t *slk_wlabel(int);
#endif

PDCEX  bool    PDC_getcbreak(void);
PDCEX  bool    PDC_getecho(void);
PDCEX  void    PDC_debug(const char *, ...);
PDCEX  void    _tracef(const char *, ...);
PDCEX  void    PDC_get_version(PDC_VERSION *);
PDCEX  int     PDC_ungetch(int);
PDCEX  int     PDC_set_blink(bool);
PDCEX  int     PDC_set_bold(bool);
PDCEX  int     PDC_set_line_color(short);
PDCEX  void    PDC_set_title(const char *);
PDCEX  int     PDC_set_box_type( const int box_type);

PDCEX  int     PDC_clearclipboard(void);
PDCEX  int     PDC_freeclipboard(char *);
PDCEX  int     PDC_getclipboard(char **, long *);
PDCEX  int     PDC_setclipboard(const char *, long);

PDCEX  unsigned long PDC_get_key_modifiers(void);
PDCEX  int     PDC_return_key_modifiers(bool);
PDCEX  void    PDC_set_resize_limits( const int new_min_lines,
                               const int new_max_lines,
                               const int new_min_cols,
                               const int new_max_cols);

#define FUNCTION_KEY_SHUT_DOWN        0
#define FUNCTION_KEY_PASTE            1
#define FUNCTION_KEY_ENLARGE_FONT     2
#define FUNCTION_KEY_SHRINK_FONT      3
#define FUNCTION_KEY_CHOOSE_FONT      4
#define FUNCTION_KEY_ABORT            5
#define FUNCTION_KEY_COPY             6
#define PDC_MAX_FUNCTION_KEYS         7

PDCEX int     PDC_set_function_key( const unsigned function,
                              const int new_key);
PDCEX int     PDC_get_function_key( const unsigned function);

PDCEX void    PDC_set_window_resized_callback(void (*callback)(void));

PDCEX int     PDC_set_preferred_fontface( const wchar_t* fontface);
PDCEX void    PDC_set_color_intensify_enabled( bool enabled);
PDCEX void    PDC_set_default_menu_visibility(int visible);

PDCEX  WINDOW *Xinitscr(int, char **);

#ifdef XCURSES
PDCEX  void    XCursesExit(void);
PDCEX  int     sb_init(void);
PDCEX  int     sb_set_horz(int, int, int);
PDCEX  int     sb_set_vert(int, int, int);
PDCEX  int     sb_get_horz(int *, int *, int *);
PDCEX  int     sb_get_vert(int *, int *, int *);
PDCEX  int     sb_refresh(void);
#endif

/* NetBSD */

PDCEX  int     touchoverlap(const WINDOW *, WINDOW *);
PDCEX  int     underend(void);
PDCEX  int     underscore(void);
PDCEX  int     wunderend(WINDOW *);
PDCEX  int     wunderscore(WINDOW *);

/*** Functions defined as macros ***/

/* getch() and ungetch() conflict with some DOS libraries */

#define getch()            wgetch(stdscr)
#define ungetch(ch)        PDC_ungetch(ch)

#define COLOR_PAIR(n)      (((chtype)(n) << PDC_COLOR_SHIFT) & A_COLOR)
#define PAIR_NUMBER(n)     (((n) & A_COLOR) >> PDC_COLOR_SHIFT)

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

/* Deprecated */

#define PDC_save_key_modifiers(x)  (OK)
#define PDC_get_input_fd()         0

/* masks for PDC_set_box_type() */

#define PDC_BOX_DOUBLED_V        1
#define PDC_BOX_DOUBLED_H        2

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
#define PDC_KEY_MODIFIER_REPEAT  16

/* Bitflags for trace(), curses_trace(),  for ncurses compatibility.
Values were copied from ncurses.  Note that those involving terminfo,
termcap,  and TTY control bits are meaningless in PDCurses and will be
ignored.       */

#define TRACE_DISABLE   0x0000   /* turn off tracing */
#define TRACE_TIMES     0x0001   /* trace user and system times of updates */
#define TRACE_TPUTS     0x0002   /* trace tputs calls */
#define TRACE_UPDATE    0x0004   /* trace update actions, old & new screens */
#define TRACE_MOVE      0x0008   /* trace cursor moves and scrolls */
#define TRACE_CHARPUT   0x0010   /* trace all character outputs */
#define TRACE_ORDINARY  0x001F   /* trace all update actions */
#define TRACE_CALLS     0x0020   /* trace all curses calls */
#define TRACE_VIRTPUT   0x0040   /* trace virtual character puts */
#define TRACE_IEVENT    0x0080   /* trace low-level input processing */
#define TRACE_BITS      0x0100   /* trace state of TTY control bits */
#define TRACE_ICALLS    0x0200   /* trace internal/nested calls */
#define TRACE_CCALLS    0x0400   /* trace per-character calls */
#define TRACE_DATABASE  0x0800   /* trace read/write of terminfo/termcap data */
#define TRACE_ATTRS     0x1000   /* trace attribute updates */

#define TRACE_SHIFT         13   /* number of bits in the trace masks */
#define TRACE_MAXIMUM   ((1u << TRACE_SHIFT) - 1u) /* max tracing */

#ifdef __cplusplus
# ifndef PDC_PP98
#  undef bool
# endif
}
#endif

#endif  /* __PDCURSES__ */
