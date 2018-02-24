/* Public Domain Curses */

#include "pdcwin.h"
#include <tchar.h>

/*man-start**************************************************************

  Name:                                                         pdckbd

  Synopsis:
        unsigned long PDC_get_input_fd(void);

  Description:
        PDC_get_input_fd() returns the file descriptor that PDCurses
        reads its input from. It can be used for select().  (For the
        Win32a version,  it's meaningless and returns zero.)

  Portability                                X/Open    BSD    SYS V
        PDC_get_input_fd                        -       -       -

**man-end****************************************************************/


unsigned long PDC_get_input_fd(void)
{
    PDC_LOG(("PDC_get_input_fd() - called\n"));

    return 0L;
}

void PDC_set_keyboard_binary(bool on)
{
    PDC_LOG(("PDC_set_keyboard_binary() - called\n"));
}

/* check if a key or mouse event is waiting */

#define KEY_QUEUE_SIZE    30

extern int PDC_key_queue_low, PDC_key_queue_high;
extern int PDC_key_queue[KEY_QUEUE_SIZE];
static TCHAR *clipboard_contents;
static long clipboard_len;

int PDC_getclipboard_handle( HANDLE *handle);      /* pdcclip.c */

      /* When copying the clipboard contents for use in the key */
      /* queue,  we leave out the line feeds.  We also don't do */
      /* it at all if 'clipboard_contents' is already full.     */
void PDC_add_clipboard_to_key_queue( void)
{
    long i, j;
    HANDLE handle;

    if( !clipboard_contents)
        if( PDC_getclipboard_handle( &handle) == PDC_CLIP_SUCCESS)
        {
#ifdef PDC_WIDE
            const long len = (long)wcslen((wchar_t *)handle);
#else
            const long len = (long)strlen((char *)handle);
#endif

            clipboard_contents = (TCHAR *)calloc( len + 1, sizeof( TCHAR));
            memcpy( clipboard_contents, (TCHAR *)handle, (len + 1) * sizeof( TCHAR));
            CloseClipboard( );
            for( i = j = 0; i < len; i++)
                if( clipboard_contents[i] != 10)
                    clipboard_contents[j++] = clipboard_contents[i];
            clipboard_len = j;
        }
}

/* PDCurses message/event callback */
/* Calling PDC_napms for one millisecond ensures that the message loop */
/* is called and messages in general,  and keyboard events in particular, */
/* get processed.   */

bool PDC_check_key(void)
{
    PDC_napms( 1);
    if( PDC_key_queue_low != PDC_key_queue_high || clipboard_len)
        return TRUE;
    return FALSE;
}

/* return the next available key or mouse event */

int PDC_get_key(void)
{
    int rval = -1;

    if( PDC_key_queue_low != PDC_key_queue_high)
    {
        rval = PDC_key_queue[PDC_key_queue_low++];
        if( PDC_key_queue_low == KEY_QUEUE_SIZE)
            PDC_key_queue_low = 0;
    }
    else if( clipboard_len)
    {
        rval = *clipboard_contents;
        clipboard_len--;
        memmove( clipboard_contents, clipboard_contents + 1, clipboard_len * sizeof( TCHAR));
        if( !clipboard_len)
        {
            free( clipboard_contents);
            clipboard_contents = NULL;
        }
    }

    SP->key_code = (rval >= KEY_MIN && rval <= KEY_MAX);
    return rval;
}

/* discard any pending keyboard or mouse input -- this is the core
   routine for flushinp() */

void PDC_flushinp(void)
{
    PDC_LOG(("PDC_flushinp() - called\n"));
    PDC_key_queue_low = PDC_key_queue_high = 0;
    if( clipboard_len)
    {
        free( clipboard_contents);
        clipboard_contents = NULL;
        clipboard_len = 0;
    }
}

int PDC_mouse_set(void)
{
    /* If turning on mouse input: Set ENABLE_MOUSE_INPUT, and clear
       all other flags, including the extended flags;
       If turning off the mouse: Set QuickEdit Mode to the status it
       had on startup, and clear all other flags */

    return OK;
}

int PDC_modifiers_set(void)
{
    return OK;
}
