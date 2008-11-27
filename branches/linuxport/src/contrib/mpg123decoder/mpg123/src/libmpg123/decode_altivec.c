/*
	decode.c: decoding samples...

	copyright 1995-2006 by the mpg123 project - free software under the terms of the LGPL 2.1
	see COPYING and AUTHORS files in distribution or http://mpg123.org
	initially written by Michael Hipp
	altivec optimization by tmkk
*/

#include "mpg123lib_intern.h"

#ifndef __APPLE__
#include <altivec.h>
#endif

#define WRITE_SAMPLE(samples,sum,clip) \
  if( (sum) > REAL_PLUS_32767) { *(samples) = 0x7fff; (clip)++; } \
  else if( (sum) < REAL_MINUS_32768) { *(samples) = -0x8000; (clip)++; } \
  else { *(samples) = REAL_TO_SHORT(sum); }

int synth_1to1_8bit_altivec(real *bandPtr,int channel, mpg123_handle *fr, int final)
{
  short samples_tmp[64];
  short *tmp1 = samples_tmp + channel;
  int i,ret;

  unsigned char *samples = fr->buffer.data;
  int pnt = fr->buffer.fill;
  fr->buffer.data = (unsigned char*) samples_tmp;
  fr->buffer.fill = 0;
  ret = synth_1to1_altivec(bandPtr, channel, fr, 0);
  fr->buffer.data = samples;

  samples += channel + pnt;
  for(i=0;i<32;i++) {
    *samples = fr->conv16to8[*tmp1>>AUSHIFT];
    samples += 2;
    tmp1 += 2;
  }
  fr->buffer.fill = pnt + (final ? 64 : 0 );

  return ret;
}

int synth_1to1_8bit_mono_altivec(real *bandPtr, mpg123_handle *fr)
{
  sample_t samples_tmp[64];
  sample_t *tmp1 = samples_tmp;
  int i,ret;

  /* save buffer stuff, trick samples_tmp into there, decode, restore */
  unsigned char *samples = fr->buffer.data;
  int pnt = fr->buffer.fill;
  fr->buffer.data = (unsigned char*) samples_tmp;
  fr->buffer.fill = 0;
  ret = synth_1to1_altivec(bandPtr,0, fr, 0);
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

int synth_1to1_8bit_mono2stereo_altivec(real *bandPtr, mpg123_handle *fr)
{
  sample_t samples_tmp[64];
  sample_t *tmp1 = samples_tmp;
  int i,ret;

  /* save buffer stuff, trick samples_tmp into there, decode, restore */
  unsigned char *samples = fr->buffer.data;
  int pnt = fr->buffer.fill;
  fr->buffer.data = (unsigned char*) samples_tmp;
  fr->buffer.fill = 0;
  ret = synth_1to1_altivec(bandPtr, 0, fr, 0);
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

int synth_1to1_mono_altivec(real *bandPtr, mpg123_handle *fr)
{
  sample_t samples_tmp[64];
  sample_t *tmp1 = samples_tmp;
  int i,ret;

  /* save buffer stuff, trick samples_tmp into there, decode, restore */
  unsigned char *samples = fr->buffer.data;
  int pnt = fr->buffer.fill;
  fr->buffer.data = (unsigned char*) samples_tmp;
  fr->buffer.fill = 0;
  ret = synth_1to1_altivec(bandPtr, 0, fr, 0); /* decode into samples_tmp */
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


int synth_1to1_mono2stereo_altivec(real *bandPtr, mpg123_handle *fr)
{
  int i,ret;
  unsigned char *samples = fr->buffer.data;

  ret = synth_1to1_altivec(bandPtr,0,fr,1);
  samples += fr->buffer.fill - 64*sizeof(sample_t);

  for(i=0;i<32;i++) {
    ((sample_t *)samples)[1] = ((sample_t *)samples)[0];
    samples+=2*sizeof(sample_t);
  }

  return ret;
}


int synth_1to1_altivec(real *bandPtr,int channel,mpg123_handle *fr, int final)
{
  static const int step = 2;
  sample_t *samples = (sample_t *) (fr->buffer.data+fr->buffer.fill);

  real *b0, **buf;
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
    dct64_altivec(buf[1]+((fr->bo[0]+1)&0xf),buf[0]+fr->bo[0],bandPtr);
  }
  else {
    b0 = buf[1];
    bo1 = fr->bo[0]+1;
    dct64_altivec(buf[0]+fr->bo[0],buf[1]+fr->bo[0]+1,bandPtr);
  }


  {
    register int j;
    real *window = opt_decwin(fr) + 16 - bo1;
		
		ALIGNED(16) int clip_tmp[4];
		vector float v1,v2,v3,v4,v5,v6,v7,v8,v9;
		vector unsigned char vperm1,vperm2,vperm3,vperm4,vperm5;
		vector float vsum,vsum2,vsum3,vsum4,vmin,vmax;
		vector signed int vclip;
		vector signed short vsample1,vsample2;
		vclip = vec_xor(vclip,vclip);
#ifdef __APPLE__
		vmax = (vector float)(32767.0f);
		vmin = (vector float)(-32768.0f);
		vperm5 = (vector unsigned char)(0,1,18,19,2,3,22,23,4,5,26,27,6,7,30,31);
#else
		vmax = (vector float){32767.0f,32767.0f,32767.0f,32767.0f};
		vmin = (vector float){-32768.0f,-32768.0f,-32768.0f,-32768.0f};
		vperm5 = (vector unsigned char){0,1,18,19,2,3,22,23,4,5,26,27,6,7,30,31};
#endif
		
		vperm1 = vec_lvsl(0,window);
		vperm3 = vec_lvsl(0,samples);
		vperm4 = vec_lvsr(0,samples);
		for (j=4;j;j--)
		{
			vsum = vec_xor(vsum,vsum);
			vsum2 = vec_xor(vsum2,vsum2);
			vsum3 = vec_xor(vsum3,vsum3);
			vsum4 = vec_xor(vsum4,vsum4);
			v1 = vec_ld(0,window);
			v2 = vec_ld(16,window);
			v3 = vec_ld(32,window);
			v4 = vec_ld(48,window);
			v5 = vec_ld(64,window);
			v1 = vec_perm(v1,v2,vperm1);
			v6 = vec_ld(0,b0);
			v2 = vec_perm(v2,v3,vperm1);
			v7 = vec_ld(16,b0);
			v3 = vec_perm(v3,v4,vperm1);
			v8 = vec_ld(32,b0);
			v4 = vec_perm(v4,v5,vperm1);
			v9 = vec_ld(48,b0);
			
			vsum = vec_madd(v1,v6,vsum);
			vsum = vec_madd(v2,v7,vsum);
			vsum = vec_madd(v3,v8,vsum);
			vsum = vec_madd(v4,v9,vsum);
			
			window += 32;
			b0 += 16;
			
			v1 = vec_ld(0,window);
			v2 = vec_ld(16,window);
			v3 = vec_ld(32,window);
			v4 = vec_ld(48,window);
			v5 = vec_ld(64,window);
			v1 = vec_perm(v1,v2,vperm1);
			v6 = vec_ld(0,b0);
			v2 = vec_perm(v2,v3,vperm1);
			v7 = vec_ld(16,b0);
			v3 = vec_perm(v3,v4,vperm1);
			v8 = vec_ld(32,b0);
			v4 = vec_perm(v4,v5,vperm1);
			v9 = vec_ld(48,b0);
			
			vsum2 = vec_madd(v1,v6,vsum2);
			vsum2 = vec_madd(v2,v7,vsum2);
			vsum2 = vec_madd(v3,v8,vsum2);
			vsum2 = vec_madd(v4,v9,vsum2);
			
			window += 32;
			b0 += 16;
			
			v1 = vec_ld(0,window);
			v2 = vec_ld(16,window);
			v3 = vec_ld(32,window);
			v4 = vec_ld(48,window);
			v5 = vec_ld(64,window);
			v1 = vec_perm(v1,v2,vperm1);
			v6 = vec_ld(0,b0);
			v2 = vec_perm(v2,v3,vperm1);
			v7 = vec_ld(16,b0);
			v3 = vec_perm(v3,v4,vperm1);
			v8 = vec_ld(32,b0);
			v4 = vec_perm(v4,v5,vperm1);
			v9 = vec_ld(48,b0);
			
			vsum3 = vec_madd(v1,v6,vsum3);
			vsum3 = vec_madd(v2,v7,vsum3);
			vsum3 = vec_madd(v3,v8,vsum3);
			vsum3 = vec_madd(v4,v9,vsum3);
			
			window += 32;
			b0 += 16;
			
			v1 = vec_ld(0,window);
			v2 = vec_ld(16,window);
			v3 = vec_ld(32,window);
			v4 = vec_ld(48,window);
			v5 = vec_ld(64,window);
			v1 = vec_perm(v1,v2,vperm1);
			v6 = vec_ld(0,b0);
			v2 = vec_perm(v2,v3,vperm1);
			v7 = vec_ld(16,b0);
			v3 = vec_perm(v3,v4,vperm1);
			v8 = vec_ld(32,b0);
			v4 = vec_perm(v4,v5,vperm1);
			v9 = vec_ld(48,b0);
			
			vsum4 = vec_madd(v1,v6,vsum4);
			vsum4 = vec_madd(v2,v7,vsum4);
			vsum4 = vec_madd(v3,v8,vsum4);
			vsum4 = vec_madd(v4,v9,vsum4);
			
			window += 32;
			b0 += 16;
			
			v1 = vec_mergeh(vsum,vsum3);
			v2 = vec_mergeh(vsum2,vsum4);
			v3 = vec_mergel(vsum,vsum3);
			v4 = vec_mergel(vsum2,vsum4);
			v5 = vec_mergeh(v1,v2);
			v6 = vec_mergel(v1,v2);
			v7 = vec_mergeh(v3,v4);
			v8 = vec_mergel(v3,v4);
			
			vsum = vec_sub(v5,v6);
			v9 = vec_sub(v7,v8);
			vsum = vec_add(vsum,v9);
			
			v3 = (vector float)vec_cts(vsum,0);
			v1 = (vector float)vec_cmpgt(vsum,vmax);
			v2 = (vector float)vec_cmplt(vsum,vmin);
			vsample1 = vec_ld(0,samples);
			vsample2 = vec_ld(15,samples);
			v3 = (vector float)vec_packs((vector signed int)v3,(vector signed int)v3);
			v4 = (vector float)vec_perm(vsample1,vsample2,vperm3);
			v5 = (vector float)vec_perm(v3,v4,vperm5);
			v6 = (vector float)vec_perm(vsample2,vsample1,vperm3);
			v7 = (vector float)vec_perm(v5,v6,vperm4);
			v8 = (vector float)vec_perm(v6,v5,vperm4);
			vec_st((vector signed short)v7,15,samples);
			vec_st((vector signed short)v8,0,samples);
			samples += 8;
#ifdef __APPLE__
			v1 = (vector float)vec_sr((vector unsigned int)v1,(vector unsigned int)(31));
			v2 = (vector float)vec_sr((vector unsigned int)v2,(vector unsigned int)(31));
#else
			v1 = (vector float)vec_sr((vector unsigned int)v1,(vector unsigned int){31,31,31,31});
			v2 = (vector float)vec_sr((vector unsigned int)v2,(vector unsigned int){31,31,31,31});
#endif
			v5 = (vector float)vec_add((vector unsigned int)v1,(vector unsigned int)v2);
			vclip = vec_sums((vector signed int)v5,vclip);
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
		
		vperm1 = vec_lvsl(0,window);
#ifdef __APPLE__
		vperm2 = vec_perm(vperm1,vperm1,(vector unsigned char)(12,13,14,15,8,9,10,11,4,5,6,7,0,1,2,3));
#else
		vperm2 = vec_perm(vperm1,vperm1,(vector unsigned char){12,13,14,15,8,9,10,11,4,5,6,7,0,1,2,3});
#endif
		vperm3 = vec_lvsl(0,samples);
		vperm4 = vec_lvsr(0,samples);
		for (j=3;j;j--)
		{
			vsum = vec_xor(vsum,vsum);
			vsum2 = vec_xor(vsum2,vsum2);
			vsum3 = vec_xor(vsum3,vsum3);
			vsum4 = vec_xor(vsum4,vsum4);
			v1 = vec_ld(-1,window);
			v2 = vec_ld(-16,window);
			v3 = vec_ld(-32,window);
			v4 = vec_ld(-48,window);
			v5 = vec_ld(-64,window);
			v1 = vec_perm(v2,v1,vperm2);
			v6 = vec_ld(0,b0);
			v2 = vec_perm(v3,v2,vperm2);
			v7 = vec_ld(16,b0);
			v3 = vec_perm(v4,v3,vperm2);
			v8 = vec_ld(32,b0);
			v4 = vec_perm(v5,v4,vperm2);
			v9 = vec_ld(48,b0);
			
			vsum = vec_nmsub(v1,v6,vsum);
			vsum = vec_nmsub(v2,v7,vsum);
			vsum = vec_nmsub(v3,v8,vsum);
			vsum = vec_nmsub(v4,v9,vsum);
			
			window -= 32;
			b0 -= 16;
			
			v1 = vec_ld(0,window);
			v2 = vec_ld(-16,window);
			v3 = vec_ld(-32,window);
			v4 = vec_ld(-48,window);
			v5 = vec_ld(-64,window);
			v1 = vec_perm(v2,v1,vperm2);
			v6 = vec_ld(0,b0);
			v2 = vec_perm(v3,v2,vperm2);
			v7 = vec_ld(16,b0);
			v3 = vec_perm(v4,v3,vperm2);
			v8 = vec_ld(32,b0);
			v4 = vec_perm(v5,v4,vperm2);
			v9 = vec_ld(48,b0);
			
			vsum2 = vec_nmsub(v1,v6,vsum2);
			vsum2 = vec_nmsub(v2,v7,vsum2);
			vsum2 = vec_nmsub(v3,v8,vsum2);
			vsum2 = vec_nmsub(v4,v9,vsum2);
			
			window -= 32;
			b0 -= 16;
			
			v1 = vec_ld(0,window);
			v2 = vec_ld(-16,window);
			v3 = vec_ld(-32,window);
			v4 = vec_ld(-48,window);
			v5 = vec_ld(-64,window);
			v1 = vec_perm(v2,v1,vperm2);
			v6 = vec_ld(0,b0);
			v2 = vec_perm(v3,v2,vperm2);
			v7 = vec_ld(16,b0);
			v3 = vec_perm(v4,v3,vperm2);
			v8 = vec_ld(32,b0);
			v4 = vec_perm(v5,v4,vperm2);
			v9 = vec_ld(48,b0);
			
			vsum3 = vec_nmsub(v1,v6,vsum3);
			vsum3 = vec_nmsub(v2,v7,vsum3);
			vsum3 = vec_nmsub(v3,v8,vsum3);
			vsum3 = vec_nmsub(v4,v9,vsum3);
			
			window -= 32;
			b0 -= 16;
			
			v1 = vec_ld(0,window);
			v2 = vec_ld(-16,window);
			v3 = vec_ld(-32,window);
			v4 = vec_ld(-48,window);
			v5 = vec_ld(-64,window);
			v1 = vec_perm(v2,v1,vperm2);
			v6 = vec_ld(0,b0);
			v2 = vec_perm(v3,v2,vperm2);
			v7 = vec_ld(16,b0);
			v3 = vec_perm(v4,v3,vperm2);
			v8 = vec_ld(32,b0);
			v4 = vec_perm(v5,v4,vperm2);
			v9 = vec_ld(48,b0);
			
			vsum4 = vec_nmsub(v1,v6,vsum4);
			vsum4 = vec_nmsub(v2,v7,vsum4);
			vsum4 = vec_nmsub(v3,v8,vsum4);
			vsum4 = vec_nmsub(v4,v9,vsum4);
			
			window -= 32;
			b0 -= 16;
			
			v1 = vec_mergeh(vsum,vsum3);
			v2 = vec_mergeh(vsum2,vsum4);
			v3 = vec_mergel(vsum,vsum3);
			v4 = vec_mergel(vsum2,vsum4);
			v5 = vec_mergeh(v1,v2);
			v6 = vec_mergel(v1,v2);
			v7 = vec_mergeh(v3,v4);
			v8 = vec_mergel(v3,v4);
			
			vsum = vec_add(v5,v6);
			v9 = vec_add(v7,v8);
			vsum = vec_add(vsum,v9);
			
			v3 = (vector float)vec_cts(vsum,0);
			v1 = (vector float)vec_cmpgt(vsum,vmax);
			v2 = (vector float)vec_cmplt(vsum,vmin);
			vsample1 = vec_ld(0,samples);
			vsample2 = vec_ld(15,samples);
			v3 = (vector float)vec_packs((vector signed int)v3,(vector signed int)v3);
			v4 = (vector float)vec_perm(vsample1,vsample2,vperm3);
			v5 = (vector float)vec_perm(v3,v4,vperm5);
			v6 = (vector float)vec_perm(vsample2,vsample1,vperm3);
			v7 = (vector float)vec_perm(v5,v6,vperm4);
			v8 = (vector float)vec_perm(v6,v5,vperm4);
			vec_st((vector signed short)v7,15,samples);
			vec_st((vector signed short)v8,0,samples);
			samples += 8;
#ifdef __APPLE__
			v1 = (vector float)vec_sr((vector unsigned int)v1,(vector unsigned int)(31));
			v2 = (vector float)vec_sr((vector unsigned int)v2,(vector unsigned int)(31));
#else
			v1 = (vector float)vec_sr((vector unsigned int)v1,(vector unsigned int){31,31,31,31});
			v2 = (vector float)vec_sr((vector unsigned int)v2,(vector unsigned int){31,31,31,31});
#endif
			v5 = (vector float)vec_add((vector unsigned int)v1,(vector unsigned int)v2);
			vclip = vec_sums((vector signed int)v5,vclip);
		}
#ifdef __APPLE__
		vperm5 = (vector unsigned char)(0,1,18,19,2,3,22,23,4,5,26,27,28,29,30,31);
#else
		vperm5 = (vector unsigned char){0,1,18,19,2,3,22,23,4,5,26,27,28,29,30,31};
#endif
		{
			vsum = vec_xor(vsum,vsum);
			vsum2 = vec_xor(vsum2,vsum2);
			vsum3 = vec_xor(vsum3,vsum3);
			vsum4 = vec_xor(vsum4,vsum4);
			v1 = vec_ld(-1,window);
			v2 = vec_ld(-16,window);
			v3 = vec_ld(-32,window);
			v4 = vec_ld(-48,window);
			v5 = vec_ld(-64,window);
			v1 = vec_perm(v2,v1,vperm2);
			v6 = vec_ld(0,b0);
			v2 = vec_perm(v3,v2,vperm2);
			v7 = vec_ld(16,b0);
			v3 = vec_perm(v4,v3,vperm2);
			v8 = vec_ld(32,b0);
			v4 = vec_perm(v5,v4,vperm2);
			v9 = vec_ld(48,b0);
			
			vsum = vec_nmsub(v1,v6,vsum);
			vsum = vec_nmsub(v2,v7,vsum);
			vsum = vec_nmsub(v3,v8,vsum);
			vsum = vec_nmsub(v4,v9,vsum);
			
			window -= 32;
			b0 -= 16;
			
			v1 = vec_ld(0,window);
			v2 = vec_ld(-16,window);
			v3 = vec_ld(-32,window);
			v4 = vec_ld(-48,window);
			v5 = vec_ld(-64,window);
			v1 = vec_perm(v2,v1,vperm2);
			v6 = vec_ld(0,b0);
			v2 = vec_perm(v3,v2,vperm2);
			v7 = vec_ld(16,b0);
			v3 = vec_perm(v4,v3,vperm2);
			v8 = vec_ld(32,b0);
			v4 = vec_perm(v5,v4,vperm2);
			v9 = vec_ld(48,b0);
			
			vsum2 = vec_nmsub(v1,v6,vsum2);
			vsum2 = vec_nmsub(v2,v7,vsum2);
			vsum2 = vec_nmsub(v3,v8,vsum2);
			vsum2 = vec_nmsub(v4,v9,vsum2);
			
			window -= 32;
			b0 -= 16;
			
			v1 = vec_ld(0,window);
			v2 = vec_ld(-16,window);
			v3 = vec_ld(-32,window);
			v4 = vec_ld(-48,window);
			v5 = vec_ld(-64,window);
			v1 = vec_perm(v2,v1,vperm2);
			v6 = vec_ld(0,b0);
			v2 = vec_perm(v3,v2,vperm2);
			v7 = vec_ld(16,b0);
			v3 = vec_perm(v4,v3,vperm2);
			v8 = vec_ld(32,b0);
			v4 = vec_perm(v5,v4,vperm2);
			v9 = vec_ld(48,b0);
			
			vsum3 = vec_nmsub(v1,v6,vsum3);
			vsum3 = vec_nmsub(v2,v7,vsum3);
			vsum3 = vec_nmsub(v3,v8,vsum3);
			vsum3 = vec_nmsub(v4,v9,vsum3);
			
			v1 = vec_mergeh(vsum,vsum3);
			v2 = vec_mergeh(vsum2,vsum2);
			v3 = vec_mergel(vsum,vsum3);
			v4 = vec_mergel(vsum2,vsum2);
			v5 = vec_mergeh(v1,v2);
			v6 = vec_mergel(v1,v2);
			v7 = vec_mergeh(v3,v4);
			v8 = vec_mergel(v3,v4);
			
			vsum = vec_add(v5,v6);
			v9 = vec_add(v7,v8);
			vsum = vec_add(vsum,v9);
			
			v3 = (vector float)vec_cts(vsum,0);
			v1 = (vector float)vec_cmpgt(vsum,vmax);
			v2 = (vector float)vec_cmplt(vsum,vmin);
			vsample1 = vec_ld(0,samples);
			vsample2 = vec_ld(15,samples);
			v3 = (vector float)vec_packs((vector signed int)v3,(vector signed int)v3);
			v4 = (vector float)vec_perm(vsample1,vsample2,vperm3);
			v5 = (vector float)vec_perm(v3,v4,vperm5);
			v6 = (vector float)vec_perm(vsample2,vsample1,vperm3);
			v7 = (vector float)vec_perm(v5,v6,vperm4);
			v8 = (vector float)vec_perm(v6,v5,vperm4);
			vec_st((vector signed short)v7,15,samples);
			vec_st((vector signed short)v8,0,samples);
			samples += 6;
#ifdef __APPLE__
			v1 = (vector float)vec_sr((vector unsigned int)v1,(vector unsigned int)(31,31,31,32));
			v2 = (vector float)vec_sr((vector unsigned int)v2,(vector unsigned int)(31,31,31,32));
#else
			v1 = (vector float)vec_sr((vector unsigned int)v1,(vector unsigned int){31,31,31,32});
			v2 = (vector float)vec_sr((vector unsigned int)v2,(vector unsigned int){31,31,31,32});
#endif
			v5 = (vector float)vec_add((vector unsigned int)v1,(vector unsigned int)v2);
			vclip = vec_sums((vector signed int)v5,vclip);
			vec_st(vclip,0,clip_tmp);
			clip += clip_tmp[3];
		}
  }
  if(final) fr->buffer.fill += 128;

  return clip;
}
