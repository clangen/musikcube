/*
    SuperEQ DSP plugin for DeaDBeeF Player
    Copyright (C) 2009-2014 Alexey Yakovenko <waker@users.sourceforge.net>
    Original SuperEQ code (C) Naoki Shibata <shibatch@users.sf.net>

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
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>
#include "paramlist.hpp"
#include "Equ.h"

extern "C" void rdft(int, int, REAL *, int *, REAL *);
void rfft(int n,int isign,REAL *x)
{
    static int ipsize = 0,wsize=0;
    static int *ip = NULL;
    static REAL *w = NULL;
    int newipsize,newwsize;
    if (n == 0) {
        free(ip); ip = NULL; ipsize = 0;
        free(w);  w  = NULL; wsize  = 0;
        return;
    }

    n = 1 << n;


    newipsize = 2+sqrt(n/2);
    if (newipsize > ipsize) {
        ipsize = newipsize;
        ip = (int *)realloc(ip,sizeof(int)*ipsize);
        ip[0] = 0;
    }

    newwsize = n/2;
    if (newwsize > wsize) {
        wsize = newwsize;
        w = (REAL *)realloc(w,sizeof(REAL)*wsize);
    }

    rdft(n,isign,x,ip,w);
}

#define PI 3.1415926535897932384626433832795

#define DITHERLEN 65536

#define M 15
static REAL fact[M+1];
static REAL aa = 96;
static REAL iza = 0;

#define NBANDS 17
static REAL bands[NBANDS] = {
  65.406392,92.498606,130.81278,184.99721,261.62557,369.99442,523.25113,
  739.9884 ,1046.5023,1479.9768,2093.0045,2959.9536,4186.0091,5919.9072,
  8372.0181,11839.814,16744.036
};

static REAL alpha(REAL a)
{
  if (a <= 21) return 0;
  if (a <= 50) return 0.5842*pow(a-21,0.4)+0.07886*(a-21);
  return 0.1102*(a-8.7);
}

static REAL izero(REAL x)
{
  REAL ret = 1;
  int m;

  for(m=1;m<=M;m++)
    {
      REAL t;
      t = pow(x/2,m)/fact[m];
      ret += t*t;
    }

  return ret;
}

void *equ_malloc (int size) {
    return malloc (size);
}

void equ_free (void *mem) {
    free (mem);
}

extern "C" void equ_init(SuperEqState *state, int wb, int channels)
{
  int i,j;

  if (state->lires1 != NULL)   free(state->lires1);
  if (state->lires2 != NULL)   free(state->lires2);
  if (state->irest != NULL)    free(state->irest);
  if (state->fsamples != NULL) free(state->fsamples);
  if (state->finbuf != NULL)    free(state->finbuf);
  if (state->outbuf != NULL)   free(state->outbuf);
  if (state->ditherbuf != NULL) free(state->ditherbuf);


  memset (state, 0, sizeof (SuperEqState));
  state->channels = channels;
  state->enable = 1;

  state->winlen = (1 << (wb-1))-1;
  state->winlenbit = wb;
  state->tabsize  = 1 << wb;
  state->fft_bits = wb;

  state->lires1   = (REAL *)equ_malloc(sizeof(REAL)*state->tabsize * state->channels);
  state->lires2   = (REAL *)equ_malloc(sizeof(REAL)*state->tabsize * state->channels);
  state->irest    = (REAL *)equ_malloc(sizeof(REAL)*state->tabsize);
  state->fsamples = (REAL *)equ_malloc(sizeof(REAL)*state->tabsize);
  state->finbuf    = (REAL *)equ_malloc(state->winlen*state->channels*sizeof(REAL));
  state->outbuf   = (REAL *)equ_malloc(state->tabsize*state->channels*sizeof(REAL));
  state->ditherbuf = (REAL *)equ_malloc(sizeof(REAL)*DITHERLEN);

  memset (state->lires1, 0, sizeof(REAL)*state->tabsize * state->channels);
  memset (state->lires2, 0, sizeof(REAL)*state->tabsize * state->channels);
  memset (state->irest, 0, sizeof(REAL)*state->tabsize);
  memset (state->fsamples, 0, sizeof(REAL)*state->tabsize);
  memset (state->finbuf, 0, state->winlen*state->channels*sizeof(REAL));
  memset (state->outbuf, 0, state->tabsize*state->channels*sizeof(REAL));
  memset (state->ditherbuf, 0, sizeof(REAL)*DITHERLEN);

  state->lires = state->lires1;
  state->cur_ires = 1;
  state->chg_ires = 1;

  for(i=0;i<DITHERLEN;i++)
	state->ditherbuf[i] = (float(rand())/RAND_MAX-0.5);

  if (fact[0] < 1) {
      for(i=0;i<=M;i++)
      {
          fact[i] = 1;
          for(j=1;j<=i;j++) fact[i] *= j;
      }
      iza = izero(alpha(aa));
  }
}

// -(N-1)/2 <= n <= (N-1)/2
static REAL win(REAL n,int N)
{
  return izero(alpha(aa)*sqrt(1-4*n*n/((N-1)*(N-1))))/iza;
}

static REAL sinc(REAL x)
{
  return x == 0 ? 1 : sin(x)/x;
}

static REAL hn_lpf(int n,REAL f,REAL fs)
{
  REAL t = 1/fs;
  REAL omega = 2*PI*f;
  return 2*f*t*sinc(n*omega*t);
}

static REAL hn_imp(int n)
{
  return n == 0 ? 1.0 : 0.0;
}

static REAL hn(int n,paramlist &param2,REAL fs)
{
  paramlistelm *e;
  REAL ret,lhn;

  lhn = hn_lpf(n,param2.elm->upper,fs);
  ret = param2.elm->gain*lhn;

  for(e=param2.elm->next;e->next != NULL && e->upper < fs/2;e = e->next)
    {
      REAL lhn2 = hn_lpf(n,e->upper,fs);
      ret += e->gain*(lhn2-lhn);
      lhn = lhn2;
    }

  ret += e->gain*(hn_imp(n)-lhn);
  
  return ret;
}

void process_param(REAL *bc,paramlist *param,paramlist &param2,REAL fs,int ch)
{
  paramlistelm **pp,*p,*e,*e2;
  int i;

  delete param2.elm;
  param2.elm = NULL;

  for(i=0,pp=&param2.elm;i<=NBANDS;i++,pp = &(*pp)->next)
  {
    (*pp) = new paramlistelm;
	(*pp)->lower = i == 0      ?  0 : bands[i-1];
	(*pp)->upper = i == NBANDS ? fs : bands[i  ];
	(*pp)->gain  = bc[i];
  }
  
  for(e = param->elm;e != NULL;e = e->next)
  {
	if (e->lower >= e->upper) continue;

	for(p=param2.elm;p != NULL;p = p->next)
		if (p->upper > e->lower) break;

	while(p != NULL && p->lower < e->upper)
	{
		if (e->lower <= p->lower && p->upper <= e->upper) {
			p->gain *= pow(10,e->gain/20);
			p = p->next;
			continue;
		}
		if (p->lower < e->lower && e->upper < p->upper) {
			e2 = new paramlistelm;
			e2->lower = e->upper;
			e2->upper = p->upper;
			e2->gain  = p->gain;
			e2->next  = p->next;
			p->next   = e2;

			e2 = new paramlistelm;
			e2->lower = e->lower;
			e2->upper = e->upper;
			e2->gain  = p->gain * pow(10,e->gain/20);
			e2->next  = p->next;
			p->next   = e2;

			p->upper  = e->lower;

			p = p->next->next->next;
			continue;
		}
		if (p->lower < e->lower) {
			e2 = new paramlistelm;
			e2->lower = e->lower;
			e2->upper = p->upper;
			e2->gain  = p->gain * pow(10,e->gain/20);
			e2->next  = p->next;
			p->next   = e2;

			p->upper  = e->lower;
			p = p->next->next;
			continue;
		}
		if (e->upper < p->upper) {
			e2 = new paramlistelm;
			e2->lower = e->upper;
			e2->upper = p->upper;
			e2->gain  = p->gain;
			e2->next  = p->next;
			p->next   = e2;

			p->upper  = e->upper;
			p->gain   = p->gain * pow(10,e->gain/20);
			p = p->next->next;
			continue;
		}
		abort();
	}
  }
}

extern "C" void equ_makeTable(SuperEqState *state, REAL *lbc,void *_param,REAL fs)
{
  paramlist *param = (paramlist *)_param;
  int i,cires = state->cur_ires;
  REAL *nires;

  if (fs <= 0) return;

  paramlist param2;

  for (int ch = 0; ch < state->channels; ch++) {
      process_param(lbc,param,param2,fs,ch);

      for(i=0;i<state->winlen;i++)
          state->irest[i] = hn(i-state->winlen/2,param2,fs)*win(i-state->winlen/2,state->winlen);

      for(;i<state->tabsize;i++)
          state->irest[i] = 0;

      rfft(state->fft_bits,1,state->irest);

      nires = cires == 1 ? state->lires2 : state->lires1;
      nires += ch * state->tabsize;

      for(i=0;i<state->tabsize;i++)
          nires[i] = state->irest[i];
  }
  state->chg_ires = cires == 1 ? 2 : 1;
}

extern "C" void equ_quit(SuperEqState *state)
{
  equ_free(state->lires1);
  equ_free(state->lires2);
  equ_free(state->irest);
  equ_free(state->fsamples);
  equ_free(state->finbuf);
  equ_free(state->outbuf);
  equ_free(state->ditherbuf);

  state->lires1   = NULL;
  state->lires2   = NULL;
  state->irest    = NULL;
  state->fsamples = NULL;
  state->finbuf    = NULL;
  state->outbuf   = NULL;

  rfft(0,0,NULL);
}

extern "C" void equ_clearbuf(SuperEqState *state)
{
	int i;

	state->nbufsamples = 0;
	for(i=0;i<state->tabsize*state->channels;i++) state->outbuf[i] = 0;
}

extern "C" int equ_modifySamples_float (SuperEqState *state, char *buf,int nsamples,int nch)
{
  int i,p,ch;
  REAL *ires;
  float amax = 1.0f;
  float amin = -1.0f;
  static float hm1 = 0, hm2 = 0;

  if (state->chg_ires) {
	  state->cur_ires = state->chg_ires;
	  state->lires = state->cur_ires == 1 ? state->lires1 : state->lires2;
	  state->chg_ires = 0;
  }

  p = 0;

  while(state->nbufsamples+nsamples >= state->winlen)
    {
		for(i=0;i<(state->winlen-state->nbufsamples)*nch;i++)
			{
                state->finbuf[state->nbufsamples*nch+i] = ((float *)buf)[i+p*nch];
				float s = state->outbuf[state->nbufsamples*nch+i];
				//if (dither) s += ditherbuf[(ditherptr++) & (DITHERLEN-1)];
				if (s < amin) s = amin;
				if (amax < s) s = amax;
				((float *)buf)[i+p*nch] = s;
			}
		for(i=state->winlen*nch;i<state->tabsize*nch;i++)
			state->outbuf[i-state->winlen*nch] = state->outbuf[i];


      p += state->winlen-state->nbufsamples;
      nsamples -= state->winlen-state->nbufsamples;
      state->nbufsamples = 0;

      for(ch=0;ch<nch;ch++)
		{
            ires = state->lires + ch * state->tabsize;

            for(i=0;i<state->winlen;i++)
                state->fsamples[i] = state->finbuf[nch*i+ch];

			for(i=state->winlen;i<state->tabsize;i++)
				state->fsamples[i] = 0;

			if (state->enable) {
				rfft(state->fft_bits,1,state->fsamples);

				state->fsamples[0] = ires[0]*state->fsamples[0];
				state->fsamples[1] = ires[1]*state->fsamples[1]; 
			
				for(i=1;i<state->tabsize/2;i++)
					{
						REAL re,im;

						re = ires[i*2  ]*state->fsamples[i*2] - ires[i*2+1]*state->fsamples[i*2+1];
						im = ires[i*2+1]*state->fsamples[i*2] + ires[i*2  ]*state->fsamples[i*2+1];

						state->fsamples[i*2  ] = re;
						state->fsamples[i*2+1] = im;
					}

				rfft(state->fft_bits,-1,state->fsamples);
			} else {
				for(i=state->winlen-1+state->winlen/2;i>=state->winlen/2;i--) state->fsamples[i] = state->fsamples[i-state->winlen/2]*state->tabsize/2;
				for(;i>=0;i--) state->fsamples[i] = 0;
			}

			for(i=0;i<state->winlen;i++) state->outbuf[i*nch+ch] += state->fsamples[i]/state->tabsize*2;

			for(i=state->winlen;i<state->tabsize;i++) state->outbuf[i*nch+ch] = state->fsamples[i]/state->tabsize*2;
		}
    }

		for(i=0;i<nsamples*nch;i++)
			{
				state->finbuf[state->nbufsamples*nch+i] = ((float *)buf)[i+p*nch];
				float s = state->outbuf[state->nbufsamples*nch+i];
				if (state->dither) {
					float u;
					s -= hm1;
					u = s;
//					s += ditherbuf[(ditherptr++) & (DITHERLEN-1)];
					if (s < amin) s = amin;
					if (amax < s) s = amax;
					hm1 = s - u;
					((float *)buf)[i+p*nch] = s;
				} else {
					if (s < amin) s = amin;
					if (amax < s) s = amax;
					((float *)buf)[i+p*nch] = s;
				}
			}

  p += nsamples;
  state->nbufsamples += nsamples;

  return p;
}

extern "C" void *paramlist_alloc (void) {
    return (void *)(new paramlist);
}
extern "C" void paramlist_free (void *pl) {
    delete ((paramlist *)pl);
}
