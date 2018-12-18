/*
    DeaDBeeF - The Ultimate Music Player
    Copyright (C) 2009-2013 Alexey Yakovenko <waker@users.sourceforge.net>

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License
    as published by the Free Software Foundation; either version 2
    of the License, or (at your option) any later version.
    
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
    
    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/
#ifndef __EQU_H
#define __EQU_H

#ifdef __cplusplus
extern "C" {
#endif

typedef float REAL;
typedef struct {
    REAL *lires,*lires1,*lires2;
    REAL *irest;
    REAL *fsamples;
    REAL *ditherbuf;
    int ditherptr;
    volatile int chg_ires,cur_ires;
    int winlen,winlenbit,tabsize,nbufsamples;
    REAL *finbuf;
    REAL *outbuf;
    int dither;
    int channels;
    int enable;
    int fft_bits;
} SuperEqState;

void *paramlist_alloc (void);
void paramlist_free (void *);
void equ_makeTable(SuperEqState *state, float *lbc,void *param,float fs);
int equ_modifySamples(SuperEqState *state, char *buf,int nsamples,int nch,int bps);
int equ_modifySamples_float (SuperEqState *state, char *buf,int nsamples,int nch);
void equ_clearbuf(SuperEqState *state);
void equ_init(SuperEqState *state, int wb, int channels);
void equ_quit(SuperEqState *state);

#ifdef __cplusplus
}
#endif

#endif
