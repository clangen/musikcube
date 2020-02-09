/* Public Domain Curses */

#if defined( PDC_WIDE) && !defined( UNICODE)
# define UNICODE
#endif

#include <windows.h>
#undef MOUSE_MOVED
#include <curspriv.h>

# if(CHTYPE_LONG >= 2)     /* 64-bit chtypes */
    # define PDC_ATTR_SHIFT  23
# else
#ifdef CHTYPE_LONG         /* 32-bit chtypes */
    # define PDC_ATTR_SHIFT  19
#else                      /* 16-bit chtypes */
    # define PDC_ATTR_SHIFT  8
#endif
#endif
#if (defined(__CYGWIN__) || defined(__MINGW32__) || defined(__WATCOMC__) \
     || (defined(_MSC_VER) && _WIN32_WINNT >= _WIN32_WINNT_VISTA))
   #if !defined(HAVE_INFOEX) && !defined(HAVE_NO_INFOEX)
       # define HAVE_INFOEX
   #endif
#endif

extern unsigned char *pdc_atrtab;
extern HANDLE pdc_con_out, pdc_con_in;
extern DWORD pdc_quick_edit;

extern int PDC_get_buffer_rows(void);
