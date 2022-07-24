/* PDCurses */

#include <curspriv.h>
#include <string.h>

#include <stdlib.h>

/*man-start**************************************************************

clipboard
---------

### Synopsis

    int PDC_getclipboard(char **contents, long *length);
    int PDC_setclipboard(const char *contents, long length);
    int PDC_freeclipboard(char *contents);
    int PDC_clearclipboard(void);

### Description

   PDC_getclipboard() gets the textual contents of the system's
   clipboard. This function returns the contents of the clipboard in the
   contents argument. It is the responsibility of the caller to free the
   memory returned, via PDC_freeclipboard(). The length of the clipboard
   contents is returned in the length argument.

   PDC_setclipboard copies the supplied text into the system's
   clipboard, emptying the clipboard prior to the copy.

   PDC_clearclipboard() clears the internal clipboard.

### Return Values

    indicator of success/failure of call.
    PDC_CLIP_SUCCESS        the call was successful
    PDC_CLIP_MEMORY_ERROR   unable to allocate sufficient memory for
                            the clipboard contents
    PDC_CLIP_EMPTY          the clipboard contains no text
    PDC_CLIP_ACCESS_ERROR   no clipboard support

### Portability
                             X/Open  ncurses  NetBSD
    PDC_getclipboard            -       -       -
    PDC_setclipboard            -       -       -
    PDC_freeclipboard           -       -       -
    PDC_clearclipboard          -       -       -

**man-end****************************************************************/

/* This version is used (at present) in the VT,  framebuffer,  DOS,
and DOSVGA ports,  and could be used for the Plan9 one as well.  These
platforms lack system clipboards,  so we just store the clipboard text
in a malloced buffer.   */

/* global clipboard contents, should be NULL if none set */

static char *pdc_clipboard = NULL;

int PDC_getclipboard(char **contents, long *length)
{
    int len;

    PDC_LOG(("PDC_getclipboard() - called\n"));

    if (!pdc_clipboard)
        return PDC_CLIP_EMPTY;

    len = (int)strlen(pdc_clipboard);
    *contents = malloc(len + 1);
    if (!*contents)
        return PDC_CLIP_MEMORY_ERROR;

    strcpy(*contents, pdc_clipboard);
    *length = len;

    return PDC_CLIP_SUCCESS;
}

int PDC_setclipboard(const char *contents, long length)
{
    PDC_LOG(("PDC_setclipboard() - called\n"));

    PDC_clearclipboard( );

    if (contents)
    {
        pdc_clipboard = malloc(length + 1);
        if (!pdc_clipboard)
            return PDC_CLIP_MEMORY_ERROR;

        strcpy(pdc_clipboard, contents);
    }

    return PDC_CLIP_SUCCESS;
}

int PDC_freeclipboard(char *contents)
{
    PDC_LOG(("PDC_freeclipboard() - called\n"));

    /* should we also free empty the system clipboard? probably not */

    if (contents)
    {
        /* NOTE: We free the memory, but we can not set caller's pointer
           to NULL, so if caller calls again then will try to access
           free'd memory.  We 1st overwrite memory with a string so if
           caller tries to use free memory they won't get what they
           expect & hopefully notice. */
        const char *refreed = "!Freed buffer PDC clipboard!";
        const char *tptr = refreed;
        char *tptr2 = contents;

        while( *tptr2 && *tptr)
            *tptr2++ = *tptr++;

        free(contents);
    }

    return PDC_CLIP_SUCCESS;
}

int PDC_clearclipboard(void)
{
    PDC_LOG(("PDC_clearclipboard() - called\n"));

    if (pdc_clipboard)
    {
        free(pdc_clipboard);
        pdc_clipboard = NULL;
    }

    return PDC_CLIP_SUCCESS;
}
