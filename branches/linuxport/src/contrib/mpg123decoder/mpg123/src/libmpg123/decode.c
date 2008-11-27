/*
	decode.c: decoding samples...

	copyright 1995-2006 by the mpg123 project - free software under the terms of the LGPL 2.1
	see COPYING and AUTHORS files in distribution or http://mpg123.org
	initially written by Michael Hipp
*/

#include "mpg123lib_intern.h"

/* 8bit functions silenced for FLOATOUT */

int synth_1to1_8bit(real *bandPtr,int channel, mpg123_handle *fr, int final)
{
  sample_t samples_tmp[64];
  sample_t *tmp1 = samples_tmp + channel;
  int i,ret;

  /* save buffer stuff, trick samples_tmp into there, decode, restore */
  unsigned char *samples = fr->buffer.data;
  int pnt = fr->buffer.fill;
  fr->buffer.data = (unsigned char*) samples_tmp;
  fr->buffer.fill = 0;
  ret = synth_1to1(bandPtr, channel, fr, 0);
  fr->buffer.data = samples; /* restore original value */

  samples += channel + pnt;
  for(i=0;i<32;i++) {
#ifdef FLOATOUT
    *samples = 0;
#else
    *samples = fr->conv16to8[*tmp1>>AUSHIFT];
#endif
    samples += 2;
    tmp1 += 2;
  }
  fr->buffer.fill = pnt + (final ? 64 : 0 );

  return ret;
}

int synth_1to1_8bit_mono(real *bandPtr, mpg123_handle *fr)
{
  sample_t samples_tmp[64];
  sample_t *tmp1 = samples_tmp;
  int i,ret;

  /* save buffer stuff, trick samples_tmp into there, decode, restore */
  unsigned char *samples = fr->buffer.data;
  int pnt = fr->buffer.fill;
  fr->buffer.data = (unsigned char*) samples_tmp;
  fr->buffer.fill = 0;
  ret = synth_1to1(bandPtr,0, fr, 0);
  fr->buffer.data = samples; /* restore original value */

  samples += pnt;
  for(i=0;i<32;i++) {
#ifdef FLOATOUT
    *samples++ = 0;
#else
    *samples++ = fr->conv16to8[*tmp1>>AUSHIFT];
#endif
    tmp1 += 2;
  }
  fr->buffer.fill = pnt + 32;

  return ret;
}

int synth_1to1_8bit_mono2stereo(real *bandPtr, mpg123_handle *fr)
{
  sample_t samples_tmp[64];
  sample_t *tmp1 = samples_tmp;
  int i,ret;

  /* save buffer stuff, trick samples_tmp into there, decode, restore */
  unsigned char *samples = fr->buffer.data;
  int pnt = fr->buffer.fill;
  fr->buffer.data = (unsigned char*) samples_tmp;
  fr->buffer.fill = 0;
  ret = synth_1to1(bandPtr, 0, fr, 0);
  fr->buffer.data = samples; /* restore original value */

  samples += pnt;
  for(i=0;i<32;i++) {
#ifdef FLOATOUT
    *samples++ = 0;
    *samples++ = 0;
#else
    *samples++ = fr->conv16to8[*tmp1>>AUSHIFT];
    *samples++ = fr->conv16to8[*tmp1>>AUSHIFT];
#endif
    tmp1 += 2;
  }
  fr->buffer.fill = pnt + 64;

  return ret;
}

int synth_1to1_mono(real *bandPtr, mpg123_handle *fr)
{
  sample_t samples_tmp[64];
  sample_t *tmp1 = samples_tmp;
  int i,ret;

  /* save buffer stuff, trick samples_tmp into there, decode, restore */
  unsigned char *samples = fr->buffer.data;
  int pnt = fr->buffer.fill;
  fr->buffer.data = (unsigned char*) samples_tmp;
  fr->buffer.fill = 0;
  ret = synth_1to1(bandPtr, 0, fr, 0); /* decode into samples_tmp */
  fr->buffer.data = samples; /* restore original value */

  /* now append samples from samples_tmp */
  samples += pnt; /* just the next mem in frame buffer */
  for(i=0;i<32;i++){
    *( (sample_t *)samples) = *tmp1;
    samples += sizeof(sample_t);
    tmp1 += 2;
  }
  fr->buffer.fill = pnt + 32*sizeof(sample_t);

  return ret;
}


int synth_1to1_mono2stereo(real *bandPtr, mpg123_handle *fr)
{
  int i,ret;
  unsigned char *samples = fr->buffer.data;

  ret = synth_1to1(bandPtr,0,fr,1);
  samples += fr->buffer.fill - 64*sizeof(sample_t);

  for(i=0;i<32;i++) {
    ((sample_t *)samples)[1] = ((sample_t *)samples)[0];
    samples+=2*sizeof(sample_t);
  }

  return ret;
}


int synth_1to1(real *bandPtr,int channel,mpg123_handle *fr, int final)
{
  static const int step = 2;
  sample_t *samples = (sample_t *) (fr->buffer.data+fr->buffer.fill);

  real *b0, **buf; /* (*buf)[0x110]; */
  int clip = 0; 
  int bo1;

  if(fr->have_eq_settings) do_equalizer(bandPtr,channel,fr->equalizer);

  if(!channel) {
    fr->bo[0]--;
    fr->bo[0] &= 0xf;
    buf = fr->real_buffs[0];
  }
  else {
    samples++;
    buf = fr->real_buffs[1];
  }

  if(fr->bo[0] & 0x1) {
    b0 = buf[0];
    bo1 = fr->bo[0];
    dct64(buf[1]+((fr->bo[0]+1)&0xf),buf[0]+fr->bo[0],bandPtr);
  }
  else {
    b0 = buf[1];
    bo1 = fr->bo[0]+1;
    dct64(buf[0]+fr->bo[0],buf[1]+fr->bo[0]+1,bandPtr);
  }


  {
    register int j;
    real *window = opt_decwin(fr) + 16 - bo1;
 
    for (j=16;j;j--,window+=0x10,samples+=step)
    {
      real sum;
      sum  = REAL_MUL(*window++, *b0++);
      sum -= REAL_MUL(*window++, *b0++);
      sum += REAL_MUL(*window++, *b0++);
      sum -= REAL_MUL(*window++, *b0++);
      sum += REAL_MUL(*window++, *b0++);
      sum -= REAL_MUL(*window++, *b0++);
      sum += REAL_MUL(*window++, *b0++);
      sum -= REAL_MUL(*window++, *b0++);
      sum += REAL_MUL(*window++, *b0++);
      sum -= REAL_MUL(*window++, *b0++);
      sum += REAL_MUL(*window++, *b0++);
      sum -= REAL_MUL(*window++, *b0++);
      sum += REAL_MUL(*window++, *b0++);
      sum -= REAL_MUL(*window++, *b0++);
      sum += REAL_MUL(*window++, *b0++);
      sum -= REAL_MUL(*window++, *b0++);

      WRITE_SAMPLE(samples,sum,clip);
    }

    {
      real sum;
      sum  = REAL_MUL(window[0x0], b0[0x0]);
      sum += REAL_MUL(window[0x2], b0[0x2]);
      sum += REAL_MUL(window[0x4], b0[0x4]);
      sum += REAL_MUL(window[0x6], b0[0x6]);
      sum += REAL_MUL(window[0x8], b0[0x8]);
      sum += REAL_MUL(window[0xA], b0[0xA]);
      sum += REAL_MUL(window[0xC], b0[0xC]);
      sum += REAL_MUL(window[0xE], b0[0xE]);
      WRITE_SAMPLE(samples,sum,clip);
      b0-=0x10,window-=0x20,samples+=step;
    }
    window += bo1<<1;

    for (j=15;j;j--,b0-=0x20,window-=0x10,samples+=step)
    {
      real sum;
      sum = -REAL_MUL(*(--window), *b0++);
      sum -= REAL_MUL(*(--window), *b0++);
      sum -= REAL_MUL(*(--window), *b0++);
      sum -= REAL_MUL(*(--window), *b0++);
      sum -= REAL_MUL(*(--window), *b0++);
      sum -= REAL_MUL(*(--window), *b0++);
      sum -= REAL_MUL(*(--window), *b0++);
      sum -= REAL_MUL(*(--window), *b0++);
      sum -= REAL_MUL(*(--window), *b0++);
      sum -= REAL_MUL(*(--window), *b0++);
      sum -= REAL_MUL(*(--window), *b0++);
      sum -= REAL_MUL(*(--window), *b0++);
      sum -= REAL_MUL(*(--window), *b0++);
      sum -= REAL_MUL(*(--window), *b0++);
      sum -= REAL_MUL(*(--window), *b0++);
      sum -= REAL_MUL(*(--window), *b0++);

      WRITE_SAMPLE(samples,sum,clip);
    }
  }

  if(final) fr->buffer.fill += 64*sizeof(sample_t);

  return clip;
}
