/*
	decode.h: common definitions for decode functions

	copyright 2007 by the mpg123 project - free software under the terms of the LGPL 2.1
	see COPYING and AUTHORS files in distribution or http://mpg123.org
	initially written by Thomas Orgis, taking WRITE_SAMPLE from decode.c
*/
#ifndef MPG123_DECODE_H
#define MPG123_DECODE_H

#ifdef FLOATOUT
#define WRITE_SAMPLE(samples,sum,clip) *(samples) = sum
#define sample_t float
#else
#define WRITE_SAMPLE(samples,sum,clip) \
  if( (sum) > REAL_PLUS_32767) { *(samples) = 0x7fff; (clip)++; } \
  else if( (sum) < REAL_MINUS_32768) { *(samples) = -0x8000; (clip)++; } \
  else { *(samples) = REAL_TO_SHORT(sum); }
#define sample_t short
#endif

#define NTOM_MAX 8          /* maximum allowed factor for upsampling */
#define NTOM_MAX_FREQ 96000 /* maximum frequency to upsample to / downsample from */
#define NTOM_MUL (32768)

/* synth_1to1 in optimize.h, one should also use opts for these here... */

int synth_2to1 (real *,int, mpg123_handle*, int);
int synth_2to1_8bit (real *,int, mpg123_handle *,int);
int synth_2to1_mono (real *, mpg123_handle *);
int synth_2to1_mono2stereo (real *, mpg123_handle *);
int synth_2to1_8bit_mono (real *, mpg123_handle *);
int synth_2to1_8bit_mono2stereo (real *, mpg123_handle *);

int synth_4to1 (real *,int, mpg123_handle*, int);
int synth_4to1_8bit (real *,int, mpg123_handle *,int);
int synth_4to1_mono (real *, mpg123_handle *);
int synth_4to1_mono2stereo (real *, mpg123_handle *);
int synth_4to1_8bit_mono (real *, mpg123_handle *);
int synth_4to1_8bit_mono2stereo (real *, mpg123_handle *);

int synth_ntom (real *,int, mpg123_handle*, int);
int synth_ntom_8bit (real *,int, mpg123_handle *,int);
int synth_ntom_mono (real *, mpg123_handle *);
int synth_ntom_mono2stereo (real *, mpg123_handle *);
int synth_ntom_8bit_mono (real *, mpg123_handle *);
int synth_ntom_8bit_mono2stereo (real *, mpg123_handle *);

int synth_ntom_set_step(mpg123_handle *fr); /* prepare ntom decoding */
unsigned long ntom_val(mpg123_handle *fr, off_t frame); /* compute ntom_val for frame offset */

off_t ntom_frmouts(mpg123_handle *fr, off_t frame);
off_t ntom_ins2outs(mpg123_handle *fr, off_t ins);
off_t ntom_frameoff(mpg123_handle *fr, off_t soff);

void init_layer3(void);
void init_layer3_stuff(mpg123_handle *fr);
void init_layer2(void);
void init_layer2_stuff(mpg123_handle *fr);
int make_conv16to8_table(mpg123_handle *fr);

int do_layer3(mpg123_handle *fr);
int do_layer2(mpg123_handle *fr);
int do_layer1(mpg123_handle *fr);
void do_equalizer(real *bandPtr,int channel, real equalizer[2][32]);

#endif
