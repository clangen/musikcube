/* PDCurses */

/* Common pieces between DOS and DOSVGA 'pdcutil' functions */

void PDC_beep(void)
{
    PDCREGS regs;

    PDC_LOG(("PDC_beep() - called\n"));

    regs.W.ax = 0x0e07;       /* Write ^G in TTY fashion */
    regs.W.bx = 0;
    PDCINT(0x10, regs);
}

#define MAX_TICK       0x1800b0L
        /* no. of IRQ 0 clock ticks per day;  BIOS counter (0:0x46c) will go
           to MAX_TICK - 1 before wrapping to 0 at midnight */

#define MS_PER_DAY     86400000L

/* 1080 seconds = 18 minutes = 1/80 day is exactly 19663 ticks.
If asked to nap for longer than 1080000 milliseconds,  we take
one or more 18-minute naps.  This avoids wraparound issues and
the integer overflows that would result for ms > MAX_LONG / 859
(about 42 minutes).  */

#define MAX_NAP_SPAN      (MS_PER_DAY / 80ul)

void PDC_napmsl( long ms)
{
    long tick0, ticks_to_wait;

    PDC_LOG(("PDC_napms() - called: ms=%d\n", ms));

    while( ms > MAX_NAP_SPAN)
    {
         PDC_napmsl( MAX_NAP_SPAN);
         ms -= MAX_NAP_SPAN;
    }
        /* We should convert from milliseconds to BIOS ticks by
           multiplying by MAX_TICK and dividing by MS_PER_DAY.  But
           that would overflow,  and we'd need floating point math.
           The following is good to four parts per billion and
           doesn't overflow (because 0 <= ms <= MAX_NAP_SPAN). */
    ticks_to_wait = (ms * 859L) / 47181L + 1L;
    tick0 = getdosmemdword(0x46c);
    for( ;;)
    {
        long ticks_elapsed = getdosmemdword(0x46c) - tick0;
        PDCREGS regs;

        if( ticks_elapsed < 0L)     /*  midnight rollover */
            ticks_elapsed += MAX_TICK;
        if (ticks_elapsed > ticks_to_wait)
            return;

        regs.W.ax = 0x1680;
        PDCINT(0x2f, regs);
        PDCINT(0x28, regs);
    }
}

void PDC_napms( int ms)
{
    PDC_napmsl( (long)ms);
}

#ifdef __DJGPP__

unsigned char getdosmembyte(int offset)
{
    unsigned char b;

    dosmemget(offset, sizeof(unsigned char), &b);
    return b;
}

unsigned short getdosmemword(int offset)
{
    unsigned short w;

    dosmemget(offset, sizeof(unsigned short), &w);
    return w;
}

unsigned long getdosmemdword(int offset)
{
    unsigned long dw;

    dosmemget(offset, sizeof(unsigned long), &dw);
    return dw;
}

void setdosmembyte(int offset, unsigned char b)
{
    dosmemput(&b, sizeof(unsigned char), offset);
}

void setdosmemword(int offset, unsigned short w)
{
    dosmemput(&w, sizeof(unsigned short), offset);
}

void setdosmemdword(int offset, unsigned long d)
{
    dosmemput(&d, sizeof(unsigned long), offset);
}
#endif

#if defined(__WATCOMC__) && defined(__386__)

void PDC_dpmi_int(int vector, pdc_dpmi_regs *rmregs)
{
    union REGPACK regs = {0};

    rmregs->w.ss = 0;
    rmregs->w.sp = 0;
    rmregs->w.flags = 0;

    regs.w.ax = 0x300;
    regs.h.bl = vector;
    regs.x.edi = FP_OFF(rmregs);
    regs.x.es = FP_SEG(rmregs);

    intr(0x31, &regs);
}

#endif
