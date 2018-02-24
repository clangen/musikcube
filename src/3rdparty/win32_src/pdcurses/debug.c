/* Public Domain Curses */

#include <curspriv.h>

/*man-start**************************************************************

debug
-----

### Synopsis

    void traceon(void);
    void traceoff(void);
    void PDC_debug(const char *, ...);

### Description

   traceon() and traceoff() toggle the recording of debugging
   information to the file "trace". Although not standard, similar
   functions are in some other curses implementations.

   PDC_debug() is the function that writes to the file, based on
   whether traceon() has been called. It's used from the PDC_LOG()
   macro.

   The environment variable PDC_TRACE_FLUSH controls whether the
   trace file contents are fflushed after each write.  The default
   is not. Set it to 1 to enable this (may affect performance).

### Portability
                             X/Open    BSD    SYS V
    traceon                     -       -       -
    traceoff                    -       -       -
    PDC_debug                   -       -       -

**man-end****************************************************************/

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>

FILE *pdc_dbfp = NULL;
static bool trace_on = FALSE;
static bool want_fflush = FALSE;

void PDC_debug(const char *fmt, ...)
{
    va_list args;
    char hms[9];
    time_t now;

    if( !trace_on)
        return;
    if (!pdc_dbfp)
    {
        pdc_dbfp = fopen("trace", "a");
        if (!pdc_dbfp)
        {
            fprintf(stderr,
                "PDC_debug(): Unable to open debug log file\n");
            return;
        }
    }

    time(&now);
    strftime(hms, 9, "%H:%M:%S", localtime(&now));
    fprintf(pdc_dbfp, "At: %8.8ld - %s ", (long) clock(), hms);

    va_start(args, fmt);
    vfprintf(pdc_dbfp, fmt, args);
    va_end(args);

    /* If you are crashing and losing debugging information, enable this
     * by setting the environment variable PDC_TRACE_FLUSH to 1. This may
     * impact performance.
     */
    if (want_fflush)
        fflush(pdc_dbfp);

    /* If with PDC_TRACE_FLUSH enabled you are still losing logging in
     * crashes, you may need to add a platform-dependent mechanism to flush
     * the OS buffers as well (such as fsync() on POSIX) -- but expect
     * terrible performance.
     */
}

void traceon(void)
{
    char *env;

    trace_on = TRUE;
    if ((env = getenv("PDC_TRACE_FLUSH")))
        want_fflush = atoi(env);

    PDC_LOG(("traceon() - called\n"));
}

void traceoff(void)
{
    if (!pdc_dbfp)
        return;

    PDC_LOG(("traceoff() - called\n"));

    fclose(pdc_dbfp);
    pdc_dbfp = NULL;
    want_fflush = trace_on = FALSE;
}
