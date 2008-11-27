#ifndef MPG123_H_OPTIMIZE
#define MPG123_H_OPTIMIZE
/*
	optimize: get a grip on the different optimizations

	copyright 2007 by the mpg123 project - free software under the terms of the LGPL 2.1
	see COPYING and AUTHORS files in distribution or http://mpg123.org
	initially written by Thomas Orgis, taking from mpg123.[hc]

	for building mpg123 with one optimization only, you have to choose exclusively between
	OPT_GENERIC (generic C code for everyone)
	OPT_I386 (Intel i386)
	OPT_I486 (...)
	OPT_I586 (Intel Pentium)
	OPT_I586_DITHER (Intel Pentium with dithering/noise shaping for enhanced quality)
	OPT_MMX (Intel Pentium and compatibles with MMX, fast, but not the best accuracy)
	OPT_3DNOW (AMD 3DNow!, K6-2/3, Athlon, compatibles...)
	OPT_ALTIVEC (Motorola/IBM PPC with AltiVec under MacOSX)

	or you define OPT_MULTI and give a combination which makes sense (do not include i486, do not mix altivec and x86).

	I still have to examine the dynamics of this here together with REAL_IS_FIXED.
*/

/* this is included in mpg123.h, which includes config.h */
#ifdef CCALIGN
#define ALIGNED(a) __attribute__((aligned(a)))
#else
#define ALIGNED(a)
#endif

/* the optimizations only cover the synth1to1 mode and the dct36 function */
/* the first two types are needed in set_synth_functions regardless of optimizations */
typedef int (*func_synth)(real *,int, mpg123_handle *,int );
typedef int (*func_synth_mono)(real *, mpg123_handle *);
typedef void (*func_dct36)(real *,real *,real *,real *,real *);
typedef	void (*func_dct64)(real *,real *,real *);
typedef void (*func_make_decode_tables)(mpg123_handle*);
typedef real (*func_init_layer3_gainpow2)(mpg123_handle*, int);
typedef real* (*func_init_layer2_table)(mpg123_handle*, real*, double);
typedef int (*func_synth_pent)(real *,int,unsigned char *);

/* last headaches about getting mmx hardcode out */
real init_layer3_gainpow2(mpg123_handle *fr, int i);
real* init_layer2_table(mpg123_handle *fr, real *table, double m);
void make_decode_tables(mpg123_handle *fr);
void prepare_decode_tables(void); /* perhaps not best place here */

/* only 3dnow replaces that one, it's internal to layer3.c otherwise */
void dct36(real *,real *,real *,real *,real *);
#define opt_dct36(fr) dct36
/* only mmx replaces those */
#define opt_make_decode_tables(fr) make_decode_tables(fr)
#define opt_decwin(fr) (fr)->decwin
#define opt_init_layer3_gainpow2(fr) init_layer3_gainpow2
#define opt_init_layer2_table(fr) init_layer2_table

#ifdef OPT_GENERIC
	#define PENTIUM_FALLBACK
	void dct64(real *,real *,real *);
	int synth_1to1(real *bandPtr,int channel, mpg123_handle *fr, int final);
	int synth_1to1_8bit(real *bandPtr,int channel, mpg123_handle *fr, int final);
	int synth_1to1_mono(real *, mpg123_handle *fr);
	int synth_1to1_mono2stereo (real *, mpg123_handle *fr);
	int synth_1to1_8bit_mono (real *, mpg123_handle *fr);
	int synth_1to1_8bit_mono2stereo (real *, mpg123_handle *fr);
	#ifndef OPT_MULTI
	#define defopt generic
	#define opt_dct64(fr) dct64
	#define opt_synth_1to1(fr) synth_1to1
	#define opt_synth_1to1_mono(fr) synth_1to1_mono
	#define opt_synth_1to1_mono2stereo(fr) synth_1to1_mono2stereo
	#define opt_synth_1to1_8bit(fr) synth_1to1_8bit
	#define opt_synth_1to1_8bit_mono(fr) synth_1to1_8bit_mono
	#define opt_synth_1to1_8bit_mono2stereo(fr) synth_1to1_8bit_mono2stereo
	#endif
#endif

/* i486 is special... always alone! */
#ifdef OPT_I486
#define OPT_X86
#define OPT_I386_SYNTH
#define defopt ivier
	int synth_1to1_486(real *bandPtr, int channel, mpg123_handle *fr, int nb_blocks);
#ifdef OPT_MULTI
#error "i486 can only work alone!"
#endif
#define opt_synth_1to1(fr) synth_1to1_i386
#define FIR_BUFFER_SIZE  128
#define FIR_SIZE 16
	void dct64_i486(int *a,int *b,real *c); /* not used generally */
#endif

#ifdef OPT_I386
	#define PENTIUM_FALLBACK
	#define OPT_X86
	#define OPT_I386_SYNTH
	#ifndef OPT_MULTI
#ifndef defopt
	#define defopt idrei
#endif
	#define opt_synth_1to1(fr) synth_1to1_i386
	#endif
#endif

#ifdef OPT_I386_SYNTH
	int synth_1to1_i386(real *bandPtr, int channel, mpg123_handle *fr, int final);
#endif

#ifdef OPT_I586
	#define PENTIUM_FALLBACK
	#define OPT_PENTIUM
	#define OPT_X86
	int synth_1to1_i586(real *bandPtr, int channel, mpg123_handle *fr, int final);
	int synth_1to1_i586_asm(real *bandPtr, int channel, unsigned char *out, unsigned char *buffs, int *bo, real *decwin);
	#ifndef OPT_MULTI
	#define defopt ifuenf
	#define opt_synth_1to1(fr) synth_1to1_i586
	#define opt_synth_1to1_i586_asm(fr) synth_1to1_i586_asm
	#endif
#endif

#ifdef OPT_I586_DITHER
	#define PENTIUM_FALLBACK
	#define OPT_PENTIUM
	#define OPT_X86
	int synth_1to1_i586(real *bandPtr, int channel, mpg123_handle *fr, int final);
	int synth_1to1_i586_asm_dither(real *bandPtr, int channel, unsigned char *out, unsigned char *buffs, int *bo, real *decwin);
	#ifndef OPT_MULTI
	#define defopt ifuenf_dither
	#define opt_synth_1to1(fr) synth_1to1_i586
	#define opt_synth_1to1_i586_asm(fr) synth_1to1_i586_asm_dither
	#endif
#endif

/* That one has by far the most ugly hacks to make it cooperative. */
#ifdef OPT_MMX
	#define OPT_MMXORSSE
	#define OPT_X86
	real init_layer3_gainpow2_mmx(mpg123_handle *fr, int i);
	real* init_layer2_table_mmx(mpg123_handle *fr, real *table, double m);
	/* I think one can optimize storage here with the normal decwin */
	extern real decwin_mmx[512+32];
	void dct64_mmx(real *,real *,real *);
	int synth_1to1_mmx(real *bandPtr, int channel, mpg123_handle *fr, int final);
	void make_decode_tables_mmx(mpg123_handle *fr); /* tabinit_mmx.s */
	void make_decode_tables_mmx_asm(long scaleval, float* decwin_mmx, float *decwins); /* tabinit_mmx.s */
	/* these are in asm, dct64 called directly there */
	void dct64_MMX(short *a,short *b,real *c);
	int synth_1to1_MMX(real *bandPtr, int channel, short *out, short *buffs, int *bo, float *decwins);
	#ifndef OPT_MULTI
	#define defopt mmx
/*	#undef opt_decwin
	#define opt_decwin(fr) decwin_mmx */
	#define opt_dct64(fr) dct64_mmx
	#define opt_synth_1to1(fr) synth_1to1_mmx
	#define opt_
	#undef opt_make_decode_tables
	#define opt_make_decode_tables(fr) make_decode_tables_mmx(fr)
	#undef opt_init_layer3_gainpow2
	#define opt_init_layer3_gainpow2(fr) init_layer3_gainpow2_mmx
	#undef opt_init_layer2_table
	#define opt_init_layer2_table(fr) init_layer2_table_mmx
	#define OPT_MMX_ONLY
	#endif
#endif

/* first crude hack into our source */
#ifdef OPT_SSE
	#define OPT_MMXORSSE
	#define OPT_MPLAYER
	#define OPT_X86
	real init_layer3_gainpow2_mmx(mpg123_handle *fr, int i);
	real* init_layer2_table_mmx(mpg123_handle *fr, real *table, double m);
	/* I think one can optimize storage here with the normal decwin */
	extern real decwin_mmx[512+32];
	void dct64_mmx(real *,real *,real *);
	void dct64_sse(real *,real *,real *);
	int synth_1to1_sse(real *bandPtr, int channel, mpg123_handle *fr, int final);
	void synth_1to1_sse_asm(real *bandPtr, int channel, short *samples, short *buffs, int *bo, real *decwin);
	void make_decode_tables_mmx(mpg123_handle *fr); /* tabinit_mmx.s */
	void make_decode_tables_mmx_asm(long scaleval, float* decwin_mmx, float *decwins); /* tabinit_mmx.s */
	/* ugly! */
	extern func_dct64 mpl_dct64;
	#ifndef OPT_MULTI
	#define defopt sse
	#define opt_mpl_dct64(fr) dct64_sse
/*	#undef opt_decwin
	#define opt_decwin(fr) decwin_mmx */
	#define opt_dct64(fr) dct64_mmx /* dct64_sse is silent in downsampling modes */
	#define opt_synth_1to1(fr) synth_1to1_sse /* that will use dct64_sse */
	#undef opt_make_decode_tables
	#define opt_make_decode_tables(fr) make_decode_tables_mmx(fr)
	#undef opt_init_layer3_gainpow2
	#define opt_init_layer3_gainpow2(fr) init_layer3_gainpow2_mmx
	#undef opt_init_layer2_table
	#define opt_init_layer2_table(fr) init_layer2_table_mmx
	#define OPT_MMX_ONLY /* watch out! */
	#endif
#endif

/* first crude hack into our source */
#ifdef OPT_3DNOWEXT
	#define OPT_MMXORSSE
	#define OPT_MPLAYER
	#define OPT_X86
	real init_layer3_gainpow2_mmx(mpg123_handle *fr, int i);
	real* init_layer2_table_mmx(mpg123_handle *fr, real *table, double m);
	/* I think one can optimize storage here with the normal decwin */
	extern real decwin_mmx[512+32];
	void dct64_mmx(real *,real *,real *);
	void dct64_3dnowext(real *,real *,real *);
	void dct36_3dnowext(real *,real *,real *,real *,real *);
	int synth_1to1_3dnowext(real *bandPtr, int channel, mpg123_handle *fr, int final);
	void synth_1to1_3dnowext_asm(real *bandPtr, int channel, short *samples, short *buffs, int *bo, real *decwin);
	void make_decode_tables_mmx(mpg123_handle *fr); /* tabinit_mmx.s */
	void make_decode_tables_mmx_asm(long scaleval, float* decwin_mmx, float *decwins); /* tabinit_mmx.s */
	/* ugly! */
	extern func_dct64 mpl_dct64;
	#ifndef OPT_MULTI
	#define defopt dreidnowext
	#define opt_mpl_dct64(fr) dct64_3dnowext
	#undef opt_dct36
	#define opt_dct36(fr) dct36_3dnowext
/*	#undef opt_decwin
	#define opt_decwin(fr) decwin_mmx */
	#define opt_dct64(fr) dct64_mmx /* dct64_sse is silent in downsampling modes */
	#define opt_synth_1to1(fr) synth_1to1_3dnowext /* that will use dct64_3dnowext */
	#undef opt_make_decode_tables
	#define opt_make_decode_tables(fr) make_decode_tables_mmx(fr)
	#undef opt_init_layer3_gainpow2
	#define opt_init_layer3_gainpow2(fr) init_layer3_gainpow2_mmx
	#undef opt_init_layer2_table
	#define opt_init_layer2_table(fr) init_layer2_table_mmx
	#define OPT_MMX_ONLY /* watch out! */
	#endif
#endif


#ifndef OPT_MMX_ONLY
extern real *pnts[5];
extern real decwin[512+32];
#endif
#ifdef OPT_MPLAYER
extern const int costab_mmxsse[];
#endif

/* 3dnow used to use synth_1to1_i586 for mono / 8bit conversion - was that intentional? */
/* I'm trying to skip the pentium code here ... until I see that that is indeed a bad idea */
#ifdef OPT_3DNOW
	#define K6_FALLBACK /* a fallback for 3DNowExt */
	#define OPT_X86
	void dct36_3dnow(real *,real *,real *,real *,real *);
	void do_equalizer_3dnow(real *bandPtr,int channel, real equalizer[2][32]);
	int synth_1to1_3dnow(real *bandPtr, int channel, mpg123_handle *fr, int final);
	int synth_1to1_3dnow_asm(real *bandPtr, int channel, unsigned char *out, unsigned char *buffs, int *bo, real *decwin);
	#ifndef OPT_MULTI
	#define defopt dreidnow
	#undef opt_dct36
	#define opt_dct36(fr) dct36_3dnow
	#define opt_synth_1to1(fr) synth_1to1_3dnow
	#endif
#endif

#ifdef OPT_X86
	/* these have to be merged back into one! */
	unsigned int getcpuid();
	unsigned int getextcpuflags();
	unsigned int getstdcpuflags();
	unsigned int getstd2cpuflags();

	void dct64_i386(real *,real *,real *);
	int synth_1to1_mono_i386(real *, mpg123_handle *fr);
	int synth_1to1_mono2stereo_i386(real *, mpg123_handle *fr);
	int synth_1to1_8bit_i386(real *,int, mpg123_handle *fr, int final);
	int synth_1to1_8bit_mono_i386(real *, mpg123_handle *fr);
	int synth_1to1_8bit_mono2stereo_i386(real *, mpg123_handle *fr);
	#ifndef OPT_MULTI
	#ifndef opt_dct64
	#define opt_dct64(fr) dct64_i386 /* default one even for 3dnow and i486 in decode_2to1, decode_ntom */
	#endif
	#define opt_synth_1to1_mono(fr) synth_1to1_mono_i386
	#define opt_synth_1to1_mono2stereo(fr) synth_1to1_mono2stereo_i386
	#define opt_synth_1to1_8bit(fr) synth_1to1_8bit_i386
	#define opt_synth_1to1_8bit_mono(fr) synth_1to1_8bit_mono_i386
	#define opt_synth_1to1_8bit_mono2stereo(fr) synth_1to1_8bit_mono2stereo_i386
	#endif
#endif

#ifdef OPT_ALTIVEC
	void dct64_altivec(real *out0,real *out1,real *samples);
	int synth_1to1_altivec(real *,int,mpg123_handle *, int);
	int synth_1to1_mono_altivec(real *,mpg123_handle *);
	int synth_1to1_mono2stereo_altivec(real *, mpg123_handle *);
	int synth_1to1_8bit_altivec(real *,int,mpg123_handle *,int);
	int synth_1to1_8bit_mono_altivec(real *,mpg123_handle *);
	int synth_1to1_8bit_mono2stereo_altivec(real *,mpg123_handle *);
	#ifndef OPT_MULTI
	#define defopt altivec
	#define opt_dct64(fr) dct64_altivec
	#define opt_synth_1to1(fr) synth_1to1_altivec
	#define opt_synth_1to1_mono(fr) synth_1to1_mono_altivec
	#define opt_synth_1to1_mono2stereo(fr) synth_1to1_mono2stereo_altivec
	#define opt_synth_1to1_8bit(fr) synth_1to1_8bit_altivec
	#define opt_synth_1to1_8bit_mono(fr) synth_1to1_8bit_mono_altivec
	#define opt_synth_1to1_8bit_mono2stereo(fr) synth_1to1_8bit_mono2stereo_altivec
	#endif
#endif
		
/* used for multi opt mode and the single 3dnow mode to have the old 3dnow test flag still working */
void check_decoders(void);

#ifdef OPT_MULTI
	#ifdef OPT_X86
	extern struct cpuflags cf;
	#endif
	#define defopt nodec
	/* a simple global struct to hold the decoding function pointers, could be localized later if really wanted */

	#define opt_synth_1to1(fr) ((fr)->cpu_opts.synth_1to1)
	#define opt_synth_1to1_mono(fr) ((fr)->cpu_opts.synth_1to1_mono)
	#define opt_synth_1to1_mono2stereo(fr) ((fr)->cpu_opts.synth_1to1_mono2stereo)
	#define opt_synth_1to1_8bit(fr) ((fr)->cpu_opts.synth_1to1_8bit)
	#define opt_synth_1to1_8bit_mono(fr) ((fr)->cpu_opts.synth_1to1_8bit_mono)
	#define opt_synth_1to1_8bit_mono2stereo(fr) ((fr)->cpu_opts.synth_1to1_8bit_mono2stereo)
	#ifdef OPT_PENTIUM
	#define opt_synth_1to1_i586_asm(fr) ((fr)->cpu_opts.synth_1to1_i586_asm)
	#endif
	#ifdef OPT_MMXORSSE
	#undef opt_make_decode_tables
	#define opt_make_decode_tables(fr) ((fr)->cpu_opts.make_decode_tables)(fr)
/*	#undef opt_decwin
	#define opt_decwin(fr) (fr)->cpu_opts.decwin */
	#undef opt_init_layer3_gainpow2
	#define opt_init_layer3_gainpow2(fr) ((fr)->cpu_opts.init_layer3_gainpow2)
	#undef opt_init_layer2_table
	#define opt_init_layer2_table(fr) ((fr)->cpu_opts.init_layer2_table)
	#endif
	#ifdef OPT_3DNOW
	#undef opt_dct36
	#define opt_dct36(fr) ((fr)->cpu_opts.dct36)
	#endif
	#define opt_dct64(fr) ((fr)->cpu_opts.dct64)
	#ifdef OPT_MPLAYER
	#define opt_mpl_dct64(fr) ((fr)->cpu_opts.mpl_dct64)
	#endif
#endif

#endif /* MPG123_H_OPTIMIZE */

