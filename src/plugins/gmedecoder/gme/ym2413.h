#pragma once

#ifndef __YM2413_H__
#define __YM2413_H__

#ifdef __cplusplus
extern "C" {
#endif

/* select output bits size of output : 8 or 16 */
#define SAMPLE_BITS 16

#include "mamedef.h"

typedef stream_sample_t SAMP;
/*
#if (SAMPLE_BITS==16)
typedef INT16 SAMP;
#endif
#if (SAMPLE_BITS==8)
typedef INT8 SAMP;
#endif
*/



void *ym2413_init(int clock, int rate, int type);
void ym2413_shutdown(void *chip);
void ym2413_reset_chip(void *chip);
void ym2413_write(void *chip, int a, int v);
unsigned char ym2413_read(void *chip, int a);
void ym2413_update_one(void *chip, SAMP **buffers, int length);

void ym2413_advance_lfo(void *chip);    /* call this once */
SAMP ym2413_calcch(void *chip, int ch); /* then call this for each channel */
void ym2413_advance(void *chip);        /* then call this */

void * ym2413_get_inst0(void *chip);

void ym2413_set_mask(void *chip, UINT32 mask);

typedef void (*OPLL_UPDATEHANDLER)(void *param,int min_interval_us);

void ym2413_set_update_handler(void *chip, OPLL_UPDATEHANDLER UpdateHandler, void *param);

#ifdef __cplusplus
}
#endif

#endif /*__YM2413_H__*/
