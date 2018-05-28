/* Public Domain Curses */

#include "pdcwin.h"
#include <tchar.h>
#include <stdint.h>
#include <assert.h>

/* COLOR_PAIR to attribute encoding table. */

static short *color_pair_indices = (short *)NULL;
COLORREF *pdc_rgbs = (COLORREF *)NULL;
static int menu_shown = 1;
static int min_lines = 25, max_lines = 25;
static int min_cols = 80, max_cols = 80;

#if defined( CHTYPE_LONG) && CHTYPE_LONG >= 2 && defined( PDC_WIDE)
    #define USING_COMBINING_CHARACTER_SCHEME
    int PDC_expand_combined_characters( const cchar_t c, cchar_t *added);  /* addch.c */
#endif

/* Some older versions of Microsoft C/C++ don't understand about
inlined functions.  Until we puzzle out which ones do and which
don't,  we'll just leave "inlined" functions as plain old static
functions.        */

#ifdef _MSC_VER
#define INLINE static
#else
#define INLINE static inline
#endif

static int keep_size_within_bounds( int *lines, int *cols);
INLINE int set_default_sizes_from_registry( const int n_cols, const int n_rows,
               const int xloc, const int yloc, const int menu_shown);
void PDC_transform_line_given_hdc( const HDC hdc, const int lineno,
                             int x, int len, const chtype *srcp);

#define N_COLORS 256

#ifdef A_OVERLINE
#define A_ALL_LINES (A_UNDERLINE | A_LEFTLINE | A_RIGHTLINE | A_OVERLINE | A_STRIKEOUT)
#else
#define A_ALL_LINES (A_UNDERLINE | A_LEFTLINE | A_RIGHTLINE)
#endif

   /* If PDC_MAX_MOUSE_BUTTONS is undefined,  it means the user hasn't     */
   /* gotten a current 'curses.h' in which five-button mice are supported. */
   /* To handle this gracefully,  we'll just fall back to three buttons.   */

#ifndef PDC_MAX_MOUSE_BUTTONS
   #define PDC_MAX_MOUSE_BUTTONS 3
#endif

#define VERTICAL_WHEEL_EVENT      PDC_MAX_MOUSE_BUTTONS
#define HORIZONTAL_WHEEL_EVENT   (PDC_MAX_MOUSE_BUTTONS + 1)

unsigned long pdc_key_modifiers = 0L;
int PDC_show_ctrl_alts = 0;

/* RR: Removed statis on next line */
bool PDC_bDone = FALSE;
static HWND originally_focussed_window;

int debug_printf( const char *format, ...)
{
    static bool debugging = TRUE;

    if( debugging)
    {
        const char *output_filename = getenv( "PDC_DEBUG");

        if( !output_filename)
            debugging = FALSE;       /* don't bother trying again */
        else
        {
            FILE *ofile = fopen( output_filename, "a");

            if( ofile)
            {
                va_list argptr;
                va_start( argptr, format);
                vfprintf( ofile, format, argptr);
                va_end( argptr);
                fclose( ofile);
            }
            else
            {
                printf( "Opening '%s' failed\n", output_filename);
                exit( 0);
                debugging = FALSE;       /* don't bother trying again */
            }
        }
    }
    return( 0);
}

HWND PDC_hWnd;
static int PDC_argc = 0;
static char **PDC_argv = NULL;

static void final_cleanup( void)
{
    debug_printf( "final_cleanup: SP = %p\n", SP);
    if (SP)
    {
        RECT rect;

        GetWindowRect( PDC_hWnd, &rect);
        set_default_sizes_from_registry( SP->cols, SP->lines,
                  rect.left, rect.top, menu_shown);
    }
    PDC_LOG(( "final_cleanup: freeing fonts\n"));
    PDC_transform_line( 0, 0, 0, NULL);  /* free any fonts */
    if( originally_focussed_window)
        SetForegroundWindow( originally_focussed_window);
    if( PDC_argc)
    {
        int i;

        for( i = 0; i < PDC_argc; i++)
            free( PDC_argv[i]);
        free( PDC_argv);
        PDC_argc = 0;
        PDC_argv = NULL;
    }
#ifdef USING_COMBINING_CHARACTER_SCHEME
    PDC_expand_combined_characters( 0, NULL);       /* free internal buffer */
#endif
    debug_printf( "reset foreground window\n");
}

void PDC_scr_close(void)
{
    PDC_LOG(("PDC_scr_close() - called\n"));
    final_cleanup( );
    PDC_bDone = TRUE;
}

/* NOTE that PDC_scr_free( ) is called only from delscreen( ),    */
/* which is rarely called.  It appears that most programs simply  */
/* rely on the memory getting freed when the program terminates.  */
/* It seems conceivable to me that we could get into some trouble */
/* here,  if SP is freed and NULLed,  but then accessed again,    */
/* possibly within the Win32a window thread.                      */

void PDC_scr_free(void)
{
    if (SP)
        free(SP);
    SP = (SCREEN *)NULL;

    if (color_pair_indices)
        free(color_pair_indices);
    color_pair_indices = (short *)NULL;

    if (pdc_rgbs)
        free(pdc_rgbs);
    pdc_rgbs = (COLORREF *)NULL;
}

int PDC_choose_a_new_font( void);                     /* pdcdisp.c */
void PDC_add_clipboard_to_key_queue( void);           /* pdckbd.c */

#define KEY_QUEUE_SIZE    30

       /* By default,  the PDC_shutdown_key[] array contains 0       */
       /* (i.e., there's no key that's supposed to be returned for   */
       /* exit handling), and 22 = Ctrl-V (i.e.,  hit Ctrl-V to      */
       /* paste text from the clipboard into the key queue);  then   */
       /* Ctl-= (enlarge font) and Ctl-Minus (decrease font);  then  */
       /* Ctl-, (select font from dialog).                           */

static int PDC_shutdown_key[PDC_MAX_FUNCTION_KEYS] = { 0, 22, CTL_EQUAL, CTL_MINUS,
                                 CTL_COMMA };
int PDC_n_rows, PDC_n_cols;
int PDC_cxChar, PDC_cyChar, PDC_key_queue_low = 0, PDC_key_queue_high = 0;
int PDC_key_queue[KEY_QUEUE_SIZE];

   /* If the following is true,  you can enter Unicode values by hitting */
   /* Alt and holding it down while typing the value of the character on */
   /* the numeric keypad (for decimal entry);  _or_ you can hit Alt-Padplus */
   /* and then enter a hex value,  while holding down the Alt key.  In   */
   /* either case,  when you release the Alt key,  the Unicode character */
   /* is added to the queue.  For hex entry,  0-9 can come either from   */
   /* the numeric keypad or the "usual" keyboard.                        */
bool PDC_allow_numpad_unicode = TRUE;
static int numpad_unicode_value = 0;

static void adjust_font_size( const int font_size_change);

static void add_key_to_queue( const int new_key)
{
    const int new_idx = ((PDC_key_queue_high + 1) % KEY_QUEUE_SIZE);
                /* This is usually 10,  but is set to 16 if the user   */
                /* hits ALT_PADPLUS and is about to enter a hex value: */
    static int unicode_radix = 10;

    if( PDC_allow_numpad_unicode)
    {
        int digit = -1;

        if( new_key >= ALT_PAD0 && new_key <= ALT_PAD9)
            digit = new_key - ALT_PAD0;
                  /* In hex Unicode entry,  you can enter digits on both */
                  /* the numeric and "standard" keyboards :              */
        if( unicode_radix == 16 && new_key >= ALT_0 && new_key <= ALT_9)
            digit = new_key - ALT_0;
        if( unicode_radix == 16 && new_key >= ALT_A && new_key <= ALT_F)
            digit = new_key - ALT_A + 10;
        if( digit >= 0)
        {
            numpad_unicode_value = numpad_unicode_value * unicode_radix + digit;
            return;
        }
        if( new_key == ALT_PADPLUS)
        {                            /* signal to begin hex Unicode entry */
            unicode_radix = 16;
            return;
        }
    }
    unicode_radix = 10;
    if( new_key && new_key == PDC_shutdown_key[FUNCTION_KEY_PASTE])
        PDC_add_clipboard_to_key_queue( );
    else if( new_key && new_key == PDC_shutdown_key[FUNCTION_KEY_ABORT])
        exit( -1);
    else if( new_key && new_key == PDC_shutdown_key[FUNCTION_KEY_ENLARGE_FONT])
        adjust_font_size( 1);
    else if( new_key && new_key == PDC_shutdown_key[FUNCTION_KEY_SHRINK_FONT])
        adjust_font_size( -1);
    else if( new_key && new_key == PDC_shutdown_key[FUNCTION_KEY_CHOOSE_FONT])
    {
        if( PDC_choose_a_new_font( ))
            adjust_font_size( 0);
    }
    else if( new_idx != PDC_key_queue_low)
    {
        PDC_key_queue[PDC_key_queue_high] = new_key;
        PDC_key_queue_high = new_idx;
    }
}

/************************************************************************
 *    Table for key code translation of function keys in keypad mode    *
 *    These values are for strict IBM keyboard compatibles only         *
 ************************************************************************/

typedef struct
{
    unsigned short normal;
    unsigned short shift;
    unsigned short control;
    unsigned short alt;
    unsigned short extended;
} KPTAB;


static const KPTAB kptab[] =
{
    {0,          0,         0,           0,          0   }, /* 0  */
    {0,          0,         0,           0,          0   }, /* 1   VK_LBUTTON */
    {0,          0,         0,           0,          0   }, /* 2   VK_RBUTTON */
    {CTL_PAUSE,  'a',       'b',         'c',        0   }, /* 3   VK_CANCEL  */
    {0,          0,         0,           0,          0   }, /* 4   VK_MBUTTON */
    {0,          0,         0,           0,          0   }, /* 5   */
    {0,          0,         0,           0,          0   }, /* 6   */
    {0,          0,         0,           0,          0   }, /* 7   */
    {0x08,       0x08,      0x7F,        ALT_BKSP,   0   }, /* 8   VK_BACK    */
    {0x09,       KEY_BTAB,  CTL_TAB,     ALT_TAB,    999 }, /* 9   VK_TAB     */
    {0,          0,         0,           0,          0   }, /* 10  */
    {0,          0,         0,           0,          0   }, /* 11  */
    {KEY_B2,     0x35,      CTL_PAD5,    ALT_PAD5,   0   }, /* 12  VK_CLEAR   */
    {0x0D,       0x0D,      CTL_ENTER,   ALT_ENTER,  1   }, /* 13  VK_RETURN  */
    {0,          0,         0,           0,          0   }, /* 14  */
    {0,          0,         0,           0,          0   }, /* 15  */
    {0,          0,         0,           0,          0   }, /* 16  VK_SHIFT   HANDLED SEPARATELY */
    {0,          0,         0,           0,          0   }, /* 17  VK_CONTROL HANDLED SEPARATELY */
    {0,          0,         0,           0,          0   }, /* 18  VK_MENU    HANDLED SEPARATELY */
    {KEY_PAUSE,  KEY_SPAUSE,CTL_PAUSE,   0,          0   }, /* 19  VK_PAUSE   */
    {0,          0,         0,           0,          0   }, /* 20  VK_CAPITAL HANDLED SEPARATELY */
    {0,          0,         0,           0,          0   }, /* 21  VK_HANGUL  */
    {0,          0,         0,           0,          0   }, /* 22  */
    {0,          0,         0,           0,          0   }, /* 23  VK_JUNJA   */
    {0,          0,         0,           0,          0   }, /* 24  VK_FINAL   */
    {0,          0,         0,           0,          0   }, /* 25  VK_HANJA   */
    {0,          0,         0,           0,          0   }, /* 26  */
    {0x1B,       0x1B,      0x1B,        ALT_ESC,    0   }, /* 27  VK_ESCAPE  */
    {0,          0,         0,           0,          0   }, /* 28  VK_CONVERT */
    {0,          0,         0,           0,          0   }, /* 29  VK_NONCONVERT */
    {0,          0,         0,           0,          0   }, /* 30  VK_ACCEPT  */
    {0,          0,         0,           0,          0   }, /* 31  VK_MODECHANGE */
    {0x20,       0x20,      0x20,        0x20,       0   }, /* 32  VK_SPACE   */
    {KEY_A3,     0x39,      CTL_PAD9,    ALT_PAD9,   3   }, /* 33  VK_PRIOR   */
    {KEY_C3,     0x33,      CTL_PAD3,    ALT_PAD3,   4   }, /* 34  VK_NEXT    */
    {KEY_C1,     0x31,      CTL_PAD1,    ALT_PAD1,   5   }, /* 35  VK_END     */
    {KEY_A1,     0x37,      CTL_PAD7,    ALT_PAD7,   6   }, /* 36  VK_HOME    */
    {KEY_B1,     0x34,      CTL_PAD4,    ALT_PAD4,   7   }, /* 37  VK_LEFT    */
    {KEY_A2,     0x38,      CTL_PAD8,    ALT_PAD8,   8   }, /* 38  VK_UP      */
    {KEY_B3,     0x36,      CTL_PAD6,    ALT_PAD6,   9   }, /* 39  VK_RIGHT   */
    {KEY_C2,     0x32,      CTL_PAD2,    ALT_PAD2,   10  }, /* 40  VK_DOWN    */
    {0,          0,         0,           0,          0   }, /* 41  VK_SELECT  */
    {0,          0,         0,           0,          0   }, /* 42  VK_PRINT   */
    {0,          0,         0,           0,          0   }, /* 43  VK_EXECUTE */
    {KEY_PRINTSCREEN, 0,    0,       ALT_PRINTSCREEN, 0  }, /* 44  VK_SNAPSHOT*/
    {PAD0,       0x30,      CTL_PAD0,    ALT_PAD0,   11  }, /* 45  VK_INSERT  */
    {PADSTOP,    0x2E,      CTL_PADSTOP, ALT_PADSTOP,12  }, /* 46  VK_DELETE  */
    {0,          0,         0,           0,          0   }, /* 47  VK_HELP    */
    {0x30,       0x29,      CTL_0,       ALT_0,      0   }, /* 48  */
    {0x31,       0x21,      CTL_1,       ALT_1,      0   }, /* 49  */
    {0x32,       0x40,      CTL_2,       ALT_2,      0   }, /* 50  */
    {0x33,       0x23,      CTL_3,       ALT_3,      0   }, /* 51  */
    {0x34,       0x24,      CTL_4,       ALT_4,      0   }, /* 52  */
    {0x35,       0x25,      CTL_5,       ALT_5,      0   }, /* 53  */
    {0x36,       0x5E,      CTL_6,       ALT_6,      0   }, /* 54  */
    {0x37,       0x26,      CTL_7,       ALT_7,      0   }, /* 55  */
    {0x38,       0x2A,      CTL_8,       ALT_8,      0   }, /* 56  */
    {0x39,       0x28,      CTL_9,       ALT_9,      0   }, /* 57  */
    {0,          0,         0,           0,          0   }, /* 58  */
    {0,          0,         0,           0,          0   }, /* 59  */
    {0,          0,         0,           0,          0   }, /* 60  */
    {0,          0,         0,           0,          0   }, /* 61  */
    {0,          0,         0,           0,          0   }, /* 62  */
    {0,          0,         0,           0,          0   }, /* 63  */
    {0,          0,         0,           0,          0   }, /* 64  */
    {0x61,       0x41,      0x01,        ALT_A,      0   }, /* 65  */
    {0x62,       0x42,      0x02,        ALT_B,      0   }, /* 66  */
    {0x63,       0x43,      0x03,        ALT_C,      0   }, /* 67  */
    {0x64,       0x44,      0x04,        ALT_D,      0   }, /* 68  */
    {0x65,       0x45,      0x05,        ALT_E,      0   }, /* 69  */
    {0x66,       0x46,      0x06,        ALT_F,      0   }, /* 70  */
    {0x67,       0x47,      0x07,        ALT_G,      0   }, /* 71  */
    {0x68,       0x48,      0x08,        ALT_H,      0   }, /* 72  */
    {0x69,       0x49,      0x09,        ALT_I,      0   }, /* 73  */
    {0x6A,       0x4A,      0x0A,        ALT_J,      0   }, /* 74  */
    {0x6B,       0x4B,      0x0B,        ALT_K,      0   }, /* 75  */
    {0x6C,       0x4C,      0x0C,        ALT_L,      0   }, /* 76  */
    {0x6D,       0x4D,      0x0D,        ALT_M,      0   }, /* 77  */
    {0x6E,       0x4E,      0x0E,        ALT_N,      0   }, /* 78  */
    {0x6F,       0x4F,      0x0F,        ALT_O,      0   }, /* 79  */
    {0x70,       0x50,      0x10,        ALT_P,      0   }, /* 80  */
    {0x71,       0x51,      0x11,        ALT_Q,      0   }, /* 81  */
    {0x72,       0x52,      0x12,        ALT_R,      0   }, /* 82  */
    {0x73,       0x53,      0x13,        ALT_S,      0   }, /* 83  */
    {0x74,       0x54,      0x14,        ALT_T,      0   }, /* 84  */
    {0x75,       0x55,      0x15,        ALT_U,      0   }, /* 85  */
    {0x76,       0x56,      0x16,        ALT_V,      0   }, /* 86  */
    {0x77,       0x57,      0x17,        ALT_W,      0   }, /* 87  */
    {0x78,       0x58,      0x18,        ALT_X,      0   }, /* 88  */
    {0x79,       0x59,      0x19,        ALT_Y,      0   }, /* 89  */
    {0x7A,       0x5A,      0x1A,        ALT_Z,      0   }, /* 90  */
    {0,          0,         0,           0,          0   }, /* 91  VK_LWIN    */
    {0,          0,         0,           0,          0   }, /* 92  VK_RWIN    */
    {KEY_APPS,   KEY_SAPPS, CTL_APPS,    ALT_APPS,   13  }, /* 93  VK_APPS    */
    {0,          0,         0,           0,          0   }, /* 94  */
    {0,          0,         0,           0,          0   }, /* 95  */
    {0x30,       0,         CTL_PAD0,    ALT_PAD0,   0   }, /* 96  VK_NUMPAD0 */
    {0x31,       0,         CTL_PAD1,    ALT_PAD1,   0   }, /* 97  VK_NUMPAD1 */
    {0x32,       0,         CTL_PAD2,    ALT_PAD2,   0   }, /* 98  VK_NUMPAD2 */
    {0x33,       0,         CTL_PAD3,    ALT_PAD3,   0   }, /* 99  VK_NUMPAD3 */
    {0x34,       0,         CTL_PAD4,    ALT_PAD4,   0   }, /* 100 VK_NUMPAD4 */
    {0x35,       0,         CTL_PAD5,    ALT_PAD5,   0   }, /* 101 VK_NUMPAD5 */
    {0x36,       0,         CTL_PAD6,    ALT_PAD6,   0   }, /* 102 VK_NUMPAD6 */
    {0x37,       0,         CTL_PAD7,    ALT_PAD7,   0   }, /* 103 VK_NUMPAD7 */
    {0x38,       0,         CTL_PAD8,    ALT_PAD8,   0   }, /* 104 VK_NUMPAD8 */
    {0x39,       0,         CTL_PAD9,    ALT_PAD9,   0   }, /* 105 VK_NUMPAD9 */
    {PADSTAR,   SHF_PADSTAR,CTL_PADSTAR, ALT_PADSTAR,999 }, /* 106 VK_MULTIPLY*/
    {PADPLUS,   SHF_PADPLUS,CTL_PADPLUS, ALT_PADPLUS,999 }, /* 107 VK_ADD     */
    {0,          0,         0,           0,          0   }, /* 108 VK_SEPARATOR     */
    {PADMINUS, SHF_PADMINUS,CTL_PADMINUS,ALT_PADMINUS,999}, /* 109 VK_SUBTRACT*/
    {0x2E,       0,         CTL_PADSTOP, ALT_PADSTOP,0   }, /* 110 VK_DECIMAL */
    {PADSLASH,  SHF_PADSLASH,CTL_PADSLASH,ALT_PADSLASH,2 }, /* 111 VK_DIVIDE  */
    {KEY_F(1),   KEY_F(13), KEY_F(25),   KEY_F(37),  0   }, /* 112 VK_F1      */
    {KEY_F(2),   KEY_F(14), KEY_F(26),   KEY_F(38),  0   }, /* 113 VK_F2      */
    {KEY_F(3),   KEY_F(15), KEY_F(27),   KEY_F(39),  0   }, /* 114 VK_F3      */
    {KEY_F(4),   KEY_F(16), KEY_F(28),   KEY_F(40),  0   }, /* 115 VK_F4      */
    {KEY_F(5),   KEY_F(17), KEY_F(29),   KEY_F(41),  0   }, /* 116 VK_F5      */
    {KEY_F(6),   KEY_F(18), KEY_F(30),   KEY_F(42),  0   }, /* 117 VK_F6      */
    {KEY_F(7),   KEY_F(19), KEY_F(31),   KEY_F(43),  0   }, /* 118 VK_F7      */
    {KEY_F(8),   KEY_F(20), KEY_F(32),   KEY_F(44),  0   }, /* 119 VK_F8      */
    {KEY_F(9),   KEY_F(21), KEY_F(33),   KEY_F(45),  0   }, /* 120 VK_F9      */
    {KEY_F(10),  KEY_F(22), KEY_F(34),   KEY_F(46),  0   }, /* 121 VK_F10     */
    {KEY_F(11),  KEY_F(23), KEY_F(35),   KEY_F(47),  0   }, /* 122 VK_F11     */
    {KEY_F(12),  KEY_F(24), KEY_F(36),   KEY_F(48),  0   }, /* 123 VK_F12     */

    /* 124 through 218 */

    {0, 0, 0, 0, 0},  /* 124 VK_F13 */
    {0, 0, 0, 0, 0},  /* 125 VK_F14 */
    {0, 0, 0, 0, 0},  /* 126 VK_F15 */
    {0, 0, 0, 0, 0},  /* 127 VK_F16 */
    {0, 0, 0, 0, 0},  /* 128 VK_F17 */
    {0, 0, 0, 0, 0},  /* 129 VK_F18 */
    {0, 0, 0, 0, 0},  /* 130 VK_F19 */
    {0, 0, 0, 0, 0},  /* 131 VK_F20 */
    {0, 0, 0, 0, 0},  /* 132 VK_F21 */
    {0, 0, 0, 0, 0},  /* 133 VK_F22 */
    {0, 0, 0, 0, 0},  /* 134 VK_F23 */
    {0, 0, 0, 0, 0},  /* 135 VK_F24 */
    {0, 0, 0, 0, 0},  /* 136 unassigned */
    {0, 0, 0, 0, 0},  /* 137 unassigned */
    {0, 0, 0, 0, 0},  /* 138 unassigned */
    {0, 0, 0, 0, 0},  /* 139 unassigned */
    {0, 0, 0, 0, 0},  /* 140 unassigned */
    {0, 0, 0, 0, 0},  /* 141 unassigned */
    {0, 0, 0, 0, 0},  /* 142 unassigned */
    {0, 0, 0, 0, 0},  /* 143 unassigned */
    {0, 0, 0, 0, 0},  /* 144 VK_NUMLOCK */
    {KEY_SCROLLLOCK, 0, 0, ALT_SCROLLLOCK, 0},    /* 145 VKSCROLL */
    {0, 0, 0, 0, 0},  /* 146 OEM specific */
    {0, 0, 0, 0, 0},  /* 147 OEM specific */
    {0, 0, 0, 0, 0},  /* 148 OEM specific */
    {0, 0, 0, 0, 0},  /* 149 OEM specific */
    {0, 0, 0, 0, 0},  /* 150 OEM specific */
    {0, 0, 0, 0, 0},  /* 151 Unassigned */
    {0, 0, 0, 0, 0},  /* 152 Unassigned */
    {0, 0, 0, 0, 0},  /* 153 Unassigned */
    {0, 0, 0, 0, 0},  /* 154 Unassigned */
    {0, 0, 0, 0, 0},  /* 155 Unassigned */
    {0, 0, 0, 0, 0},  /* 156 Unassigned */
    {0, 0, 0, 0, 0},  /* 157 Unassigned */
    {0, 0, 0, 0, 0},  /* 158 Unassigned */
    {0, 0, 0, 0, 0},  /* 159 Unassigned */
    {0, 0, 0, 0, 0},  /* 160 VK_LSHIFT */
    {0, 0, 0, 0, 0},  /* 161 VK_RSHIFT */
    {0, 0, 0, 0, 0},  /* 162 VK_LCONTROL */
    {0, 0, 0, 0, 0},  /* 163 VK_RCONTROL */
    {0, 0, 0, 0, 0},  /* 164 VK_LMENU */
    {0, 0, 0, 0, 0},  /* 165 VK_RMENU */
    {0, 0, 0, 0, 14},  /* 166 VK_BROWSER_BACK        */
    {0, 0, 0, 0, 15},  /* 167 VK_BROWSER_FORWARD     */
    {0, 0, 0, 0, 16},  /* 168 VK_BROWSER_REFRESH     */
    {0, 0, 0, 0, 17},  /* 169 VK_BROWSER_STOP        */
    {0, 0, 0, 0, 18},  /* 170 VK_BROWSER_SEARCH      */
    {0, 0, 0, 0, 19},  /* 171 VK_BROWSER_FAVORITES   */
    {0, 0, 0, 0, 20},  /* 172 VK_BROWSER_HOME        */
    {0, 0, 0, 0, 21},  /* 173 VK_VOLUME_MUTE         */
    {0, 0, 0, 0, 22},  /* 174 VK_VOLUME_DOWN         */
    {0, 0, 0, 0, 23},  /* 175 VK_VOLUME_UP           */
    {0, 0, 0, 0, 24},  /* 176 VK_MEDIA_NEXT_TRACK    */
    {0, 0, 0, 0, 25},  /* 177 VK_MEDIA_PREV_TRACK    */
    {0, 0, 0, 0, 26},  /* 178 VK_MEDIA_STOP          */
    {0, 0, 0, 0, 27},  /* 179 VK_MEDIA_PLAY_PAUSE    */
    {0, 0, 0, 0, 28},  /* 180 VK_LAUNCH_MAIL         */
    {0, 0, 0, 0, 29},  /* 181 VK_LAUNCH_MEDIA_SELECT */
    {0, 0, 0, 0, 30},  /* 182 VK_LAUNCH_APP1         */
    {0, 0, 0, 0, 31},  /* 183 VK_LAUNCH_APP2         */
    {0, 0, 0, 0, 0},  /* 184 Reserved */
    {0, 0, 0, 0, 0},  /* 185 Reserved */
    {';', ':', CTL_SEMICOLON, ALT_SEMICOLON, 0},  /* 186 VK_OEM_1      */
    {'=', '+', CTL_EQUAL,     ALT_EQUAL,     0},  /* 187 VK_OEM_PLUS   */
    {',', '<', CTL_COMMA,     ALT_COMMA,     0},  /* 188 VK_OEM_COMMA  */
    {'-', '_', CTL_MINUS,     ALT_MINUS,     0},  /* 189 VK_OEM_MINUS  */
    {'.', '>', CTL_STOP,      ALT_STOP,      0},  /* 190 VK_OEM_PERIOD */
    {'/', '?', CTL_FSLASH,    ALT_FSLASH,    0},  /* 191 VK_OEM_2      */
    {'`', '~', CTL_BQUOTE,    ALT_BQUOTE,    0},  /* 192 VK_OEM_3      */
    {0, 0, 0, 0, 0},  /* 193 */
    {0, 0, 0, 0, 0},  /* 194 */
    {0, 0, 0, 0, 0},  /* 195 */
    {0, 0, 0, 0, 0},  /* 196 */
    {0, 0, 0, 0, 0},  /* 197 */
    {0, 0, 0, 0, 0},  /* 198 */
    {0, 0, 0, 0, 0},  /* 199 */
    {0, 0, 0, 0, 0},  /* 200 */
    {0, 0, 0, 0, 0},  /* 201 */
    {0, 0, 0, 0, 0},  /* 202 */
    {0, 0, 0, 0, 0},  /* 203 */
    {0, 0, 0, 0, 0},  /* 204 */
    {0, 0, 0, 0, 0},  /* 205 */
    {0, 0, 0, 0, 0},  /* 206 */
    {0, 0, 0, 0, 0},  /* 207 */
    {0, 0, 0, 0, 0},  /* 208 */
    {0, 0, 0, 0, 0},  /* 209 */
    {0, 0, 0, 0, 0},  /* 210 */
    {0, 0, 0, 0, 0},  /* 211 */
    {0, 0, 0, 0, 0},  /* 212 */
    {0, 0, 0, 0, 0},  /* 213 */
    {0, 0, 0, 0, 0},  /* 214 */
    {0, 0, 0, 0, 0},  /* 215 */
    {0, 0, 0, 0, 0},  /* 216 */
    {0, 0, 0, 0, 0},  /* 217 */
    {0, 0, 0, 0, 0},  /* 218 */
    {0x5B,       0x7B,      0x1B,        ALT_LBRACKET,0  }, /* 219 VK_OEM_4 */
    {0x5C,       0x7C,      0x1C,        ALT_BSLASH, 0   }, /* 220 VK_OEM_5 */
    {0x5D,       0x7D,      0x1D,        ALT_RBRACKET,0  }, /* 221 VK_OEM_6 */
    {'\'',       '"',       0x27,        ALT_FQUOTE, 0   }, /* 222 VK_OEM_7 */
    {0,          0,         0,           0,          0   }, /* 223 VK_OEM_8 */
    {0,          0,         0,           0,          0   }, /* 224 */
    {0,          0,         0,           0,          0   }  /* 225 */
};
/* End of kptab[] */

static const KPTAB ext_kptab[] =
{
   {0,          0,              0,              0,          }, /*  0  MUST BE EMPTY */
   {PADENTER,   SHF_PADENTER,   CTL_PADENTER,   ALT_PADENTER}, /*  1  13 */
   {PADSLASH,   SHF_PADSLASH,   CTL_PADSLASH,   ALT_PADSLASH}, /*  2 111 */
   {KEY_PPAGE,  KEY_SPREVIOUS,  CTL_PGUP,       ALT_PGUP    }, /*  3  33 */
   {KEY_NPAGE,  KEY_SNEXT,      CTL_PGDN,       ALT_PGDN    }, /*  4  34 */
   {KEY_END,    KEY_SEND,       CTL_END,        ALT_END     }, /*  5  35 */
   {KEY_HOME,   KEY_SHOME,      CTL_HOME,       ALT_HOME    }, /*  6  36 */
   {KEY_LEFT,   KEY_SLEFT,      CTL_LEFT,       ALT_LEFT    }, /*  7  37 */
   {KEY_UP,     KEY_SUP,        CTL_UP,         ALT_UP      }, /*  8  38 */
   {KEY_RIGHT,  KEY_SRIGHT,     CTL_RIGHT,      ALT_RIGHT   }, /*  9  39 */
   {KEY_DOWN,   KEY_SDOWN,      CTL_DOWN,       ALT_DOWN    }, /* 10  40 */
   {KEY_IC,     KEY_SIC,        CTL_INS,        ALT_INS     }, /* 11  45 */
   {KEY_DC,     KEY_SDC,        CTL_DEL,        ALT_DEL     }, /* 12  46 */
   {KEY_APPS,   KEY_SAPPS     , CTL_APPS,       ALT_APPS    }, /* 13  93  VK_APPS    */
   {KEY_BROWSER_BACK, KEY_SBROWSER_BACK, KEY_CBROWSER_BACK, KEY_ABROWSER_BACK, }, /* 14 166 VK_BROWSER_BACK        */
   {KEY_BROWSER_FWD,  KEY_SBROWSER_FWD,  KEY_CBROWSER_FWD,  KEY_ABROWSER_FWD,  }, /* 15 167 VK_BROWSER_FORWARD     */
   {KEY_BROWSER_REF,  KEY_SBROWSER_REF,  KEY_CBROWSER_REF,  KEY_ABROWSER_REF,  }, /* 16 168 VK_BROWSER_REFRESH     */
   {KEY_BROWSER_STOP, KEY_SBROWSER_STOP, KEY_CBROWSER_STOP, KEY_ABROWSER_STOP, }, /* 17 169 VK_BROWSER_STOP        */
   {KEY_SEARCH,       KEY_SSEARCH,       KEY_CSEARCH,       KEY_ASEARCH,       }, /* 18 170 VK_BROWSER_SEARCH      */
   {KEY_FAVORITES,    KEY_SFAVORITES,    KEY_CFAVORITES,    KEY_AFAVORITES,    }, /* 19 171 VK_BROWSER_FAVORITES   */
   {KEY_BROWSER_HOME, KEY_SBROWSER_HOME, KEY_CBROWSER_HOME, KEY_ABROWSER_HOME, }, /* 20 172 VK_BROWSER_HOME        */
   {KEY_VOLUME_MUTE,  KEY_SVOLUME_MUTE,  KEY_CVOLUME_MUTE,  KEY_AVOLUME_MUTE,  }, /* 21 173 VK_VOLUME_MUTE         */
   {KEY_VOLUME_DOWN,  KEY_SVOLUME_DOWN,  KEY_CVOLUME_DOWN,  KEY_AVOLUME_DOWN,  }, /* 22 174 VK_VOLUME_DOWN         */
   {KEY_VOLUME_UP,    KEY_SVOLUME_UP,    KEY_CVOLUME_UP,    KEY_AVOLUME_UP,    }, /* 23 175 VK_VOLUME_UP           */
   {KEY_NEXT_TRACK,   KEY_SNEXT_TRACK,   KEY_CNEXT_TRACK,   KEY_ANEXT_TRACK,   }, /* 24 176 VK_MEDIA_NEXT_TRACK    */
   {KEY_PREV_TRACK,   KEY_SPREV_TRACK,   KEY_CPREV_TRACK,   KEY_APREV_TRACK,   }, /* 25 177 VK_MEDIA_PREV_TRACK    */
   {KEY_MEDIA_STOP,   KEY_SMEDIA_STOP,   KEY_CMEDIA_STOP,   KEY_AMEDIA_STOP,   }, /* 26 178 VK_MEDIA_STOP          */
   {KEY_PLAY_PAUSE,   KEY_SPLAY_PAUSE,   KEY_CPLAY_PAUSE,   KEY_APLAY_PAUSE,   }, /* 27 179 VK_MEDIA_PLAY_PAUSE    */
   {KEY_LAUNCH_MAIL,  KEY_SLAUNCH_MAIL,  KEY_CLAUNCH_MAIL,  KEY_ALAUNCH_MAIL,  }, /* 28 180 VK_LAUNCH_MAIL         */
   {KEY_MEDIA_SELECT, KEY_SMEDIA_SELECT, KEY_CMEDIA_SELECT, KEY_AMEDIA_SELECT, }, /* 29 181 VK_LAUNCH_MEDIA_SELECT */
   {KEY_LAUNCH_APP1,  KEY_SLAUNCH_APP1,  KEY_CLAUNCH_APP1,  KEY_ALAUNCH_APP1,  }, /* 30 182 VK_LAUNCH_APP1         */
   {KEY_LAUNCH_APP2,  KEY_SLAUNCH_APP2,  KEY_CLAUNCH_APP2,  KEY_ALAUNCH_APP2,  }, /* 31 183 VK_LAUNCH_APP2         */
};


HFONT PDC_get_font_handle( const int is_bold);            /* pdcdisp.c */

/* Mouse handling is done as follows:

   What we want is a setup wherein,  if the user presses and releases a
mouse button within SP->mouse_wait milliseconds,  there will be a
KEY_MOUSE issued through getch( ) and the "button state" for that button
will be set to BUTTON_CLICKED.

   If the user presses and releases the button,  and it takes _longer_
than SP->mouse_wait milliseconds,  then there should be a KEY_MOUSE
issued with the "button state" set to BUTTON_PRESSED.  Then,  later,
another KEY_MOUSE with a BUTTON_RELEASED.

   To accomplish this:  when a message such as WM_LBUTTONDOWN,
WM_RBUTTONDOWN,  or WM_MBUTTONDOWN is issued (and more recently WM_XBUTTONDOWN
for five-button mice),  we set up a timer with a period of SP->mouse_wait
milliseconds.  There are then two possibilities. The user will release the
button quickly (so it's a "click") or they won't (and it's a "press/release").

   In the first case,  we'll get the WM_xBUTTONUP message before the
WM_TIMER one.  We'll kill the timer and set up the BUTTON_CLICKED state. (*)

   In the second case,  we'll get the WM_TIMER message first,  so we'll
set the BUTTON_PRESSED state and kill the timer.  Eventually,  the user
will get around to letting go of the mouse button,  and we'll get that
WM_xBUTTONUP message.  At that time,  we'll set the BUTTON_RELEASED state
and add the second KEY_MOUSE to the key queue.

   Also,  note that if there is already a KEY_MOUSE to the queue,  there's
no point in adding another one.  At least at present,  the actual mouse
events aren't queued anyway.  So if there was,  say,  a click and then a
release without getch( ) being called in between,  you'd then have two
KEY_MOUSEs on the queue,  but would have lost all information about what
the first one actually was.  Hence the code near the end of this function
to ensure there isn't already a KEY_MOUSE in the queue.

   Also,  a note about wheel handling.  Pre-Vista,  you could just say
"the wheel went up" or "the wheel went down".  Vista introduced the possibility
that the mouse motion could be a smoothly varying quantity.  So on each
mouse move,  we add in the amount moved,  then check to see if that's
enough to trigger a wheel up/down event (or possibly several).  The idea
is that whereas before,  each movement would be 120 units (the default),
you might now get a series of 40-unit moves and should emit a wheel up/down
event on every third move.

   (*) Things are actually slightly more complicated than this.  In general,
it'll just be a plain old BUTTON_CLICKED state.  But if there was another
BUTTON_CLICKED within the last 2 * SP->mouse_wait milliseconds,  then this
must be a _double_ click,  so we set the BUTTON_DOUBLE_CLICKED state.  And
if,  within that time frame,  there was a double or triple click,  then we
set the BUTTON_TRIPLE_CLICKED state.  There isn't a "quad" or higher state,
so if you quadruple-click the mouse,  with each click separated by less
than 2 * SP->mouse_wait milliseconds,  then the messages sent will be
BUTTON_CLICKED,  BUTTON_DOUBLE_CLICKED,  BUTTON_TRIPLE_CLICKED,  and
then another BUTTON_TRIPLE_CLICKED.                                     */

static int set_mouse( const int button_index, const int button_state,
                           const LPARAM lParam)
{
    int i, n_key_mouse_to_add = 1;
    POINT pt;

    pt.x = LOWORD( lParam);
    pt.y = HIWORD( lParam);
    if( button_index == -1)         /* mouse moved,  no button */
        n_key_mouse_to_add = 1;
    else
    {
        memset(&pdc_mouse_status, 0, sizeof(MOUSE_STATUS));
        if( button_index < PDC_MAX_MOUSE_BUTTONS)
        {
            if( button_index < 3)
               {
               pdc_mouse_status.button[button_index] = (short)button_state;
               pdc_mouse_status.changes = (1 << button_index);
               }
            else
               {
               pdc_mouse_status.xbutton[button_index - 3] = (short)button_state;
               pdc_mouse_status.changes = (0x40 << button_index);
               }
        }
        else                      /* actually a wheel mouse movement */
        {                         /* button_state = number of units moved */
            static int mouse_wheel_vertical_loc = 0;
            static int mouse_wheel_horizontal_loc = 0;
            const int mouse_wheel_sensitivity = 120;

            n_key_mouse_to_add = 0;
            if( button_index == VERTICAL_WHEEL_EVENT)
            {
                mouse_wheel_vertical_loc += button_state;
                while( mouse_wheel_vertical_loc > mouse_wheel_sensitivity / 2)
                {
                    n_key_mouse_to_add++;
                    mouse_wheel_vertical_loc -= mouse_wheel_sensitivity;
                    pdc_mouse_status.changes |= PDC_MOUSE_WHEEL_UP;
                }
                while( mouse_wheel_vertical_loc < -mouse_wheel_sensitivity / 2)
                {
                    n_key_mouse_to_add++;
                    mouse_wheel_vertical_loc += mouse_wheel_sensitivity;
                    pdc_mouse_status.changes |= PDC_MOUSE_WHEEL_DOWN;
                }
             }
             else       /* must be a horizontal event: */
            {
                mouse_wheel_horizontal_loc += button_state;
                while( mouse_wheel_horizontal_loc > mouse_wheel_sensitivity / 2)
                {
                    n_key_mouse_to_add++;
                    mouse_wheel_horizontal_loc -= mouse_wheel_sensitivity;
                    pdc_mouse_status.changes |= PDC_MOUSE_WHEEL_RIGHT;
                }
                while( mouse_wheel_horizontal_loc < -mouse_wheel_sensitivity / 2)
                {
                    n_key_mouse_to_add++;
                    mouse_wheel_horizontal_loc += mouse_wheel_sensitivity;
                    pdc_mouse_status.changes |= PDC_MOUSE_WHEEL_LEFT;
                }
             }
                        /* I think it may be that for wheel events,  we   */
                        /* return x = y = -1,  rather than getting the    */
                        /* actual mouse position.  I don't like this, but */
                        /* I like messing up existing apps even less.     */
            pt.x = -PDC_cxChar;
            pt.y = -PDC_cyChar;
/*          ScreenToClient( PDC_hWnd, &pt);      Wheel posns are in screen, */
        }                         /* not client,  coords;  gotta xform them */
    }
    pdc_mouse_status.x = pt.x / PDC_cxChar;
    pdc_mouse_status.y = pt.y / PDC_cyChar;
/*  if( SP->save_key_modifiers)  */
    {
        int i, button_flags = 0;

        if( GetKeyState( VK_MENU) & 0x8000)
            button_flags |= PDC_BUTTON_ALT;

        if( GetKeyState( VK_SHIFT) & 0x8000)
            button_flags |= PDC_BUTTON_SHIFT;

        if( GetKeyState( VK_CONTROL) & 0x8000)
            button_flags |= PDC_BUTTON_CONTROL;

        for (i = 0; i < 3; i++)
            pdc_mouse_status.button[i] |= button_flags;
        for (i = 0; i < PDC_N_EXTENDED_MOUSE_BUTTONS; i++)
            pdc_mouse_status.xbutton[i] |= button_flags;
    }
                  /* If there is already a KEY_MOUSE in the queue,  we   */
                  /* don't really want to add another one.  See above.   */
    i = PDC_key_queue_low;
    while( i != PDC_key_queue_high)
    {
        if( PDC_key_queue[i] == KEY_MOUSE)
        {
            debug_printf( "Mouse key already in queue\n");
            return( 0);
        }
        i = (i + 1) % KEY_QUEUE_SIZE;
    }
                  /* If the window is maximized,  the click may occur just */
                  /* outside the "real" screen area.  If so,  we again     */
                  /* don't want to add a key to the queue:                 */
    if( pdc_mouse_status.x >= PDC_n_cols || pdc_mouse_status.y >= PDC_n_rows)
        n_key_mouse_to_add = 0;
                  /* OK,  there isn't a KEY_MOUSE already in the queue.   */
                  /* So we'll add one (or zero or more,  for wheel mice): */
    while( n_key_mouse_to_add--)
        add_key_to_queue( KEY_MOUSE);
    return( 0);
}

      /* The following should be #defined in 'winuser.h',  but such is */
      /* not always the case.  The following fixes the exceptions:     */
#ifndef WM_MOUSEWHEEL
    #define WM_MOUSEWHEEL                   0x020A
#endif
#ifndef WM_MOUSEHWHEEL
    #define WM_MOUSEHWHEEL                  0x020E
#endif
#ifndef WM_XBUTTONDOWN
    #define WM_XBUTTONDOWN                  0x020B
    #define WM_XBUTTONUP                    0x020C
#endif
#ifndef MK_XBUTTON1
    #define MK_XBUTTON1                     0x0020
    #define MK_XBUTTON2                     0x0040
#endif

#ifdef USE_FALLBACK_FONT
extern GLYPHSET *PDC_unicode_range_data;
#endif         /* #ifdef USE_FALLBACK_FONT */

int PDC_blink_state = 0;
#define TIMER_ID_FOR_BLINKING 0x2000

/* When first loading a font,  we use 'get_character_sizes' to briefly
load the (non-bold,  non-italic flavor of the) font,  get its height and
width,  and call GetFontUnicodeRanges to determine which characters are
actually available from that font.  That set of ranges is used so that,
when we come across characters not in the font,  we can switch to a
"fallback" font (Unifont,  most likely).   */

static void get_character_sizes( const HWND hwnd,
                           int *xchar_size, int *ychar_size)
{
    HFONT hFont = PDC_get_font_handle( 0);
    HFONT prev_font;
    HDC hdc = GetDC (hwnd) ;
    TEXTMETRIC tm ;
#ifdef USE_FALLBACK_FONT
    DWORD size;
#endif

    prev_font = SelectObject (hdc, hFont);
    GetTextMetrics (hdc, &tm) ;
#ifdef USE_FALLBACK_FONT
    assert( !PDC_unicode_range_data);
    size = GetFontUnicodeRanges( hdc, NULL);
    PDC_unicode_range_data = (GLYPHSET *)calloc( 1, size);
    PDC_unicode_range_data->cbThis = size;
    size = GetFontUnicodeRanges( hdc, PDC_unicode_range_data);
#endif         /* #ifdef USE_FALLBACK_FONT */
    SelectObject( hdc, prev_font);
    ReleaseDC (hwnd, hdc) ;
    DeleteObject( hFont);
    *xchar_size = tm.tmAveCharWidth ;
    *ychar_size = tm.tmHeight;
}

INLINE void sort_out_rect( RECT *rect)
{
    int temp;

    if( rect->left > rect->right)
    {
        temp = rect->right;
        rect->right = rect->left;
        rect->left = temp;
    }
    if( rect->top > rect->bottom)
    {
        temp = rect->bottom;
        rect->bottom = rect->top;
        rect->top = temp;
    }
}

static int rectangle_from_chars_to_pixels( RECT *rect)
{
    int rval = 1;

    if( rect->right == rect->left && rect->top == rect->bottom)
        rval = 0;
    sort_out_rect( rect);
    if( rect->top < 0)
        rval = 0;
    rect->right++;
    rect->bottom++;
    rect->left *= PDC_cxChar;
    rect->right *= PDC_cxChar;
    rect->top *= PDC_cyChar;
    rect->bottom *= PDC_cyChar;
    return( rval);
}

/* When updating the mouse rectangle,  you _could_ just remove the old one
and draw the new one.  But that sometimes caused flickering if the mouse
area was large.  In such cases,  it's better to determine what areas
actually changed,  and invert just those.  So the following checks to
see if two overlapping rectangles are being drawn (this is the norm)
and figures out the area that actually needs to be flipped.  It does
seem to decrease flickering to near-zero.                      */

static int PDC_selecting_rectangle = 1;

int PDC_find_ends_of_selected_text( const int line,
          const RECT *rect, int *x)
{
    int rval = 0, i;

    if( (rect->top - line) * (rect->bottom - line) <= 0
            && (rect->top != rect->bottom || rect->left != rect->right))
    {
        if( PDC_selecting_rectangle || rect->top == rect->bottom)
        {
            x[0] = min( rect->right, rect->left);
            x[1] = max( rect->right, rect->left);
            rval = 1;
        }
        else if( rect->top <= line && line <= rect->bottom)
        {
            x[0] = (line == rect->top ? rect->left : 0);
            x[1] = (line == rect->bottom ? rect->right : SP->cols - 1);
            rval = 1;
        }
        else if( rect->top >= line && line >= rect->bottom)
        {
            x[0] = (line == rect->bottom ? rect->right : 0);
            x[1] = (line == rect->top ? rect->left : SP->cols - 1);
            rval = 1;
        }
    }
    if( rval)
        for( i = 0; i < 2; i++)
           if( x[i] > SP->cols - 1)
               x[i] = SP->cols - 1;
    return( rval);
}

/* Called in only one place,  so let's inline it */

INLINE void show_mouse_rect( const HWND hwnd, RECT before, RECT after)
{
    if( before.top > -1 || after.top > -1)
        if( memcmp( &after, &before, sizeof( RECT)))
        {
            const HDC hdc = GetDC( hwnd) ;

            if( PDC_selecting_rectangle)
            {
                const int show_before = rectangle_from_chars_to_pixels( &before);
                const int show_after  = rectangle_from_chars_to_pixels( &after);

                if( show_before && show_after)
                {
                    RECT temp;

                    if( before.top < after.top)
                    {
                        temp = before;   before = after;  after = temp;
                    }
                    if( before.top < after.bottom && after.right > before.left
                                  && before.right > after.left)
                    {
                        const int tval = min( after.bottom, before.bottom);

                        temp = after;
                        temp.bottom = before.top;
                        InvertRect( hdc, &temp);

                        temp.top = temp.bottom;
                        temp.bottom = tval;
                        temp.right = max( after.right, before.right);
                        temp.left = min( after.right, before.right);
                        InvertRect( hdc, &temp);

                        temp.right = max( after.left, before.left);
                        temp.left = min( after.left, before.left);
                        InvertRect( hdc, &temp);

                        temp = (after.bottom > before.bottom ? after : before);
                        temp.top = tval;
                        InvertRect( hdc, &temp);
                    }
                }
                else if( show_before)
                    InvertRect( hdc, &before);
                else if( show_after)
                    InvertRect( hdc, &after);
            }
            else     /* _not_ selecting rectangle; selecting lines */
            {
                int line;

                for( line = 0; line < SP->lines; line++)
                {
                    int x[4], n_rects = 0, i;

                    n_rects = PDC_find_ends_of_selected_text( line, &before, x);
                    n_rects += PDC_find_ends_of_selected_text( line, &after, x + n_rects * 2);
                    if( n_rects == 2)
                        if( x[0] == x[2] && x[1] == x[3])
                            n_rects = 0;   /* Rects are same & will cancel */
                    for( i = 0; i < n_rects; i++)
                        {
                        RECT trect;

                        trect.left = x[i + i];
                        trect.right = x[i + i + 1];
                        trect.top = line;
                        trect.bottom = line;
                        rectangle_from_chars_to_pixels( &trect);
                        InvertRect( hdc, &trect);
                        }
                }
            }
            ReleaseDC( hwnd, hdc) ;
        }
}

/* Cygwin lacks _splitpath, _wsplitpath.  THE FOLLOWING ARE NOT FULLY
TESTED IMPLEMENTATIONS OF THOSE TWO FUNCTIONS,  because the only use we
make of them is to get fname.  (Though I did write a little test program,
and they seem to work.) */

#ifdef __CYGWIN__
#ifdef PDC_WIDE
static void my_wsplitpath( const wchar_t *path, wchar_t *drive,
           wchar_t *dir, wchar_t *fname, wchar_t *ext)
{
    size_t i, loc = 0;

    assert( path);
    assert( fname);
    if( path[0] && path[1] == ':')
    {
        if( drive)
        {
            drive[0] = path[0];
            drive[1] = ':';
            drive[2] = '\0';
        }
        path += 2;
    }
    else if( drive)
        *drive = '\0';
    for( i = 0; path[i]; i++)
        if( path[i] == '/' || path[i] == '\\')
            loc = i + 1;
    if( dir)
    {
        memcpy( dir, path, loc * sizeof( wchar_t));
        dir[loc] = '\0';
    }
    if( loc)
        path += loc;
    loc = 0;
    while( path[loc] && path[loc] != '.')
        loc++;
    if( fname)
    {
        memcpy( fname, path, loc * sizeof( wchar_t));
        fname[loc] = '\0';
    }
    if( ext)
        wcscpy( ext, path + loc);
}
#endif            /* #ifdef PDC_WIDE */

static void my_splitpath( const char *path, char *drive,
           char *dir, char *fname, char *ext)
{
    size_t i, loc = 0;

    assert( path);
    assert( fname);
    if( path[0] && path[1] == ':')
    {
        if( drive)
        {
            drive[0] = path[0];
            drive[1] = ':';
            drive[2] = '\0';
        }
        path += 2;
    }
    else if( drive)
        *drive = '\0';
    for( i = 0; path[i]; i++)
        if( path[i] == '/' || path[i] == '\\')
            loc = i + 1;
    if( dir)
    {
        memcpy( dir, path, loc * sizeof( char));
        dir[loc] = '\0';
    }
    if( loc)
        path += loc;
    loc = 0;
    while( path[loc] && path[loc] != '.')
        loc++;
    if( fname)
    {
        memcpy( fname, path, loc * sizeof( char));
        fname[loc] = '\0';
    }
    if( ext)
        strcpy( ext, path + loc);
}
#else          /* non-Cygwin case : */
   #define my_splitpath  _splitpath
   #define my_wsplitpath _wsplitpath
   #define GOT_ARGV_ARGC
#endif            /* #ifdef __CYGWIN__ */

/* This function looks at the full command line,  which includes a fully
specified path to the executable and arguments;  and strips out just the
name of the app,  with the arguments optionally appended.  Hence,

C:\PDCURSES\WIN32A\TESTCURS.EXE arg1 arg2

    would be reduced to 'Testcurs' (if include_args == 0) or
'Testcurs arg1 arg2' (if include_args == 1).  The former case is used to
create a (hopefully unique) registry key for the app,  so that the app's
specific settings (screen and font size) will be stored for the next run.
The latter case is used to generate a default window title.

   Unfortunately,  this code has to do some pretty strange things.  In the
Unicode (PDC_WIDE) case,  we really should use __wargv;  but that pointer
may or may not be NULL.  If it's NULL, we fall back on __argv.  In at
least one case,  where this code is compiled into a DLL using MinGW and
then used in an app compiled with MS Visual C, __argv isn't set either,
and we drop back to looking at GetCommandLine( ).  Which leads to a real
oddity:  GetCommandLine( ) may return something such as,  say,

"C:\PDCurses\Win32a\testcurs.exe" -lRussian

   ...which,  after being run through _splitpath or _wsplitpath,  becomes

testcurs.exe" -lRussian

   The .exe" is removed,  and the command-line arguments shifted or removed,
depending on the value of include_args.  Pretty strange stuff.

   However,  if one calls Xinitscr( ) and passed command-line arguments when
starting this library,  those arguments will be stored in PDC_argc and
PDC_argv,  and will be used instead of GetCommandLine.
*/

#ifdef UNICODE
   #define my_stprintf wsprintf
   #define my_tcslen   wcslen
#ifdef __CYGWIN__
                     /* Can't lowercase Unicode text in Cygwin */
   #define my_tcslwr
#elif defined _MSC_VER
   #define my_tcslwr   _wcslwr
#else
   #define my_tcslwr   wcslwr
#endif      /* __CYGWIN__ */
   #define my_tcscat   wcscat
   #define my_tcscpy   wcscpy
   #define my_stscanf  swscanf

#else /* UNICODE */

   #define my_stprintf sprintf
   #define my_tcslen   strlen
   #define my_tcslwr   strlwr
#ifdef _MSC_VER
   #define strlwr     _strlwr
#endif
   #define my_tcscat   strcat
   #define my_tcscpy   strcpy
   #define my_stscanf  sscanf
#endif /* UNICODE */


static void get_app_name( TCHAR *buff, const bool include_args)
{
    int i;
#ifdef GOT_ARGV_ARGC
    int argc = (PDC_argc ? PDC_argc : __argc);
    char **argv = (PDC_argc ? PDC_argv : __argv);
#else
    int argc = PDC_argc;
    char **argv = PDC_argv;
#endif

#ifdef PDC_WIDE
#ifdef GOT_ARGV_ARGC
    if( __wargv)
    {
        my_wsplitpath( __wargv[0], NULL, NULL, buff, NULL);
        if( include_args)
            for( i = 1; i < __argc; i++)
            {
                wcscat( buff, L" ");
                wcscat( buff, __wargv[i]);
            }
    }
    else
#endif      /* #ifdef GOT_ARGV_ARGC */
       if( argv)
    {
        char tbuff[MAX_PATH];

        my_splitpath( argv[0], NULL, NULL, tbuff, NULL);
        if( include_args)
            for( i = 1; i < argc; i++)
            {
                strcat( tbuff, " ");
                strcat( tbuff, argv[i]);
            }
        mbstowcs( buff, tbuff, strlen( tbuff) + 1);
    }
    else         /* no __argv or PDC_argv pointer available */
    {
        wchar_t *tptr;

        my_wsplitpath( GetCommandLine( ), NULL, NULL, buff, NULL);
        my_tcslwr( buff + 1);
        tptr = wcsstr( buff, L".exe\"");
        if( tptr)
        {
            if( include_args)
                memmove( tptr, tptr + 5, wcslen( tptr + 4) * sizeof( wchar_t));
            else
                *tptr = '\0';
        }
    }
#else       /* non-Unicode case */
    if( argv)
    {
        my_splitpath( argv[0], NULL, NULL, buff, NULL);
        debug_printf( "Path: %s;  exe: %s\n", argv[0], buff);
        if( include_args)
            for( i = 1; i < argc; i++)
            {
                strcat( buff, " ");
                strcat( buff, argv[i]);
            }
    }
    else         /* no __argv pointer available */
    {
        char *tptr;

        my_splitpath( GetCommandLine( ), NULL, NULL, buff, NULL);
        strlwr( buff + 1);
        tptr = strstr( buff, ".exe\"");
        if( tptr)
        {
            if( include_args)
                memmove( tptr, tptr + 5, strlen( tptr + 4));
            else
                *tptr = '\0';
        }
    }
#endif
    my_tcslwr( buff + 1);
}

/* This function extracts the first icon from the executable that is
executing this DLL */

INLINE HICON get_app_icon( )
{
#ifdef PDC_WIDE
    wchar_t filename[MAX_PATH];
#else
    char filename[MAX_PATH];
#endif

    HICON icon = NULL;
    if ( GetModuleFileName( NULL, filename, sizeof(filename) ) != 0 )
       icon = ExtractIcon( 0, filename, 0 );
    return icon;
}

extern TCHAR PDC_font_name[];

/* This flavor of Curses tries to store the window and font sizes on
an app-by-app basis.  To do this,  it uses the above get_app_name( )
function,  then sets or gets a corresponding value from the Windoze
registry.  The benefit should be that one can have one screen size/font
for,  say,  Testcurs,  while having different settings for,  say,  Firework
or Rain or one's own programs.  */

INLINE int set_default_sizes_from_registry( const int n_cols, const int n_rows,
               const int xloc, const int yloc, const int menu_shown)
{
    DWORD is_new_key;
    HKEY hNewKey;
    long rval = RegCreateKeyEx( HKEY_CURRENT_USER, _T( "SOFTWARE\\PDCurses"),
                            0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS,
                            0, &hNewKey, &is_new_key);

    if( rval == ERROR_SUCCESS)
    {
        TCHAR buff[180];
        TCHAR key_name[MAX_PATH];
        extern int PDC_font_size;

        if( IsZoomed( PDC_hWnd))    /* -1x-1 indicates a maximized window */
            my_stprintf( buff,
                      _T( "-1x-1,%d,0,0,%d"), PDC_font_size, menu_shown);
        else
            my_stprintf( buff,
                      _T( "%dx%d,%d,%d,%d,%d"), n_cols, n_rows, PDC_font_size,
                       xloc, yloc, menu_shown);
        my_stprintf( buff + my_tcslen( buff),
                  _T(";%d,%d,%d,%d:"),
                  min_lines, max_lines,
                  min_cols, max_cols);
        my_tcscat( buff, PDC_font_name);

        get_app_name( key_name, FALSE);
        rval = RegSetValueEx( hNewKey, key_name, 0, REG_SZ,
                       (BYTE *)buff, (DWORD)( my_tcslen( buff) * sizeof( TCHAR)));
        RegCloseKey( hNewKey);
    }
    debug_printf( "Size: %d %d; %d\n", n_cols, n_rows, rval);
    return( rval != ERROR_SUCCESS);
}

/* If the window is maximized,  there will usually be a fractional
character width at the right and bottom edges.  The following code fills
that in with a black brush.  It takes the "paint rectangle",  the area
passed with a WM_PAINT message that specifies what chunk of the client
area needs to be redrawn.

    If the window is _not_ maximized,  this shouldn't happen;  the window
width/height should always be an integral multiple of the character
width/height,  with no slivers at the right and bottom edges. */

static void fix_up_edges( const HDC hdc, const RECT *rect)
{
    const int x = PDC_n_cols * PDC_cxChar;
    const int y = PDC_n_rows * PDC_cyChar;

    if( rect->right >= x || rect->bottom >= y)
    {
        const HBRUSH hOldBrush =
                      SelectObject( hdc, GetStockObject( BLACK_BRUSH));

        SelectObject( hdc, GetStockObject( NULL_PEN));
        if( rect->right >= x)
           Rectangle( hdc, x, rect->top, rect->right + 1, rect->bottom + 1);
        if( rect->bottom >= y)
           Rectangle( hdc, rect->left, y, rect->right + 1, rect->bottom + 1);
        SelectObject( hdc, hOldBrush);
    }
}

static void adjust_font_size( const int font_size_change)
{
    extern int PDC_font_size;

    PDC_font_size += font_size_change;
    if( PDC_font_size < 2)
        PDC_font_size = 2;
    PDC_transform_line( 0, 0, 0, NULL);    /* free any fonts  */
    get_character_sizes( PDC_hWnd, &PDC_cxChar, &PDC_cyChar);
          /* When the font size changes,  do we want to keep  */
          /* the window the same size (except to remove any   */
          /* fractional character)?  Or do we keep the number */
          /* of rows/columns the same?  For the nonce,  I'm   */
          /* keeping the window size fixed if the window is   */
          /* maximized,  but keeping the number of rows/lines */
          /* fixed if it's windowed.  That's my opinion.  If  */
          /* you disagree,  I have others.                    */
    if( IsZoomed( PDC_hWnd))
    {
        RECT client_rect;
        HDC hdc;

        GetClientRect( PDC_hWnd, &client_rect);
        PDC_n_rows = client_rect.bottom / PDC_cyChar;
        PDC_n_cols = client_rect.right / PDC_cxChar;
        keep_size_within_bounds( &PDC_n_rows, &PDC_n_cols);
        PDC_resize_screen( PDC_n_rows, PDC_n_cols);
        add_key_to_queue( KEY_RESIZE);
        SP->resized = TRUE;
        hdc = GetDC (PDC_hWnd) ;
        GetClientRect( PDC_hWnd, &client_rect);
        fix_up_edges( hdc, &client_rect);
        ReleaseDC( PDC_hWnd, hdc) ;
    }
    else
    {
        PDC_resize_screen( PDC_n_rows, PDC_n_cols);
        InvalidateRect( PDC_hWnd, NULL, FALSE);
    }
}

         /* PDC_mouse_rect is the area currently highlit by dragging the */
         /* mouse.  It's global,  sadly,  because we need to ensure that */
         /* the highlighting is respected when the text within that      */
         /* rectangle is redrawn by PDC_transform_line(). */
RECT PDC_mouse_rect = { -1, -1, -1, -1 };

int PDC_setclipboard_raw( const char *contents, long length,
            const bool translate_multibyte_to_wide_char);

/* Called in only one place (when the left mouse button goes up), */
/* so we should inline it :   */

INLINE void HandleBlockCopy( void)
{
    int i, j, len, x[2];
    TCHAR *buff, *tptr;

            /* Make a first pass to determine how much text is blocked: */
    for( i = len = 0; i < SP->lines; i++)
        if( PDC_find_ends_of_selected_text( i, &PDC_mouse_rect, x))
            len += x[1] - x[0] + 3;
    buff = tptr = (TCHAR *)malloc( (len + 1) * sizeof( TCHAR));
            /* Make second pass to copy that text to a buffer: */
    for( i = len = 0; i < SP->lines; i++)
        if( PDC_find_ends_of_selected_text( i, &PDC_mouse_rect, x))
        {
            const chtype *cptr = curscr->_y[i];

            for( j = 0; j < x[1] - x[0] + 1; j++)
                tptr[j] = (TCHAR)cptr[j + x[0]];
            while( j > 0 && tptr[j - 1] == ' ')
                j--;          /* remove trailing spaces */
            tptr += j;
            *tptr++ = (TCHAR)13;
            *tptr++ = (TCHAR)10;
        }
    if( tptr != buff)   /* at least one line read in */
    {
       tptr[-2] = '\0';       /* cut off the last CR/LF */
       PDC_setclipboard_raw( (char *)buff, (long)( tptr - buff), FALSE);
    }
    free( buff);
}

#define WM_ENLARGE_FONT       (WM_USER + 1)
#define WM_SHRINK_FONT        (WM_USER + 2)
#define WM_MARK_AND_COPY      (WM_USER + 3)
#define WM_TOGGLE_MENU        (WM_USER + 4)
#define WM_EXIT_GRACELESSLY   (WM_USER + 5)
#define WM_CHOOSE_FONT        (WM_USER + 6)

static int add_resize_key = 1;

/*man-start**************************************************************

Resize limits
-------------

### Synopsis

    void PDC_set_resize_limits( const int new_min_lines,
                                const int new_max_lines,
                                const int new_min_cols,
                                const int new_max_cols);

### Description

   For platforms supporting resizable windows (SDLx, Win32a, X11).  Some
   programs may be unprepared for a resize event;  for these,  calling
   this function with the max and min limits equal ensures that no
   user resizing can be done.  Other programs may require at least a
   certain number,  and/or no more than a certain number,  of columns
   and/or lines.

### Portability

   PDCurses-only function.

**man-end****************************************************************/

void PDC_set_resize_limits( const int new_min_lines, const int new_max_lines,
                  const int new_min_cols, const int new_max_cols)
{
    min_lines = max( new_min_lines, 2);
    max_lines = max( new_max_lines, min_lines);
    min_cols = max( new_min_cols, 2);
    max_cols = max( new_max_cols, min_cols);
}

      /* The screen should hold the characters (PDC_cxChar * n_default_columns */
      /* pixels wide,  similarly high).  In width,  we need two frame widths,  */
      /* one on each side.  Vertically,  we need two frame heights,  plus room */
      /* for the application title and the menu.  */

static void adjust_window_size( int *xpixels, int *ypixels, int window_style,
               const int menu_shown)
{
    RECT rect;

    rect.left = rect.top = 0;
    rect.right = *xpixels;
    rect.bottom = *ypixels;
/*  printf( "Adjusting to %d, %d\n", *xpixels, *ypixels); */
    AdjustWindowRect( &rect, window_style, menu_shown);
    *xpixels = rect.right - rect.left;
    *ypixels = rect.bottom - rect.top;
}

static int keep_size_within_bounds( int *lines, int *cols)
{
    int rval = 0;

    if( *lines < min_lines)
    {
        *lines = min_lines;
        rval = 1;
    }
    else if( *lines > max_lines)
    {
        *lines = max_lines;
        rval = 2;
    }
    if( *cols < min_cols)
    {
        *cols = min_cols;
        rval |= 4;
    }
    else if( *cols > max_cols)
    {
        *cols = max_cols;
        rval |= 8;
    }
    return( rval);
}

INLINE int get_default_sizes_from_registry( int *n_cols, int *n_rows,
                                     int *xloc, int *yloc, int *menu_shown)
{
    TCHAR data[100];
    DWORD size_out = sizeof( data);
    HKEY hKey = 0;
    long rval = RegOpenKeyEx( HKEY_CURRENT_USER, _T( "SOFTWARE\\PDCurses"),
                        0, KEY_READ, &hKey);

    if( !hKey)
        return( 1);
    if( rval == ERROR_SUCCESS)
    {
        TCHAR key_name[MAX_PATH];

        get_app_name( key_name, FALSE);
        rval = RegQueryValueEx( hKey, key_name,
                        NULL, NULL, (BYTE *)data, &size_out);
        if( rval == ERROR_SUCCESS)
        {
            extern int PDC_font_size;
            int x = -1, y = -1, bytes_read = 0;

            my_stscanf( data, _T( "%dx%d,%d,%d,%d,%d;%d,%d,%d,%d:%n"),
                             &x, &y, &PDC_font_size,
                             xloc, yloc, menu_shown,
                             &min_lines, &max_lines,
                             &min_cols, &max_cols,
                             &bytes_read);
            if( bytes_read > 0 && data[bytes_read - 1] == ':')
               my_tcscpy( PDC_font_name, data + bytes_read);
            if( n_cols)
                *n_cols = x;
            if( n_rows)
                *n_rows = y;
            if( *n_cols > 0 && *n_rows > 0)   /* i.e.,  not maximized */
                keep_size_within_bounds( n_rows, n_cols);
        }
        RegCloseKey( hKey);
    }
    if( rval != ERROR_SUCCESS)
        debug_printf( "get_default_sizes_from_registry error: %d\n", rval);
    return( rval != ERROR_SUCCESS);
}

/* Ensure that the dragged rectangle    */
/* is an even multiple of the char size */
INLINE void HandleSizing( WPARAM wParam, LPARAM lParam )
{
    RECT *rect = (RECT *)lParam;
    RECT window_rect, client_rect;
    int hadd, vadd, width, height;
    int n_rows, n_cols;
    int rounded_width, rounded_height;

    GetWindowRect( PDC_hWnd, &window_rect);
    GetClientRect( PDC_hWnd, &client_rect);
    hadd = (window_rect.right - window_rect.left) - client_rect.right;
    vadd = (window_rect.bottom - window_rect.top) - client_rect.bottom;
    width = rect->right - rect->left - hadd;
    height = rect->bottom - rect->top - vadd;

    n_cols = (width + PDC_cxChar / 2) / PDC_cxChar;
    n_rows = (height + PDC_cyChar / 2) / PDC_cyChar;
    keep_size_within_bounds( &n_rows, &n_cols);

    rounded_width = hadd + n_cols * PDC_cxChar;
    rounded_height = vadd + n_rows * PDC_cyChar;

    if( wParam == WMSZ_BOTTOM || wParam == WMSZ_BOTTOMLEFT
                              || wParam == WMSZ_BOTTOMRIGHT)
        rect->bottom = rect->top + rounded_height;
    if( wParam == WMSZ_TOP || wParam == WMSZ_TOPLEFT
                           || wParam == WMSZ_TOPRIGHT)
        rect->top = rect->bottom - rounded_height;
    if( wParam == WMSZ_RIGHT || wParam == WMSZ_BOTTOMRIGHT
                             || wParam == WMSZ_TOPRIGHT)
        rect->right = rect->left + rounded_width;
    if( wParam == WMSZ_LEFT || wParam == WMSZ_BOTTOMLEFT
                            || wParam == WMSZ_TOPLEFT)
        rect->left = rect->right - rounded_width;
}

/* Under Wine,  it appears that the code to force the window size to be
an integral number of columns and rows doesn't work.  This is because
WM_SIZING messages aren't sent (this is apparently fixed as of Wine 1.7.18,
though I've not tried it yet;  I'm still on Wine 1.6,  the stable branch.)
You can therefore end up in a loop where the code keeps trying to resize a
window that isn't actually resizing.  So,  _when running in Wine only_,
we want that code not to be executed... which means having to figure out:
are we running under Wine?  Which means that when PDCurses/Win32a is
initialized,  we set the following 'wine_version' pointer.  One could
actually call wine_version(),  if not NULL,  to get the current Wine
version.      */

typedef const char *(CDECL *wine_version_func)(void);

static wine_version_func wine_version;

static void HandleSize( const WPARAM wParam, const LPARAM lParam)
{
    static WPARAM prev_wParam = (WPARAM)-99;
    const unsigned n_xpixels = LOWORD (lParam);
    const unsigned n_ypixels = HIWORD (lParam);
    unsigned new_n_rows, new_n_cols;

    debug_printf( "WM_SIZE: wParam %x %d %d %d\n", (unsigned)wParam,
                  n_xpixels, n_ypixels, SP->resized);
/*  if( wine_version)
        printf( "Wine version: %s\n", wine_version( ));  */


    if( wParam == SIZE_MINIMIZED )
    {
        prev_wParam = SIZE_MINIMIZED;
        return;
    }
    new_n_rows = n_ypixels / PDC_cyChar;
    new_n_cols = n_xpixels / PDC_cxChar;
    debug_printf( "Size was %d x %d; will be %d x %d\n",
                PDC_n_rows, PDC_n_cols, new_n_rows, new_n_cols);
    SP->resized = FALSE;

    /* If the window will have a different number of rows */
    /* or columns,  we put KEY_RESIZE in the key queue.   */
    /* We don't do this if */
    /* the resizing is the result of the window being    */
    /* initialized,  or as a result of PDC_resize_screen */
    /* being called.  In the latter case,  the user      */
    /* presumably already knows the screen's been resized. */
    if( PDC_n_rows != (int)new_n_rows || PDC_n_cols != (int)new_n_cols)
    {
        PDC_n_cols = new_n_cols;
        PDC_n_rows = new_n_rows;
        debug_printf( "prev_wParam = %d; add_resize_key = %d\n",
                      (int)prev_wParam, add_resize_key);
        if( prev_wParam != (WPARAM)-99 && add_resize_key)
        {
            /* don't add a key when the window is initialized */
            add_key_to_queue( KEY_RESIZE);
            SP->resized = TRUE;
        }
    }
    else if( wine_version)
        return;

    add_resize_key = 1;
    if( wParam == SIZE_RESTORED &&
        ( n_xpixels % PDC_cxChar || n_ypixels % PDC_cyChar))
    {
        int new_xpixels = PDC_cxChar * PDC_n_cols;
        int new_ypixels = PDC_cyChar * PDC_n_rows;

        adjust_window_size( &new_xpixels, &new_ypixels,
                            GetWindowLong( PDC_hWnd, GWL_STYLE), menu_shown);
        debug_printf( "Irregular size\n");
        SetWindowPos( PDC_hWnd, 0, 0, 0,
                      new_xpixels, new_ypixels,
                      SWP_NOMOVE | SWP_NOZORDER | SWP_SHOWWINDOW);

    }

    /* If the window has been restored from minimized form, */
    /* we should repaint.  Otherwise,  don't.               */
    prev_wParam = wParam;
}

static void HandleMouseMove( WPARAM wParam, LPARAM lParam,
                      int* ptr_modified_key_to_return )
{
    const int mouse_x = LOWORD( lParam) / PDC_cxChar;
    const int mouse_y = HIWORD( lParam) / PDC_cyChar;
    static int prev_mouse_x, prev_mouse_y;

    if( mouse_x != prev_mouse_x || mouse_y != prev_mouse_y)
    {
        int report_event = 0;

        prev_mouse_x = mouse_x;
        prev_mouse_y = mouse_y;
        if( wParam & MK_LBUTTON)
        {
            PDC_mouse_rect.left = mouse_x;
            PDC_mouse_rect.top = mouse_y;
            if( SP->_trap_mbe & BUTTON1_MOVED)
                report_event |= PDC_MOUSE_MOVED | 1;
        }
        if( wParam & MK_MBUTTON)
            if( SP->_trap_mbe & BUTTON2_MOVED)
                report_event |= PDC_MOUSE_MOVED | 2;
        if( wParam & MK_RBUTTON)
            if( SP->_trap_mbe & BUTTON3_MOVED)
                report_event |= PDC_MOUSE_MOVED | 4;

#ifdef CANT_DO_THINGS_THIS_WAY
         /* Logic would dictate the following lines.  But with PDCurses */
         /* as it's currently set up,  we've run out of bits and there  */
         /* is no BUTTON4_MOVED or BUTTON5_MOVED.  Perhaps we need to   */
         /* redefine _trap_mbe to be a 64-bit quantity?                 */
            if( wParam & MK_XBUTTON1)
                if( SP->_trap_mbe & BUTTON4_MOVED)
                    report_event |= PDC_MOUSE_MOVED | 8;
            if( wParam & MK_XBUTTON2)
                if( SP->_trap_mbe & BUTTON5_MOVED)
                    report_event |= PDC_MOUSE_MOVED | 16;
#endif

        if( !report_event)
            if( SP->_trap_mbe & REPORT_MOUSE_POSITION)
               report_event = PDC_MOUSE_POSITION;
        if( report_event)
        {
            int i;

            pdc_mouse_status.changes = report_event;
            for( i = 0; i < 3; i++)
            {
                pdc_mouse_status.button[i] = (((report_event >> i) & 1) ?
                    BUTTON_MOVED : 0);
            }
            *ptr_modified_key_to_return = 0;
            set_mouse( -1, 0, lParam );
        }             /* -1 to 'set_mouse' signals mouse move; 0 is ignored */
    }
}

static void HandlePaint( HWND hwnd )
{
    PAINTSTRUCT ps;
    HDC hdc;
    RECT rect;

    GetUpdateRect( hwnd, &rect, FALSE);
/*  printf( "In HandlePaint: %ld %ld, %ld %ld\n",
               rect.left, rect.top, rect.right, rect.bottom); */

    hdc = BeginPaint( hwnd, &ps);

    fix_up_edges( hdc, &rect);

    if( curscr && curscr->_y)
    {
        int i, x1, n_chars;

        x1 = rect.left / PDC_cxChar;
        n_chars = rect.right / PDC_cxChar - x1 + 1;
        if( n_chars > SP->cols - x1)
            n_chars = SP->cols - x1;
        if( n_chars > 0)
        for( i = rect.top / PDC_cyChar; i <= rect.bottom / PDC_cyChar; i++)
             if( i < SP->lines && curscr->_y[i])
                 PDC_transform_line_given_hdc( hdc, i, x1,
                                               n_chars, curscr->_y[i] + x1);
    }
    EndPaint( hwnd, &ps );
}

static bool key_already_handled = FALSE;
static int last_key_handled = 0;

static void HandleSyskeyDown( const WPARAM wParam, const LPARAM lParam,
               int *ptr_modified_key_to_return )
{
    const int shift_pressed = (GetKeyState( VK_SHIFT) & 0x8000);
    const int ctrl_pressed = (GetKeyState( VK_CONTROL) & 0x8000);
    const int alt_pressed = (GetKeyState( VK_MENU) & 0x8000);
    const int extended = ((lParam & 0x01000000) != 0);
    const int repeated = (int)( lParam >> 30) & 1;
    const KPTAB *kptr = kptab + wParam;
    int key = 0;
    static int repeat_count;

    if( !repeated)
        *ptr_modified_key_to_return = 0;

    if( repeated)
        repeat_count++;
    else
        repeat_count = 0;
    if( SP->return_key_modifiers && !repeated)
    {                     /* See notes above this function */
        if( wParam == VK_SHIFT)
        {
            if( GetKeyState( VK_LSHIFT) & 0x8000)
                *ptr_modified_key_to_return = KEY_SHIFT_L;
            else if( GetKeyState( VK_RSHIFT) & 0x8000)
                *ptr_modified_key_to_return = KEY_SHIFT_R;
            else if(( HIWORD( lParam) & 0xff) == 0x36)
                *ptr_modified_key_to_return = KEY_SHIFT_R;
            else
                *ptr_modified_key_to_return = KEY_SHIFT_L;
        }
        if( wParam == VK_CONTROL)
            *ptr_modified_key_to_return =
                                 (extended ? KEY_CONTROL_R : KEY_CONTROL_L);
        if( wParam == VK_MENU)
            *ptr_modified_key_to_return =
                                 (extended ? KEY_ALT_R : KEY_ALT_L);
    }

    if( !key)           /* it's not a shift, ctl, alt handled above */
    {
        if( extended && kptr->extended != 999)
            kptr = ext_kptab + kptr->extended;

        if( alt_pressed)
            key = kptr->alt;
        else if( ctrl_pressed)
            key = kptr->control;
        else if( shift_pressed)
            key = kptr->shift;
        else
            key = kptr->normal;
    }

    /* On non-US keyboards,  people hit Alt-Gr ("Alt-Ctrl" to */
    /* those on US keyboards) to get characters not otherwise */
    /* available:  accented characters,  local currency symbols, */
    /* etc.  So we default to suppressing Alt-Ctrl-letter combos.     */
    /* However,  apps can set PDC_show_ctrl_alts if they know they're */
    /* running on a US keyboard layout (or other layout that doesn't  */
    /* make special use of Ctrl-Alt... for example,  I use the Dvorak */
    /* layout;  it's fine with PDC_show_ctrl_alts = 1.)               */
    if( key >= KEY_MIN && key <= KEY_MAX)
        if( !ctrl_pressed || !alt_pressed || PDC_show_ctrl_alts)
            {
            add_key_to_queue( key);
            if (wParam == VK_MULTIPLY || wParam == VK_DIVIDE
                || wParam == VK_ADD || wParam == VK_SUBTRACT
                || wParam == VK_RETURN) {
                    last_key_handled = key;
                    key_already_handled = TRUE;
                }
            }
    pdc_key_modifiers = 0;
    /* Save the key modifiers if required. Do this first to allow to
       detect e.g. a pressed CTRL key after a hit of NUMLOCK. */

    if (SP->save_key_modifiers)
    {
        if( alt_pressed)
            pdc_key_modifiers |= PDC_KEY_MODIFIER_ALT;

        if( shift_pressed)
            pdc_key_modifiers |= PDC_KEY_MODIFIER_SHIFT;

        if( ctrl_pressed)
            pdc_key_modifiers |= PDC_KEY_MODIFIER_CONTROL;

        if( GetKeyState( VK_NUMLOCK) & 1)
            pdc_key_modifiers |= PDC_KEY_MODIFIER_NUMLOCK;

        if( repeat_count)
            pdc_key_modifiers |= PDC_KEY_MODIFIER_REPEAT;
    }
}

/* Blinking text is supposed to blink twice a second.  Therefore,
HandleTimer( ) is called every .5 seconds.

   In truth,  it's not so much 'blinking' as 'changing certain types of
text' that happens.  If text is really blinking (i.e.,  PDC_set_blink(TRUE)
has been called),  we need to flip that text.  Or if text _was_ blinking
and we've just called PDC_set_blink(FALSE),  all that text has to be
redrawn in 'standout' mode.  Also,  if PDC_set_line_color() has been
called,  all text with left/right/under/over/strikeout lines needs to
be redrawn.

   So.  After determining which attributes require redrawing (if any),
we run through all of 'curscr' and look for text with those attributes
set.  If we find such text,  we run it through PDC_transform_line.
(To speed matters up slightly,  we skip over text at the start and end
of each line that lacks the desired attributes. We could conceivably
get more clever;  as it stands,  if the very first and very last
characters are blinking,  we redraw the entire line,  even though
everything in between may not require it.  But it would probably be a
lot of code for little benefit.)

   Note that by default,  we'll usually find that the line color hasn't
changed and the 'blink mode' is still FALSE.  In that case,  attr_to_seek
will be zero and the only thing we'll do here is to blink the cursor. */

static void HandleTimer( const WPARAM wParam )
{
    int i;           /* see WndProc() notes */
    extern int PDC_really_blinking;          /* see 'pdcsetsc.c' */
    static int previously_really_blinking = 0;
    static int prev_line_color = -1;
    chtype attr_to_seek = 0;

    if( prev_line_color != SP->line_color)
        attr_to_seek = A_ALL_LINES;
    if( PDC_really_blinking || previously_really_blinking)
        attr_to_seek |= A_BLINK;
    prev_line_color = SP->line_color;
    previously_really_blinking = PDC_really_blinking;
    PDC_blink_state ^= 1;
    if( attr_to_seek)
    {
        for( i = 0; i < SP->lines; i++)
        {
            if( curscr->_y[i])
            {
                int j = 0, n_chars;
                chtype *line = curscr->_y[i];

                /* skip over starting text that isn't blinking: */
                while( j < SP->cols && !(*line & attr_to_seek))
                {
                    j++;
                    line++;
                }
                n_chars = SP->cols - j;
                /* then skip over text at the end that's not blinking: */
                while( n_chars && !(line[n_chars - 1] & attr_to_seek))
                {
                    n_chars--;
                }
                if( n_chars)
                    PDC_transform_line( i, j, n_chars, line);
            }
/*          else
                MessageBox( 0, "NULL _y[] found\n", "PDCurses", MB_OK);  */
        }
    }
    if( SP->cursrow >=SP->lines || SP->curscol >= SP->cols
        || SP->cursrow < 0 || SP->curscol < 0
        || !curscr->_y || !curscr->_y[SP->cursrow])
            debug_printf( "Cursor off-screen: %d %d, %d %d\n",
                          SP->cursrow, SP->curscol, SP->lines, SP->cols);
    else if( PDC_CURSOR_IS_BLINKING)
             PDC_transform_line( SP->cursrow, SP->curscol, 1,
                                 curscr->_y[SP->cursrow] + SP->curscol);
}

      /* Options to enlarge/shrink the font are currently commented out. */

static HMENU set_menu( void)
{
    const HMENU hMenu = CreateMenu( );
#ifdef PDC_WIDE
    AppendMenu( hMenu, MF_STRING, WM_CHOOSE_FONT, L"Font");
    AppendMenu( hMenu, MF_STRING, WM_PASTE, L"Paste");
#else
    AppendMenu( hMenu, MF_STRING, WM_CHOOSE_FONT, "Font");
    AppendMenu( hMenu, MF_STRING, WM_PASTE, "Paste");
#endif
    return( hMenu);
}

INLINE void HandleMenuToggle( bool *ptr_ignore_resize)
{
    const bool is_zoomed = IsZoomed( PDC_hWnd);
    HMENU hMenu;

    menu_shown ^= 1;
    hMenu = GetSystemMenu( PDC_hWnd, FALSE);
    CheckMenuItem( hMenu, WM_TOGGLE_MENU, MF_BYCOMMAND
                   | (menu_shown ? MF_CHECKED : MF_UNCHECKED));

    if( !is_zoomed)
        *ptr_ignore_resize = TRUE;
    if( !menu_shown)
    {
        hMenu = GetMenu( PDC_hWnd);   /* destroy existing menu */
        DestroyMenu( hMenu);
        hMenu = CreateMenu( );        /* then set an empty menu */
        SetMenu( PDC_hWnd, hMenu);
    }
    else
    {
        SetMenu( PDC_hWnd, set_menu( ));
    }
    *ptr_ignore_resize = FALSE;

    if( !is_zoomed)
    {
        PDC_resize_screen( PDC_n_rows, PDC_n_cols );
    }

    InvalidateRect( PDC_hWnd, NULL, FALSE);
}

INLINE uint64_t milliseconds_since_1970( void)
{
   FILETIME ft;
   const uint64_t jd_1601 = 2305813;  /* actually 2305813.5 */
   const uint64_t jd_1970 = 2440587;  /* actually 2440587.5 */
   const uint64_t ten_million = 10000000;
   const uint64_t diff = (jd_1970 - jd_1601) * ten_million * 86400;
   uint64_t decimicroseconds_since_1970;   /* i.e.,  time in units of 1e-7 seconds */

   GetSystemTimeAsFileTime( &ft);
   decimicroseconds_since_1970 = ((uint64_t)ft.dwLowDateTime |
                                ((uint64_t)ft.dwHighDateTime << 32)) - diff;
   return( decimicroseconds_since_1970 / 10000);
}

/* Note that there are two types of WM_TIMER timer messages.  One type
indicates that SP->mouse_wait milliseconds have elapsed since a mouse
button was pressed;  that's handled as described in the above notes.
The other type,  issued every half second,  indicates that blinking
should take place.  For these,  HandleTimer() is called (see above).

   On WM_PAINT,  we determine what parts of 'curscr' would be covered by
the update rectangle,  and run those through PDC_transform_line.

   For determining left/right shift, alt,  and control,  I borrowed code
from SDL.  Note that the Win32 version of PDCurses doesn't work correctly
here for Win9x;  it just does GetKeyState( VK_LSHIFT),  etc.,  which is
apparently not supported in Win9x.  So no matter which shift (or alt or
Ctrl key) is hit,  the right-hand variant is returned in that library.
The SDL handling,  and hence the handling below,  _does_ work on Win9x.
Note,  though,  that in Win9x,  detection of the Shift keys is hardware
dependent;  if you've an unusual keyboard,  both Shift keys may be
detected as right, or both as left. */

#if defined(_WIN32) && defined(__GNUC__)
#define ALIGN_STACK __attribute__((force_align_arg_pointer))
#else
#define ALIGN_STACK
#endif

static LRESULT ALIGN_STACK CALLBACK WndProc (const HWND hwnd,
                          const UINT message,
                          const WPARAM wParam,
                          const LPARAM lParam)
{
    int button_down = -1, button_up = -1;
    static int mouse_buttons_pressed = 0;
    static int mouse_click_type = -1;
    static LPARAM mouse_lParam;
    static uint64_t last_click_time[PDC_MAX_MOUSE_BUTTONS];
                               /* in millisec since 1970 */
    static int modified_key_to_return = 0;
    const RECT before_rect = PDC_mouse_rect;
    static bool ignore_resize = FALSE;

    PDC_hWnd = hwnd;
    if( !hwnd)
        debug_printf( "Null hWnd: msg %u, wParam %x, lParam %lx\n",
                     message, wParam, lParam);

    switch (message)
    {
    case WM_SIZING:
        HandleSizing( wParam, lParam );
        return( TRUE );

    case WM_SIZE:
                /* If ignore_resize = 1,  don't bother resizing; */
                /* the final window size has yet to be set       */
        if( ignore_resize == FALSE)
           HandleSize( wParam, lParam);
        return 0 ;

    case WM_MOUSEWHEEL:
        debug_printf( "Mouse wheel: %x %lx\n", wParam, lParam);
        modified_key_to_return = 0;
        set_mouse( VERTICAL_WHEEL_EVENT, (short)( HIWORD(wParam)), lParam);
        break;

    case WM_MOUSEHWHEEL:
        debug_printf( "Mouse horiz wheel: %x %lx\n", wParam, lParam);
        modified_key_to_return = 0;
        set_mouse( HORIZONTAL_WHEEL_EVENT, (short)( HIWORD(wParam)), lParam);
        break;

    case WM_MOUSEMOVE:
        HandleMouseMove( wParam, lParam, &modified_key_to_return );
        break;

    case WM_LBUTTONDOWN:
        PDC_mouse_rect.left = PDC_mouse_rect.right =
                                  LOWORD( lParam) / PDC_cxChar;
        PDC_mouse_rect.top = PDC_mouse_rect.bottom =
                                  HIWORD( lParam) / PDC_cyChar;
        PDC_selecting_rectangle = (wParam & MK_SHIFT);
        SetCapture( hwnd);
        button_down = 0;
        break;

    case WM_LBUTTONUP:
        button_up = 0;
        ReleaseCapture( );
        if( (PDC_mouse_rect.left != PDC_mouse_rect.right ||
             PDC_mouse_rect.top != PDC_mouse_rect.bottom) &&
             (PDC_mouse_rect.right >= 0 && PDC_mouse_rect.left >= 0
                        && curscr && curscr->_y) )
        {
            /* RR: will crash sometimes */
            /* As an example on double-click of the title bar */
            HandleBlockCopy();
        }
        PDC_mouse_rect.top = PDC_mouse_rect.bottom = -1;  /* now hide rect */
        break;

    case WM_RBUTTONDOWN:
        button_down = 2;
        SetCapture( hwnd);
        break;

    case WM_RBUTTONUP:
        button_up = 2;
        ReleaseCapture( );
        break;

    case WM_MBUTTONDOWN:
        button_down = 1;
        SetCapture( hwnd);
        break;

    case WM_MBUTTONUP:
        button_up = 1;
        ReleaseCapture( );
        break;

#if( PDC_MAX_MOUSE_BUTTONS >= 5)
             /* Win32a can support five mouse buttons.  But some may wish */
             /* to leave PDC_MAX_MOUSE_BUTTONS=3,  for compatibility      */
             /* with older PDCurses libraries.  Hence the above #if.      */
    case WM_XBUTTONDOWN:
        button_down = ((wParam & MK_XBUTTON1) ? 3 : 4);
        SetCapture( hwnd);
        break;

    case WM_XBUTTONUP:
#ifdef WRONG_WAY
          /* You'd think we'd use the following line,  wouldn't you?      */
          /* But we can't,  because an XBUTTONUP message doesn't actually */
          /* tell you which button was released!  So we'll assume that    */
          /* the released xbutton matches a pressed one;  and we've kept  */
          /* track of which buttons are currently pressed.                */
        button_up = ((wParam & MK_XBUTTON1) ? 3 : 4);
#endif
        button_up = (((mouse_buttons_pressed & 8) ||
                 pdc_mouse_status.xbutton[0] & BUTTON_PRESSED) ? 3 : 4);
        ReleaseCapture( );
        break;
#endif         /* #if( PDC_MAX_MOUSE_BUTTONS >= 5) */

    case WM_MOVE:
        return 0 ;

    case WM_ERASEBKGND:      /* no need to erase background;  it'll */
        return( 0);         /* all get painted over anyway */

      /* The WM_PAINT routine is sort of "borrowed" from doupdate( ) from */
      /* refresh.c.  I'm not entirely sure that this is what ought to be  */
      /* done,  though it does appear to work correctly.                  */
    case WM_PAINT:
        if( hwnd && curscr )
        {
            HandlePaint( hwnd );
        }
        break;

    case WM_KEYUP:
    case WM_SYSKEYUP:
        if( wParam == VK_MENU && numpad_unicode_value)
        {
            modified_key_to_return = numpad_unicode_value;
            numpad_unicode_value = 0;
            pdc_key_modifiers = 0;
        }
        if( modified_key_to_return )
        {
            add_key_to_queue( modified_key_to_return );
            modified_key_to_return = 0;
        }
        break;

    case WM_CHAR:       /* _Don't_ add Shift-Tab;  it's handled elsewhere */
        if( wParam != 9 || !(GetKeyState( VK_SHIFT) & 0x8000))
            if( !key_already_handled || last_key_handled != (int)wParam )
               add_key_to_queue( (int)wParam );
        key_already_handled = FALSE;
        last_key_handled = 0;
        break;

    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        if( wParam < 225 && wParam > 0 )
        {
            HandleSyskeyDown( wParam, lParam, &modified_key_to_return );
        }
        return 0 ;

    case WM_SYSCHAR:
        return 0 ;

    case WM_TIMER:
        /* see notes above this function */
        if( wParam != TIMER_ID_FOR_BLINKING )
        {
            static const int remap_table[PDC_MAX_MOUSE_BUTTONS] =
                    { BUTTON1_PRESSED, BUTTON2_PRESSED, BUTTON3_PRESSED,
                      BUTTON4_PRESSED, BUTTON5_PRESSED };

            modified_key_to_return = 0;
            if (SP && (SP->_trap_mbe & remap_table[wParam]))
            {
                if ( mouse_click_type != -1)
                {
                    set_mouse( (const int)wParam, mouse_click_type, mouse_lParam);
                }
            }
            KillTimer( PDC_hWnd, (int)wParam);
            mouse_click_type = -1;
            mouse_buttons_pressed ^= (1 << wParam);
        }
        else if( SP && curscr && curscr->_y)
        {
            /* blink the blinking text */
            HandleTimer( wParam );
        }
        break;

    case WM_CLOSE:
        {
            if( !PDC_shutdown_key[FUNCTION_KEY_SHUT_DOWN])
            {
                final_cleanup( );
                PDC_bDone = TRUE;
                exit( 0);
            }
            else
                add_key_to_queue( PDC_shutdown_key[FUNCTION_KEY_SHUT_DOWN]);
        }
        return( 0);

    case WM_COMMAND:
    case WM_SYSCOMMAND:
        if( wParam == WM_EXIT_GRACELESSLY)
        {
            final_cleanup( );
            PDC_bDone = TRUE;
            exit( 0);
        }
        else if( wParam == WM_ENLARGE_FONT || wParam == WM_SHRINK_FONT)
        {
            adjust_font_size( (wParam == WM_ENLARGE_FONT) ? 1 : -1);
            return( 0);
        }
        else if( wParam == WM_CHOOSE_FONT)
        {
            if( PDC_choose_a_new_font( ))
                adjust_font_size( 0);
            return( 0);
        }
        else if( wParam == WM_PASTE)
        {
            PDC_add_clipboard_to_key_queue( );
        }
        else if( wParam == WM_TOGGLE_MENU)
        {
            HandleMenuToggle( &ignore_resize);
        }
        break;

    case WM_DESTROY:
        PDC_LOG(("WM_DESTROY\n"));
        PostQuitMessage (0) ;
        PDC_bDone = TRUE;
        return 0 ;
    }

    if( hwnd)
       show_mouse_rect( hwnd, before_rect, PDC_mouse_rect);

    if( button_down >= 0)
    {
        modified_key_to_return = 0;
        mouse_buttons_pressed |= (1 << button_down);
        mouse_lParam = lParam;
    }
    if( button_up >= 0)
    {
        static const int single_remap_table[PDC_MAX_MOUSE_BUTTONS] =
                    { BUTTON1_CLICKED, BUTTON2_CLICKED, BUTTON3_CLICKED,
                    BUTTON4_CLICKED, BUTTON5_CLICKED };

        static const int double_remap_table[PDC_MAX_MOUSE_BUTTONS] =
                    { BUTTON1_DOUBLE_CLICKED, BUTTON2_DOUBLE_CLICKED,
                    BUTTON3_DOUBLE_CLICKED, BUTTON4_DOUBLE_CLICKED,
                    BUTTON5_DOUBLE_CLICKED };

        static const int triple_remap_table[PDC_MAX_MOUSE_BUTTONS] =
                    { BUTTON1_TRIPLE_CLICKED, BUTTON2_TRIPLE_CLICKED,
                    BUTTON3_TRIPLE_CLICKED, BUTTON4_TRIPLE_CLICKED,
                    BUTTON5_TRIPLE_CLICKED };

        static const int released_remap_table[PDC_MAX_MOUSE_BUTTONS] =
                    { BUTTON1_RELEASED, BUTTON2_RELEASED, BUTTON3_RELEASED,
                    BUTTON4_RELEASED, BUTTON5_RELEASED };

        modified_key_to_return = 0;
        if( (mouse_buttons_pressed >> button_up) & 1)
        {
            const uint64_t curr_click_time =
                             milliseconds_since_1970( );

            static int n_previous_clicks;

            if( curr_click_time <
                             last_click_time[button_up] + 2 * SP->mouse_wait)
               n_previous_clicks++;       /* 'n_previous_clicks' will be  */
            else                         /* zero for a "normal" click, 1  */
               n_previous_clicks = 0;   /* for a dblclick, 2 for a triple */

            if (n_previous_clicks >= 2 &&
                (SP->_trap_mbe & triple_remap_table[button_up]))
                mouse_click_type = BUTTON_TRIPLE_CLICKED;
            else if (n_previous_clicks >= 1 &&
                (SP->_trap_mbe & double_remap_table[button_up]))
                mouse_click_type = BUTTON_DOUBLE_CLICKED;
            else if (SP->_trap_mbe & single_remap_table[button_up])
                /* either it's not a doubleclick, or we aren't */
                /* checking for double clicks */
                mouse_click_type = BUTTON_CLICKED;
            else
                mouse_click_type = -1;

            KillTimer( hwnd, button_up);
            SetTimer( hwnd, button_up, SP->mouse_wait, NULL);
            mouse_buttons_pressed ^= (1 << button_up);
            last_click_time[button_up] = curr_click_time;
        }

        if( SP->_trap_mbe & released_remap_table[button_up])
            set_mouse(button_up, BUTTON_RELEASED, lParam);
    }

    return DefWindowProc( hwnd, message, wParam, lParam) ;
}

      /* Default behaviour is that,  when one clicks on the 'close' button, */
      /* exit( 0) is called,  just as in the SDL and X11 versions.  But if  */
      /* one wishes,  one can call PDC_set_shutdown_key to cause those      */
      /* buttons to put a specified character into the input queue.  It's   */
      /* then the application's problem to exit gracefully,  perhaps with   */
      /* messages such as 'are you sure' and so forth.                      */
      /*   If you've set a shutdown key,  there's always a risk that the    */
      /* program will get stuck in a loop and never process said key.  So   */
      /* when the key is set,  a 'Kill' item is appended to the system menu */
      /* so that the user still has some way to terminate the app,  albeit  */
      /* with extreme prejudice (i.e.,  click on 'Kill' and exit is called  */
      /* and the app exits gracelessly.)                                    */

int PDC_set_function_key( const unsigned function, const int new_key)
{
    int old_key = -1;

    if( function < PDC_MAX_FUNCTION_KEYS)
    {
         old_key = PDC_shutdown_key[function];
         PDC_shutdown_key[function] = new_key;
    }

    if( function == FUNCTION_KEY_SHUT_DOWN)
        if( (new_key && !old_key) || (old_key && !new_key))
        {
            HMENU hMenu = GetSystemMenu( PDC_hWnd, FALSE);

            if( new_key)
                AppendMenu( hMenu, MF_STRING, WM_EXIT_GRACELESSLY, _T( "Kill"));
            else
                RemoveMenu( hMenu, WM_EXIT_GRACELESSLY, MF_BYCOMMAND);
        }
    return( old_key);
}

/* Used to define whether or not the Font/Paste menu is visible by default.
If this method is not call, it will be true. Must be called before the main
window has been created. */
void    PDC_set_default_menu_visibility( int visible)
{
    extern HWND PDC_hWnd;
    if ( !PDC_hWnd)
    {
        menu_shown = (visible) ? 1 : 0;
    }
}


/* https://msdn.microsoft.com/en-us/library/windows/desktop/dd162826(v=vs.85).aspx
The code at the above link provides general methods for positioning a window
on a multiple-display setup.  The only instance we're using is the
MONITOR_WORKAREA one,  which ensures that even if monitor geometry changes,
the window will still be entirely on-screen.

These functions entered the Win32 API with Windows 2000.  If
MONITOR_DEFAULTTONEAREST isn't defined,  we shouldn't try to do this.  */

#ifdef MONITOR_DEFAULTTONEAREST

static void clip_or_center_rect_to_monitor( LPRECT prc)
{
    HMONITOR hMonitor;
    MONITORINFO mi;
    RECT        rc;
    const int   w = prc->right  - prc->left;
    const int   h = prc->bottom - prc->top;

    hMonitor = MonitorFromRect(prc, MONITOR_DEFAULTTONEAREST);

    mi.cbSize = sizeof(mi);
    GetMonitorInfo(hMonitor, &mi);

    rc = mi.rcMonitor;

    prc->left   = max(rc.left, min(rc.right-w,  prc->left));
    prc->top    = max(rc.top,  min(rc.bottom-h, prc->top));
    prc->right  = prc->left + w;
    prc->bottom = prc->top  + h;
}

static void clip_or_center_window_to_monitor( HWND hwnd)
{
    RECT rc;

    GetWindowRect(hwnd, &rc);
    clip_or_center_rect_to_monitor(&rc);
    SetWindowPos(hwnd, NULL, rc.left, rc.top, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}
#endif

/* By default,  the user cannot resize the window.  This is because
many apps don't handle KEY_RESIZE,  and one can get odd behavior
in such cases.  There are two ways around this.  If you call
PDC_set_resize_limits( ) before initwin( ),  telling Win32a exactly how
large/small the window can be,  the window will be user-resizable.  Or
you can set ttytype[0...3] to contain the resize limits.   A call such as

PDC_set_resize_limits( 42, 42, 135, 135);

   will result in the window being fixed at 42 lines by 135 columns.

PDC_set_resize_limits( 20, 50, 70, 200);

   will mean the window can have 20 to 50 lines and 70 to 200 columns.
The user will be able to resize the window freely within those limits.
See 'newtest.c' (in the 'demos' folder) for an example.

   This function is used in only one place (PDC_scr_open( )),  so
it's inlined. */

INLINE int set_up_window( void)
{
    /* create the dialog window  */
    WNDCLASS   wndclass ;
    HMENU hMenu;
    HANDLE hInstance = GetModuleHandleA( NULL);
    int n_default_columns = 80;
    int n_default_rows = 25;
    int xsize, ysize, window_style;
    int xloc = CW_USEDEFAULT;
    int yloc = CW_USEDEFAULT;
    TCHAR WindowTitle[MAX_PATH];
    const TCHAR *AppName = _T( "Curses_App");
    HICON icon;
    static bool wndclass_has_been_registered = FALSE;

    if( !hInstance)
        debug_printf( "No instance: %d\n", GetLastError( ));
    originally_focussed_window = GetForegroundWindow( );
    debug_printf( "hInstance %x\nOriginal window %x\n", hInstance, originally_focussed_window);
    /* set the window icon from the icon in the process */
    icon = get_app_icon();
    if( !icon )
       icon = LoadIcon( NULL, IDI_APPLICATION);
    if( !wndclass_has_been_registered)
    {
        ATOM rval;

        wndclass.style         = CS_VREDRAW ;
        wndclass.lpfnWndProc   = WndProc ;
        wndclass.cbClsExtra    = 0 ;
        wndclass.cbWndExtra    = 0 ;
        wndclass.hInstance     = hInstance ;
        wndclass.hIcon         = icon;
        wndclass.hCursor       = LoadCursor (NULL, IDC_ARROW) ;
        wndclass.hbrBackground = (HBRUSH) GetStockObject (WHITE_BRUSH) ;
        wndclass.lpszMenuName  = NULL ;
        wndclass.lpszClassName = AppName ;

        rval = RegisterClass( &wndclass) ;
        if( !rval)
        {
            const DWORD last_error = GetLastError( );

            debug_printf( "RegisterClass failed: GetLastError = %lx\n", last_error);
            return( -1);
        }
        wndclass_has_been_registered = TRUE;
    }

    get_app_name( WindowTitle, TRUE);
#ifdef PDC_WIDE
    debug_printf( "WindowTitle = '%ls'\n", WindowTitle);
#endif

    get_default_sizes_from_registry( &n_default_columns, &n_default_rows, &xloc, &yloc,
                     &menu_shown);
    if( PDC_n_rows > 2 && PDC_n_cols > 2)
    {
        n_default_columns = PDC_n_cols;
        n_default_rows    = PDC_n_rows;
    }
    if( ttytype[1])
        PDC_set_resize_limits( (unsigned char)ttytype[0],
                               (unsigned char)ttytype[1],
                               (unsigned char)ttytype[2],
                               (unsigned char)ttytype[3]);
    debug_printf( "Size %d x %d,  loc %d x %d;  menu %d\n",
               n_default_columns, n_default_rows, xloc, yloc, menu_shown);
    get_character_sizes( NULL, &PDC_cxChar, &PDC_cyChar);

    if( min_lines != max_lines || min_cols != max_cols)
        window_style = ((n_default_columns == -1) ?
                    WS_MAXIMIZE | WS_OVERLAPPEDWINDOW : WS_OVERLAPPEDWINDOW);
    else  /* fixed-size window:  looks "normal",  but w/o a maximize box */
        window_style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;

    if( n_default_columns == -1)
        xsize = ysize = CW_USEDEFAULT;
    else
    {
        keep_size_within_bounds( &n_default_rows, &n_default_columns);
        xsize = PDC_cxChar * n_default_columns;
        ysize = PDC_cyChar * n_default_rows;
        adjust_window_size( &xsize, &ysize, window_style, menu_shown);
    }

    PDC_hWnd = CreateWindow( AppName, WindowTitle, window_style,
                    xloc, yloc,
                    xsize, ysize,
                    NULL, (menu_shown ? set_menu( ) : NULL),
                    hInstance, NULL) ;

    if( !PDC_hWnd)
    {
        const DWORD last_error = GetLastError( );

        debug_printf( "CreateWindow failed; GetLastError = %ld", last_error);
        return( -2);
    }

    hMenu = GetSystemMenu( PDC_hWnd, FALSE);
    AppendMenu( hMenu, MF_STRING | (menu_shown ? MF_CHECKED : MF_UNCHECKED), WM_TOGGLE_MENU, _T( "Menu"));
    AppendMenu( hMenu, MF_STRING, WM_CHOOSE_FONT, _T( "Choose Font"));

    debug_printf( "menu set\n");

    ShowWindow (PDC_hWnd,
                    (n_default_columns == -1) ? SW_SHOWMAXIMIZED : SW_SHOWNORMAL);
    debug_printf( "window shown\n");
    ValidateRect( PDC_hWnd, NULL);       /* don't try repainting */
    UpdateWindow (PDC_hWnd) ;
    debug_printf( "window updated\n");
    SetTimer( PDC_hWnd, TIMER_ID_FOR_BLINKING, 500, NULL);
    debug_printf( "timer set\n");

#ifdef MONITOR_DEFAULTTONEAREST
    /* if the window is off-screen, move it on screen. */
    clip_or_center_window_to_monitor( PDC_hWnd);
#endif

    return( 0);
}

/* open the physical screen -- allocate SP, miscellaneous intialization,
   and may save the existing screen for later restoration.

   Deciding on a for-real maximum screen size has proven difficult.
   But there is really no particularly good reason to set such a maximum.
   If one does,  you get some tricky issues:  suppose the user drags the
   window to create a screen larger than MAX_LINES or MAX_COLUMNS?  My
   hope is to evade that problem by just setting those constants to be...
   well... unrealistically large.  */

#define MAX_LINES   50000
#define MAX_COLUMNS 50000

int PDC_scr_open( int argc, char **argv)
{
    int i, r, g, b;
    HMODULE hntdll = GetModuleHandle( _T("ntdll.dll"));

    if( hntdll)
        wine_version = (wine_version_func)GetProcAddress(hntdll, "wine_get_version");

    PDC_LOG(("PDC_scr_open() - called\n"));
    SP = calloc(1, sizeof(SCREEN));
    color_pair_indices = (short *)calloc(PDC_COLOR_PAIRS * 2, sizeof( short));
    pdc_rgbs = (COLORREF *)calloc(N_COLORS, sizeof( COLORREF));
    if (!SP || !color_pair_indices || !pdc_rgbs)
        return ERR;

    debug_printf( "colors alloc\n");
    COLORS = N_COLORS;  /* should give this a try and see if it works! */
    for( i = 0; i < 16; i++)
    {
        const int intensity = ((i & 8) ? 0xff : 0xc0);

        pdc_rgbs[i] = RGB( ((i & COLOR_RED) ? intensity : 0),
                           ((i & COLOR_GREEN) ? intensity : 0),
                           ((i & COLOR_BLUE) ? intensity : 0));
    }
           /* 256-color xterm extended palette:  216 colors in a
            6x6x6 color cube,  plus 24 (not 50) shades of gray */
    for( r = 0; r < 6; r++)
        for( g = 0; g < 6; g++)
            for( b = 0; b < 6; b++)
                pdc_rgbs[i++] = RGB( r ? r * 40 + 55 : 0,
                                   g ? g * 40 + 55 : 0,
                                   b ? b * 40 + 55 : 0);
    for( i = 0; i < 24; i++)
        pdc_rgbs[i + 232] = RGB( i * 10 + 8, i * 10 + 8, i * 10 + 8);
    SP->mouse_wait = PDC_CLICK_PERIOD;
    SP->visibility = 0;                /* no cursor,  by default */
    SP->curscol = SP->cursrow = 0;
    SP->audible = TRUE;
    SP->mono = FALSE;

    /* note: we parse the non-wide argc (see comment in header),
       therefore using non-wide char handling here */
    if( argc && argv)         /* store a copy of the input arguments */
    {
        PDC_argc = argc;
        PDC_argv = (char **)calloc( argc + 1, sizeof( char *));
        for( i = 0; i < argc; i++)
        {
            PDC_argv[i] = (char *)malloc( strlen( argv[i]) + 1);
            strcpy( PDC_argv[i], argv[i]);
        }
    }

    if( set_up_window( ))
    {
        fprintf( stderr, "set_up_window failed\n");
        return ERR;
    }
    debug_printf( "Back from set_up_window\n");
    while( !PDC_get_rows( ))     /* wait for screen to be drawn and */
      ;                          /* actual size to be determined    */

    debug_printf( "Back from PDC_get_rows\n");
    SP->lines = PDC_get_rows();
    SP->cols = PDC_get_columns();

    if (SP->lines < 2 || SP->lines > MAX_LINES
       || SP->cols < 2 || SP->cols > MAX_COLUMNS)
    {
        fprintf(stderr, "LINES value must be >= 2 and <= %d: got %d\n",
                MAX_LINES, SP->lines);
        fprintf(stderr, "COLS value must be >= 2 and <= %d: got %d\n",
                MAX_COLUMNS, SP->cols);

        return ERR;
    }

/*  PDC_reset_prog_mode();   doesn't do anything anyway */
    debug_printf( "...we're done\n");
    return OK;
}

/* the core of resize_term() */

int PDC_resize_screen( int nlines, int ncols)
{
    if( !stdscr)        /* window hasn't been created yet;  we're */
    {                   /* specifying its size before doing so    */
        PDC_n_rows = nlines;
        PDC_n_cols = ncols;
        return OK;
    }
    SP->resized = FALSE;
    debug_printf( "Incoming: %d %d\n", nlines, ncols);
    if( nlines >= 2 && ncols >= 2 && PDC_cxChar && PDC_cyChar && PDC_hWnd &&
                  !IsZoomed( PDC_hWnd) /* && WaitResult == WAIT_OBJECT_0 */)
    {
        RECT rect, client_rect;
        int new_width;
        int new_height;

        GetWindowRect( PDC_hWnd, &rect);
        GetClientRect( PDC_hWnd, &client_rect);
        debug_printf( "Outgoing: %d %d\n", nlines, ncols);
        new_width = ncols * PDC_cxChar;
        new_height = nlines * PDC_cyChar;

        if( new_width != client_rect.right || new_height != client_rect.bottom)
        {                    /* check to make sure size actually changed */
            add_resize_key = 0;
            SetWindowPos( PDC_hWnd, 0, 0, 0,
                   new_width + (rect.right - rect.left) - client_rect.right,
                   new_height + (rect.bottom - rect.top) - client_rect.bottom,
                  SWP_NOMOVE | SWP_NOZORDER | SWP_SHOWWINDOW);
        }
    }
    return OK;
}

void PDC_reset_prog_mode(void)
{
    PDC_LOG(("PDC_reset_prog_mode() - called.\n"));
#ifdef NOT_CURRENTLY_IN_USE
    if( PDC_bDone == FALSE && PDC_hWnd)
    {
        PDC_bDone = TRUE;
        SetForegroundWindow( PDC_hWnd);
    }
#endif
}

void PDC_reset_shell_mode(void)
{
}

void PDC_restore_screen_mode(int i)
{
}

void PDC_save_screen_mode(int i)
{
}

/* NOTE:  as with PDC_init_color() (see below),  this function has to
redraw all text with color attribute 'pair' to match the newly-set
foreground and background colors.  The loops to go through every character
in curscr,  looking for those that need to be redrawn and ignoring
those at the front and start of each line,  are very similar. */

static short get_pair( const chtype ch)
{
   return( (short)( (ch & A_COLOR) >> PDC_COLOR_SHIFT));
}

void PDC_init_pair( short pair, short fg, short bg)
{
    if( color_pair_indices[pair] != fg ||
        color_pair_indices[pair + PDC_COLOR_PAIRS] != bg)
    {
        color_pair_indices[pair] = fg;
        color_pair_indices[pair + PDC_COLOR_PAIRS] = bg;
        /* Possibly go through curscr and redraw everything with that color! */
        if( curscr && curscr->_y)
        {
            int i;

            for( i = 0; i < SP->lines; i++)
                if( curscr->_y[i])
                {
                    int j = 0, n_chars;
                    chtype *line = curscr->_y[i];

             /* skip over starting text that isn't of the desired color: */
                    while( j < SP->cols && get_pair( *line) != pair)
                    {
                        j++;
                        line++;
                    }
                    n_chars = SP->cols - j;
            /* then skip over text at the end that's not the right color: */
                    while( n_chars && get_pair( line[n_chars - 1]) != pair)
                        n_chars--;
                    if( n_chars)
                        PDC_transform_line( i, j, n_chars, line);
                }
        }
    }
}

int PDC_pair_content( short pair, short *fg, short *bg)
{
    *fg = color_pair_indices[pair];
    *bg = color_pair_indices[pair + PDC_COLOR_PAIRS];
    return OK;
}

bool PDC_can_change_color(void)
{
    return TRUE;
}

int PDC_color_content( short color, short *red, short *green, short *blue)
{
    COLORREF col = pdc_rgbs[color];

    *red = DIVROUND(GetRValue(col) * 1000, 255);
    *green = DIVROUND(GetGValue(col) * 1000, 255);
    *blue = DIVROUND(GetBValue(col) * 1000, 255);

    return OK;
}

/* We have an odd problem when changing colors with PDC_init_color().  On
palette-based systems,  you just change the palette and the hardware takes
care of the rest.  Here,  though,  we actually need to redraw any text that's
drawn in the specified color.  So we gotta look at each character and see if
either the foreground or background matches the index that we're changing.
Then, that text gets redrawn.  For speed/simplicity,  the code looks for the
first and last character in each line that would be affected, then draws those
in between (frequently,  this will be zero characters, i.e., no text on that
particular line happens to use the color index in question.)  See similar code
above for PDC_init_pair(),  to handle basically the same problem. */

static int color_used_for_this_char( const chtype c, const int idx)
{
    const int color = get_pair( c);
    const int rval = (color_pair_indices[color] == idx ||
                     color_pair_indices[color + PDC_COLOR_PAIRS] == idx);

    return( rval);
}

int PDC_init_color( short color, short red, short green, short blue)
{
    const COLORREF new_rgb = RGB(DIVROUND(red * 255, 1000),
                                 DIVROUND(green * 255, 1000),
                                 DIVROUND(blue * 255, 1000));

    if( pdc_rgbs[color] != new_rgb)
    {
        pdc_rgbs[color] = new_rgb;
        /* Possibly go through curscr and redraw everything with that color! */
        if( curscr && curscr->_y)
        {
            int i;

            for( i = 0; i < SP->lines; i++)
                if( curscr->_y[i])
                {
                    int j = 0, n_chars;
                    chtype *line = curscr->_y[i];

             /* skip over starting text that isn't of the desired color: */
                    while( j < SP->cols
                                 && !color_used_for_this_char( *line, color))
                    {
                        j++;
                        line++;
                    }
                    n_chars = SP->cols - j;
            /* then skip over text at the end that's not the right color: */
                    while( n_chars &&
                         !color_used_for_this_char( line[n_chars - 1], color))
                        n_chars--;
                    if( n_chars)
                        PDC_transform_line( i, j, n_chars, line);
                }
        }
    }
    return OK;
}
