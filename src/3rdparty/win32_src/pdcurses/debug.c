/* PDCurses */

#include <curspriv.h>
#include <assert.h>

/*man-start**************************************************************

debug
-----

### Synopsis

    void traceon(void);
    void traceoff(void);
    unsigned curses_trace( const unsigned);
    void trace( const unsigned);
    void PDC_debug(const char *, ...);
    void _tracef(const char *, ...);

### Description

   traceon() and traceoff() toggle the recording of debugging
   information to the file "trace". Although not standard, similar
   functions are in some other curses implementations (e.g., SVr4).

   curses_trace() turns tracing on if called with a non-zero value and
   off if called with zero.  At some point,  the input value will be
   used to set flags for more nuanced trace output,  a la ncurses;  but
   at present,  debugging is simply on or off.  The previous tracing
   flags are returned.

   trace() is a duplicate of curses_trace(),  but returns nothing.  It
   is deprecated because it often conflicts with application names.

   PDC_debug() is the function that writes to the file, based on whether
   traceon() has been called. It's used from the PDC_LOG() macro.

   _tracef() is an ncurses alias for PDC_debug,  and is added solely
   for compatibility.

   The environment variable PDC_TRACE_FLUSH controls whether the trace
   file contents are fflushed after each write. The default is not. Set
   it to enable this (may affect performance).

### Portability
                             X/Open  ncurses  NetBSD
    traceon                     -       -       -
    traceoff                    -       -       -
    trace                       -       Y       -
    curses_trace                -       Y       -
    PDC_debug                   -       -       -
    _tracef                     -       Y       -

**man-end****************************************************************/

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>

/* PDC_trace_flags will eventually be global or part of the SCREEN struct. */

static unsigned PDC_trace_flags = 0;

static bool want_fflush = FALSE;

void PDC_debug(const char *fmt, ...)
{
    va_list args;
    char hms[9];
    time_t now;

    assert( SP);
    if (!SP || !SP->dbfp || SP->in_endwin)
        return;

    time(&now);
    strftime(hms, 9, "%H:%M:%S", localtime(&now));
    fprintf(SP->dbfp, "At: %8.8ld - %s ", (long) clock(), hms);

    va_start(args, fmt);
    vfprintf(SP->dbfp, fmt, args);
    va_end(args);

    /* If you are crashing and losing debugging information, enable this
       by setting the environment variable PDC_TRACE_FLUSH. This may
       impact performance. */

    if (want_fflush)
        fflush(SP->dbfp);

    /* If with PDC_TRACE_FLUSH enabled you are still losing logging in
       crashes, you may need to add a platform-dependent mechanism to
       flush the OS buffers as well (such as fsync() on POSIX) -- but
       expect terrible performance. */
}

void _tracef(const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    PDC_debug( fmt, args);
    va_end(args);
}

void traceon(void)
{
    assert( SP);
    if (!SP)
        return;

    if (SP->dbfp)
        fclose(SP->dbfp);

    /* open debug log file append */
    SP->dbfp = fopen("trace", "a");
    if (!SP->dbfp)
    {
        fprintf(stderr, "PDC_debug(): Unable to open debug log file\n");
        return;
    }

    PDC_trace_flags = TRACE_MAXIMUM;
    if (getenv("PDC_TRACE_FLUSH"))
        want_fflush = TRUE;

    PDC_LOG(("traceon() - called\n"));
}

void traceoff(void)
{
    assert( SP);
    if (!SP || !SP->dbfp)
        return;

    PDC_LOG(("traceoff() - called\n"));

    fclose(SP->dbfp);
    SP->dbfp = NULL;
    PDC_trace_flags = TRACE_DISABLE;
    want_fflush = FALSE;
}

unsigned curses_trace( const unsigned param)
{
    const unsigned rval = PDC_trace_flags;

    assert( SP);
    if( SP)
    {
        param ? traceon( ) : traceoff( );
        PDC_trace_flags = param;
    }
    PDC_LOG(("curses_trace() - called\n"));
    return( rval);
}

void trace( const unsigned param)
{
   curses_trace( param);
}
