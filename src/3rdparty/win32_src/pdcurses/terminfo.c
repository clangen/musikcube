/* PDCurses */

#include <curspriv.h>

/*man-start**************************************************************

terminfo
--------

### Synopsis

    int vidattr(chtype attr);
    int vid_attr(attr_t attr, short color_pair, void *opt);
    int vidputs(chtype attr, int (*putfunc)(int));
    int vid_puts(attr_t attr, short color_pair, void *opt,
    int (*putfunc)(int));

    int del_curterm(TERMINAL *);
    int putp(const char *);
    int restartterm(const char *, int, int *);
    TERMINAL *set_curterm(TERMINAL *);
    int setterm(const char *term);
    int setupterm(const char *, int, int *);
    int tgetent(char *, const char *);
    int tgetflag(const char *);
    int tgetnum(const char *);
    char *tgetstr(const char *, char **);
    char *tgoto(const char *, int, int);
    int tigetflag(const char *);
    int tigetnum(const char *);
    char *tigetstr(const char *);
    char *tparm(const char *,long, long, long, long, long, long,
                long, long, long);
    int tputs(const char *, int, int (*)(int));

### Description

   These functions are currently implemented as stubs,
   returning the appropriate errors and doing nothing else.
   They are only compiled and used for certain ncurses tests.

### Portability
                             X/Open    BSD    SYS V
    mvcur                       Y       Y       Y

**man-end****************************************************************/

#include <term.h>

TERMINAL *cur_term = NULL;

int vidattr(chtype attr)
{
    PDC_LOG(("vidattr() - called: attr %d\n", attr));

    INTENTIONALLY_UNUSED_PARAMETER( attr);
    return ERR;
}

int vid_attr(attr_t attr, short color_pair, void *opt)
{
    PDC_LOG(("vid_attr() - called\n"));

    INTENTIONALLY_UNUSED_PARAMETER( attr);
    INTENTIONALLY_UNUSED_PARAMETER( color_pair);
    INTENTIONALLY_UNUSED_PARAMETER( opt);
    return ERR;
}

int vidputs(chtype attr, int (*putfunc)(int))
{
    PDC_LOG(("vidputs() - called: attr %d\n", attr));

    INTENTIONALLY_UNUSED_PARAMETER( attr);
    INTENTIONALLY_UNUSED_PARAMETER( putfunc);
    return ERR;
}

int vid_puts(attr_t attr, short color_pair, void *opt, int (*putfunc)(int))
{
    PDC_LOG(("vid_puts() - called\n"));

    INTENTIONALLY_UNUSED_PARAMETER( attr);
    INTENTIONALLY_UNUSED_PARAMETER( color_pair);
    INTENTIONALLY_UNUSED_PARAMETER( opt);
    INTENTIONALLY_UNUSED_PARAMETER( putfunc);
    return ERR;
}

int del_curterm(TERMINAL *oterm)
{
    PDC_LOG(("del_curterm() - called\n"));

    INTENTIONALLY_UNUSED_PARAMETER( oterm);
    return ERR;
}

int putp(const char *str)
{
    PDC_LOG(("putp() - called: str %s\n", str));

    INTENTIONALLY_UNUSED_PARAMETER( str);
    return ERR;
}

int restartterm(const char *term, int filedes, int *errret)
{
    PDC_LOG(("restartterm() - called\n"));

    if (errret)
        *errret = -1;

    INTENTIONALLY_UNUSED_PARAMETER( term);
    INTENTIONALLY_UNUSED_PARAMETER( filedes);
    return ERR;
}

TERMINAL *set_curterm(TERMINAL *nterm)
{
    PDC_LOG(("set_curterm() - called\n"));

    INTENTIONALLY_UNUSED_PARAMETER( nterm);
    return (TERMINAL *)NULL;
}

int setterm(const char *term)
{
    PDC_LOG(("setterm() - called\n"));

    INTENTIONALLY_UNUSED_PARAMETER( term);
    return ERR;
}

int setupterm(const char *term, int filedes, int *errret)
{
    PDC_LOG(("setupterm() - called\n"));

    if (errret)
        *errret = -1;
    else
        fprintf(stderr, "There is no terminfo database\n");

    INTENTIONALLY_UNUSED_PARAMETER( term);
    INTENTIONALLY_UNUSED_PARAMETER( filedes);
    return ERR;
}

int tgetent(char *bp, const char *name)
{
    PDC_LOG(("tgetent() - called: name %s\n", name));

    INTENTIONALLY_UNUSED_PARAMETER( bp);
    INTENTIONALLY_UNUSED_PARAMETER( name);
    return ERR;
}

int tgetflag(const char *id)
{
    PDC_LOG(("tgetflag() - called: id %s\n", id));

    INTENTIONALLY_UNUSED_PARAMETER( id);
    return ERR;
}

int tgetnum(const char *id)
{
    PDC_LOG(("tgetnum() - called: id %s\n", id));

    INTENTIONALLY_UNUSED_PARAMETER( id);
    return ERR;
}

char *tgetstr(const char *id, char **area)
{
    PDC_LOG(("tgetstr() - called: id %s\n", id));

    INTENTIONALLY_UNUSED_PARAMETER( id);
    INTENTIONALLY_UNUSED_PARAMETER( area);
    return (char *)NULL;
}

char *tgoto(const char *cap, int col, int row)
{
    PDC_LOG(("tgoto() - called\n"));

    INTENTIONALLY_UNUSED_PARAMETER( cap);
    INTENTIONALLY_UNUSED_PARAMETER( col);
    INTENTIONALLY_UNUSED_PARAMETER( row);
    return (char *)NULL;
}

int tigetflag(const char *capname)
{
    PDC_LOG(("tigetflag() - called: capname %s\n", capname));

    INTENTIONALLY_UNUSED_PARAMETER( capname);
    return -1;
}

int tigetnum(const char *capname)
{
    PDC_LOG(("tigetnum() - called: capname %s\n", capname));

    INTENTIONALLY_UNUSED_PARAMETER( capname);
    return -2;
}

char *tigetstr(const char *capname)
{
    PDC_LOG(("tigetstr() - called: capname %s\n", capname));

    INTENTIONALLY_UNUSED_PARAMETER( capname);
    return (char *)(-1);
}

char *tparm(const char *cap, long p1, long p2, long p3, long p4,
            long p5, long p6, long p7, long p8, long p9)
{
    PDC_LOG(("tparm() - called: cap %s\n", cap));
    INTENTIONALLY_UNUSED_PARAMETER( cap);
    INTENTIONALLY_UNUSED_PARAMETER( p1);
    INTENTIONALLY_UNUSED_PARAMETER( p2);
    INTENTIONALLY_UNUSED_PARAMETER( p3);
    INTENTIONALLY_UNUSED_PARAMETER( p4);
    INTENTIONALLY_UNUSED_PARAMETER( p5);
    INTENTIONALLY_UNUSED_PARAMETER( p6);
    INTENTIONALLY_UNUSED_PARAMETER( p7);
    INTENTIONALLY_UNUSED_PARAMETER( p8);
    INTENTIONALLY_UNUSED_PARAMETER( p9);

    return (char *)NULL;
}

int tputs(const char *str, int affcnt, int (*putfunc)(int))
{
    PDC_LOG(("tputs() - called\n"));

    INTENTIONALLY_UNUSED_PARAMETER( str);
    INTENTIONALLY_UNUSED_PARAMETER( affcnt);
    INTENTIONALLY_UNUSED_PARAMETER( putfunc);
    return ERR;
}
