/*
	optimize: get a grip on the different optimizations

	copyright 2006 by the mpg123 project - free software under the terms of the LGPL 2.1
	see COPYING and AUTHORS files in distribution or http://mpg123.org
	initially written by Thomas Orgis, inspired by 3DNow stuff in mpg123.[hc]

	Currently, this file contains the struct and function to choose an optimization variant and works only when OPT_MULTI is in effect.
*/

#include "mpg123lib_intern.h" /* includes optimize.h */
#ifdef OPT_MULTI

#include "getcpuflags.h"
struct cpuflags cpu_flags;

/* same number of entries as full list, but empty at beginning */
static char *mpg123_supported_decoder_list[] =
{
	#ifdef OPT_3DNOWEXT
	NULL,
	#endif
	#ifdef OPT_SSE
	NULL,
	#endif
	#ifdef OPT_3DNOW
	NULL,
	#endif
	#ifdef OPT_MMX
	NULL,
	#endif
	#ifdef OPT_I586
	NULL,
	#endif
	#ifdef OPT_I586_DITHER
	NULL,
	#endif
	#ifdef OPT_I486
	NULL,
	#endif
	#ifdef OPT_I386
	NULL,
	#endif
	#ifdef OPT_ALTIVEC
	NULL,
	#endif
	NULL, /* generic */
	NULL
};
#endif

static char *mpg123_decoder_list[] =
{
	#ifdef OPT_3DNOWEXT
	"3DNowExt",
	#endif
	#ifdef OPT_SSE
	"SSE",
	#endif
	#ifdef OPT_3DNOW
	"3DNow",
	#endif
	#ifdef OPT_MMX
	"MMX",
	#endif
	#ifdef OPT_I586
	"i586",
	#endif
	#ifdef OPT_I586_DITHER
	"i586_dither",
	#endif
	#ifdef OPT_I486
	"i486",
	#endif
	#ifdef OPT_I386
	"i386",
	#endif
	#ifdef OPT_ALTIVEC
	"AltiVec",
	#endif
	#ifdef OPT_GENERIC
	"generic",
	#endif
	NULL
};

void check_decoders(void )
{
#ifndef OPT_MULTI
	return;
#else
	char **d = mpg123_supported_decoder_list;
#ifdef OPT_X86
	getcpuflags(&cpu_flags);
	if(cpu_i586(cpu_flags))
	{
		/* not yet: if(cpu_sse2(cpu_flags)) printf(" SSE2");
		if(cpu_sse3(cpu_flags)) printf(" SSE3"); */
#ifdef OPT_3DNOWEXT
		if(cpu_3dnowext(cpu_flags)) *(d++) = "3DNowExt";
#endif
#ifdef OPT_SSE
		if(cpu_sse(cpu_flags)) *(d++) = "SSE";
#endif
#ifdef OPT_3DNOW
		if(cpu_3dnow(cpu_flags)) *(d++) = "3DNow";
#endif
#ifdef OPT_MMX
		if(cpu_mmx(cpu_flags)) *(d++) = "MMX";
#endif
#ifdef OPT_I586
		*(d++) = "i586";
#endif
#ifdef OPT_I586_DITHER
		*(d++) = "i586_dither";
#endif
	}
#endif
/* just assume that the i486 built is run on a i486 cpu... */
#ifdef OPT_I486
	*(d++) = "i486";
#endif
#ifdef OPT_ALTIVEC
	*(d++) = "AltiVec";
#endif
/* every supported x86 can do i386, any cpu can do generic */
#ifdef OPT_I386
	*(d++) = "i386";
#endif
#ifdef OPT_GENERIC
	*(d++) = "generic";
#endif
#endif /* ndef OPT_MULTI */
}

char attribute_align_arg **mpg123_decoders(){ return mpg123_decoder_list; }
char attribute_align_arg **mpg123_supported_decoders()
{
#ifdef OPT_MULTI
	return mpg123_supported_decoder_list;
#else
	return mpg123_decoder_list;
#endif
}
