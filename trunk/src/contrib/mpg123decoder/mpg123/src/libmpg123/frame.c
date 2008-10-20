/*
	frame: Heap of routines dealing with the core mpg123 data structure.

	copyright 2008 by the mpg123 project - free software under the terms of the LGPL 2.1
	see COPYING and AUTHORS files in distribution or http://mpg123.org
	initially written by Thomas Orgis
*/

#include "mpg123lib_intern.h"
#include "getcpuflags.h"
#include "debug.h"

#define IGNORESHIFT 2

static void frame_fixed_reset(mpg123_handle *fr);

/* that's doubled in decode_ntom.c */
#define NTOM_MUL (32768)
#define aligned_pointer(p,type,alignment) \
	(((char*)(p)-(char*)NULL) % (alignment)) \
	? (type*)((char*)(p) + (alignment) - (((char*)(p)-(char*)NULL) % (alignment))) \
	: (type*)(p)
void frame_default_pars(mpg123_pars *mp)
{
	mp->outscale = MAXOUTBURST;
#ifdef GAPLESS
	mp->flags = MPG123_GAPLESS;
#else
	mp->flags = 0;
#endif
	mp->force_rate = 0;
	mp->down_sample = 0;
	mp->rva = 0;
	mp->halfspeed = 0;
	mp->doublespeed = 0;
	mp->verbose = 0;
	mp->icy_interval = 0;
#ifndef WIN32
	mp->timeout = 0;
#endif
	mp->resync_limit = 1024;
	mpg123_fmt_all(mp);
}

void frame_init(mpg123_handle *fr)
{
	frame_init_par(fr, NULL);
}

void frame_init_par(mpg123_handle *fr, mpg123_pars *mp)
{
	fr->own_buffer = FALSE;
	fr->buffer.data = NULL;
	fr->rawbuffs = NULL;
	fr->rawdecwin = NULL;
	fr->conv16to8_buf = NULL;
	fr->cpu_opts.type = defopt;
	fr->cpu_opts.class = (defopt == mmx || defopt == sse || defopt == dreidnowext) ? mmxsse : normal;
	/* these two look unnecessary, check guarantee for synth_ntom_set_step (in control_generic, even)! */
	fr->ntom_val[0] = NTOM_MUL>>1;
	fr->ntom_val[1] = NTOM_MUL>>1;
	fr->ntom_step = NTOM_MUL;
	/* unnecessary: fr->buffer.size = fr->buffer.fill = 0; */
	mpg123_reset_eq(fr);
	init_icy(&fr->icy);
	init_id3(fr);
	/* frame_outbuffer is missing... */
	/* frame_buffers is missing... that one needs cpu opt setting! */
	/* after these... frame_reset is needed before starting full decode */
	fr->af.encoding = 0;
	fr->af.rate = 0;
	fr->af.channels = 0;
	fr->rdat.r_read = NULL;
	fr->rdat.r_lseek = NULL;
	fr->decoder_change = 1;
	fr->err = MPG123_OK;
	if(mp == NULL) frame_default_pars(&fr->p);
	else memcpy(&fr->p, mp, sizeof(struct mpg123_pars_struct));
	frame_fixed_reset(fr); /* Reset only the fixed data, dynamic buffers are not there yet! */
}

mpg123_pars attribute_align_arg *mpg123_new_pars(int *error)
{
	mpg123_pars *mp = malloc(sizeof(struct mpg123_pars_struct));
	if(mp != NULL){ frame_default_pars(mp); if(error != NULL) *error = MPG123_OK; }
	else if(error != NULL) *error = MPG123_OUT_OF_MEM;
	return mp;
}

void attribute_align_arg mpg123_delete_pars(mpg123_pars* mp)
{
	if(mp != NULL) free(mp);
}

int attribute_align_arg mpg123_reset_eq(mpg123_handle *mh)
{
	int i;
	mh->have_eq_settings = 0;
	for(i=0; i < 32; ++i) mh->equalizer[0][i] = mh->equalizer[1][i] = DOUBLE_TO_REAL(1.0);

	return MPG123_OK;
}

int frame_outbuffer(mpg123_handle *fr)
{
	size_t size = mpg123_safe_buffer()*AUDIOBUFSIZE;
	if(!fr->own_buffer) fr->buffer.data = NULL;
	if(fr->buffer.data != NULL && fr->buffer.size != size)
	{
		free(fr->buffer.data);
		fr->buffer.data = NULL;
	}
	fr->buffer.size = size;
	if(fr->buffer.data == NULL) fr->buffer.data = (unsigned char*) malloc(fr->buffer.size);
	if(fr->buffer.data == NULL)
	{
		fr->err = MPG123_OUT_OF_MEM;
		return -1;
	}
	fr->own_buffer = TRUE;
	fr->buffer.fill = 0;
	return 0;
}

int attribute_align_arg mpg123_replace_buffer(mpg123_handle *mh, unsigned char *data, size_t size)
{
	if(data == NULL || size < mpg123_safe_buffer())
	{
		mh->err = MPG123_BAD_BUFFER;
		return MPG123_ERR;
	}
	if(mh->own_buffer && mh->buffer.data != NULL) free(mh->buffer.data);
	mh->own_buffer = FALSE;
	mh->buffer.data = data;
	mh->buffer.size = size;
	mh->buffer.fill = 0;
	return MPG123_OK;
}

int frame_buffers(mpg123_handle *fr)
{
	int buffssize = 0;
	debug1("frame %p buffer", (void*)fr);
/*
	the used-to-be-static buffer of the synth functions, has some subtly different types/sizes

	2to1, 4to1, ntom, generic, i386: real[2][2][0x110]
	mmx, sse: short[2][2][0x110]
	i586(_dither): 4352 bytes; int/long[2][2][0x110]
	i486: int[2][2][17*FIR_BUFFER_SIZE]
	altivec: static real __attribute__ ((aligned (16))) buffs[4][4][0x110]

	Huh, altivec looks like fun. Well, let it be large... then, the 16 byte alignment seems to be implicit on MacOSX malloc anyway.
	Let's make a reasonable attempt to allocate enough memory...
	Keep in mind: biggest ones are i486 and altivec (mutually exclusive!), then follows i586 and normal real.
	mmx/sse use short but also real for resampling.
	Thus, minimum is 2*2*0x110*sizeof(real).
*/
	if(fr->cpu_opts.type == altivec) buffssize = 4*4*0x110*sizeof(real);
#ifdef OPT_I486
	else if(fr->cpu_opts.type == ivier) buffssize = 2*2*17*FIR_BUFFER_SIZE*sizeof(int);
#endif
	else if(fr->cpu_opts.type == ifuenf || fr->cpu_opts.type == ifuenf_dither || fr->cpu_opts.type == dreidnow)
	buffssize = 2*2*0x110*4; /* don't rely on type real, we need 4352 bytes */

	if(2*2*0x110*sizeof(real) > buffssize)
	buffssize = 2*2*0x110*sizeof(real);
	buffssize += 15; /* For 16-byte alignment (SSE likes that). */

	if(fr->rawbuffs != NULL && fr->rawbuffss != buffssize)
	{
		free(fr->rawbuffs);
		fr->rawbuffs = NULL;
	}

	if(fr->rawbuffs == NULL) fr->rawbuffs = (unsigned char*) malloc(buffssize);
	if(fr->rawbuffs == NULL) return -1;
	fr->rawbuffss = buffssize;
	fr->short_buffs[0][0] = aligned_pointer(fr->rawbuffs,short,16);
	fr->short_buffs[0][1] = fr->short_buffs[0][0] + 0x110;
	fr->short_buffs[1][0] = fr->short_buffs[0][1] + 0x110;
	fr->short_buffs[1][1] = fr->short_buffs[1][0] + 0x110;
	fr->real_buffs[0][0] = aligned_pointer(fr->rawbuffs,real,16);
	fr->real_buffs[0][1] = fr->real_buffs[0][0] + 0x110;
	fr->real_buffs[1][0] = fr->real_buffs[0][1] + 0x110;
	fr->real_buffs[1][1] = fr->real_buffs[1][0] + 0x110;
#ifdef OPT_I486
	if(fr->cpu_opts.type == ivier)
	{
		fr->int_buffs[0][0] = (int*) fr->rawbuffs;
		fr->int_buffs[0][1] = fr->int_buffs[0][0] + 17*FIR_BUFFER_SIZE;
		fr->int_buffs[1][0] = fr->int_buffs[0][1] + 17*FIR_BUFFER_SIZE;
		fr->int_buffs[1][1] = fr->int_buffs[1][0] + 17*FIR_BUFFER_SIZE;
	}
#endif
#ifdef OPT_ALTIVEC
	if(fr->cpu_opts.type == altivec)
	{
		int i,j;
		fr->areal_buffs[0][0] = (real*) fr->rawbuffs;
		for(i=0; i<4; ++i) for(j=0; j<4; ++j)
		fr->areal_buffs[i][j] = fr->areal_buffs[0][0] + (i*4+j)*0x110;
	}
#endif
	/* now the different decwins... all of the same size, actually */
	/* The MMX ones want 32byte alignment, which I'll try to ensure manually */
	{
		int decwin_size = (512+32)*sizeof(real);
		if(fr->rawdecwin != NULL) free(fr->rawdecwin);
#ifdef OPT_MMXORSSE
#ifdef OPT_MULTI
		if(fr->cpu_opts.class == mmxsse)
		{
#endif
			/* decwin_mmx will share, decwins will be appended ... sizeof(float)==4 */
			if(decwin_size < (512+32)*4) decwin_size = (512+32)*4;
			decwin_size += (512+32)*4 + 31; /* the second window + alignment zone */
			/* (512+32)*4/32 == 2176/32 == 68, so one decwin block retains alignment */
#ifdef OPT_MULTI
		}
#endif
#endif
		fr->rawdecwin = (unsigned char*) malloc(decwin_size);
		if(fr->rawdecwin == NULL) return -1;
		fr->decwin = (real*) fr->rawdecwin;
#ifdef OPT_MMXORSSE
#ifdef OPT_MULTI
		if(fr->cpu_opts.class == mmxsse)
		{
#endif
			/* align decwin, assign that to decwin_mmx, append decwins */
			/* I need to add to decwin what is missing to the next full 32 byte -- also I want to make gcc -pedantic happy... */
			fr->decwin = aligned_pointer(fr->rawdecwin,real,32);
			debug1("aligned decwin: %p", (void*)fr->decwin);
			fr->decwin_mmx = (float*)fr->decwin;
			fr->decwins = fr->decwin_mmx+512+32;
#ifdef OPT_MULTI
		}
		else debug("no decwins/decwin_mmx for that class");
#endif
#endif
	}
	frame_buffers_reset(fr);
	debug1("frame %p buffer done", (void*)fr);
	return 0;
}

int frame_buffers_reset(mpg123_handle *fr)
{
	fr->buffer.fill = 0; /* hm, reset buffer fill... did we do a flush? */
	fr->bsnum = 0;
	/* Wondering: could it be actually _wanted_ to retain buffer contents over different files? (special gapless / cut stuff) */
	fr->bsbuf = fr->bsspace[1];
	fr->bsbufold = fr->bsbuf;
	fr->bitreservoir = 0; /* Not entirely sure if this is the right place for that counter. */
	memset(fr->bsspace, 0, 2*(MAXFRAMESIZE+512));
	memset(fr->ssave, 0, 34);
	memset(fr->rawbuffs, 0, fr->rawbuffss);
	fr->hybrid_blc[0] = fr->hybrid_blc[1] = 0;
	memset(fr->hybrid_block, 0, sizeof(real)*2*2*SBLIMIT*SSLIMIT);
	/* Not totally, but quite, sure that decwin(s) doesn't need cleaning. */
	return 0;
}

void frame_icy_reset(mpg123_handle* fr)
{
	if(fr->icy.data != NULL) free(fr->icy.data);
	fr->icy.data = NULL;
	fr->icy.interval = 0;
	fr->icy.next = 0;
}

/* Prepare the handle for a new track.
   Reset variables, buffers... */
int frame_reset(mpg123_handle* fr)
{
	frame_buffers_reset(fr);
	frame_fixed_reset(fr);
	return 0;
}

/* Reset everythign except dynamic memory. */
static void frame_fixed_reset(mpg123_handle *fr)
{
	frame_icy_reset(fr);
	open_bad(fr);
	fr->to_decode = FALSE;
	fr->to_ignore = FALSE;
	fr->metaflags = 0;
	fr->outblock = mpg123_safe_buffer();
	fr->num = -1;
	fr->clip = 0;
	fr->oldhead = 0;
	fr->firsthead = 0;
	fr->vbr = MPG123_CBR;
	fr->abr_rate = 0;
	fr->track_frames = 0;
	fr->track_samples = -1;
	fr->framesize=0; 
	fr->mean_frames = 0;
	fr->mean_framesize = 0;
	fr->freesize = 0;
	fr->lastscale = -1;
	fr->rva.level[0] = -1;
	fr->rva.level[1] = -1;
	fr->rva.gain[0] = 0;
	fr->rva.gain[1] = 0;
	fr->rva.peak[0] = 0;
	fr->rva.peak[1] = 0;
#ifdef FRAME_INDEX
	fr->index.fill = 0;
	fr->index.step = 1;
#endif
	fr->fsizeold = 0;
	fr->firstframe = 0;
	fr->ignoreframe = fr->firstframe-IGNORESHIFT;
	fr->lastframe = -1;
	fr->fresh = 1;
	fr->new_format = 0;
#ifdef GAPLESS
	frame_gapless_init(fr,0,0);
	fr->lastoff = 0;
	fr->firstoff = 0;
#endif
	fr->bo[0] = 1; /* the usual bo */
	fr->bo[1] = 0; /* ditherindex */
#ifdef OPT_I486
	fr->bo[0] = fr->bo[1] = FIR_SIZE-1;
#endif
	reset_id3(fr);
	reset_icy(&fr->icy);
	/* ICY stuff should go into icy.c, eh? */
	fr->icy.interval = 0;
	fr->icy.next = 0;
	fr->halfphase = 0; /* here or indeed only on first-time init? */
}

void frame_free_buffers(mpg123_handle *fr)
{
	if(fr->rawbuffs != NULL) free(fr->rawbuffs);
	fr->rawbuffs = NULL;
	if(fr->rawdecwin != NULL) free(fr->rawdecwin);
	fr->rawdecwin = NULL;
	if(fr->conv16to8_buf != NULL) free(fr->conv16to8_buf);
	fr->conv16to8_buf = NULL;
}

void frame_exit(mpg123_handle *fr)
{
	if(fr->own_buffer && fr->buffer.data != NULL) free(fr->buffer.data);
	fr->buffer.data = NULL;
	frame_free_buffers(fr);
	exit_id3(fr);
	clear_icy(&fr->icy);
}

int attribute_align_arg mpg123_info(mpg123_handle *mh, struct mpg123_frameinfo *mi)
{
	if(mh == NULL) return MPG123_ERR;
	if(mi == NULL)
	{
		mh->err = MPG123_ERR_NULL;
		return MPG123_ERR;
	}
	mi->version = mh->mpeg25 ? MPG123_2_5 : (mh->lsf ? MPG123_2_0 : MPG123_1_0);
	mi->layer = mh->lay;
	mi->rate = frame_freq(mh);
	switch(mh->mode)
	{
		case 0: mi->mode = MPG123_M_STEREO; break;
		case 1: mi->mode = MPG123_M_JOINT;  break;
		case 2: mi->mode = MPG123_M_DUAL;   break;
		case 3: mi->mode = MPG123_M_MONO;   break;
		default: error("That mode cannot be!");
	}
	mi->mode_ext = mh->mode_ext;
	mi->framesize = mh->framesize+4; /* Include header. */
	mi->flags = 0;
	if(mh->error_protection) mi->flags |= MPG123_CRC;
	if(mh->copyright)        mi->flags |= MPG123_COPYRIGHT;
	if(mh->extension)        mi->flags |= MPG123_PRIVATE;
	if(mh->original)         mi->flags |= MPG123_ORIGINAL;
	mi->emphasis = mh->emphasis;
	mi->bitrate  = frame_bitrate(mh);
	mi->abr_rate = mh->abr_rate;
	mi->vbr = mh->vbr;
	return MPG123_OK;
}

/*
	find the best frame in index just before the wanted one, seek to there
	then step to just before wanted one with read_frame
	do not care tabout the stuff that was in buffer but not played back
	everything that left the decoder is counted as played
	
	Decide if you want low latency reaction and accurate timing info or stable long-time playback with buffer!
*/

off_t frame_index_find(mpg123_handle *fr, off_t want_frame, off_t* get_frame)
{
	/* default is file start if no index position */
	off_t gopos = 0;
	*get_frame = 0;
#ifdef FRAME_INDEX
	if(fr->index.fill)
	{
		/* find in index */
		size_t fi;
		/* at index fi there is frame step*fi... */
		fi = want_frame/fr->index.step;
		if(fi >= fr->index.fill) fi = fr->index.fill - 1;
		*get_frame = fi*fr->index.step;
		gopos = fr->index.data[fi];
	}
	else
	{
#endif
		/* A bit hackish here... but we need to be fresh when looking for the first header again. */
		fr->firsthead = 0;
		fr->oldhead = 0;
#ifdef FRAME_INDEX
	}
#endif
	debug2("index: 0x%lx for frame %li", (unsigned long)gopos, (long) *get_frame);
	return gopos;
}

off_t frame_ins2outs(mpg123_handle *fr, off_t ins)
{	
	off_t outs = 0;
	switch(fr->down_sample)
	{
		case 0:
		case 1:
		case 2: outs = ins>>fr->down_sample; break;
		case 3: outs = ntom_ins2outs(fr, ins); break;
		default: error("Bad down_sample ... should not be possible!!");
	}
	return outs;
}

off_t frame_outs(mpg123_handle *fr, off_t num)
{
	off_t outs = 0;
	switch(fr->down_sample)
	{
		case 0:
		case 1:
		case 2: outs = (spf(fr)>>fr->down_sample)*num; break;
		case 3: outs = ntom_frmouts(fr, num); break;
		default: error("Bad down_sample ... should not be possible!!");
	}
	return outs;
}

off_t frame_offset(mpg123_handle *fr, off_t outs)
{
	off_t num = 0;
	switch(fr->down_sample)
	{
		case 0:
		case 1:
		case 2: num = outs/(spf(fr)>>fr->down_sample); break;
		case 3: num = ntom_frameoff(fr, outs); break;
		default: error("Bad down_sample ... should not be possible!!");
	}
	return num;
}

#ifdef GAPLESS
/* input in _input_ samples */
void frame_gapless_init(mpg123_handle *fr, off_t b, off_t e)
{
	fr->begin_s = b;
	fr->end_s = e;
	/* These will get proper values later, from above plus resampling info. */
	fr->begin_os = 0;
	fr->end_os = 0;
	debug2("frame_gapless_init: from %lu to %lu samples", (long unsigned)fr->begin_s, (long unsigned)fr->end_s);
}

void frame_gapless_realinit(mpg123_handle *fr)
{
	fr->begin_os = frame_ins2outs(fr, fr->begin_s);
	fr->end_os   = frame_ins2outs(fr, fr->end_s);
	debug2("frame_gapless_realinit: from %lu to %lu samples", (long unsigned)fr->begin_os, (long unsigned)fr->end_os);
}
#endif

/* The frame seek... This is not simply the seek to fe*spf(fr) samples in output because we think of _input_ frames here.
   Seek to frame offset 1 may be just seek to 200 samples offset in output since the beginning of first frame is delay/padding.
   Hm, is that right? OK for the padding stuff, but actually, should the decoder delay be better totally hidden or not?
   With gapless, even the whole frame position could be advanced further than requested (since Homey don't play dat). */
void frame_set_frameseek(mpg123_handle *fr, off_t fe)
{
	fr->firstframe = fe;
#ifdef GAPLESS
	if(fr->p.flags & MPG123_GAPLESS)
	{
		/* Take care of the beginning... */
		off_t beg_f = frame_offset(fr, fr->begin_os);
		if(fe <= beg_f)
		{
			fr->firstframe = beg_f;
			fr->firstoff   = fr->begin_os - frame_outs(fr, beg_f);
		}
		else fr->firstoff = 0;
		/* The end is set once for a track at least, on the frame_set_frameseek called in get_next_frame() */
		if(fr->end_os > 0)
		{
			fr->lastframe  = frame_offset(fr,fr->end_os);
			fr->lastoff    = fr->end_os - frame_outs(fr, fr->lastframe);
		} else fr->lastoff = 0;
	} else { fr->firstoff = fr->lastoff = 0; fr->lastframe = -1; }
#endif
	fr->ignoreframe = fr->lay == 3 ? fr->firstframe-IGNORESHIFT : fr->firstframe;
#ifdef GAPLESS
	debug5("frame_set_frameseek: begin at %li frames and %li samples, end at %li and %li; ignore from %li",
	       (long) fr->firstframe, (long) fr->firstoff,
	       (long) fr->lastframe,  (long) fr->lastoff, (long) fr->ignoreframe);
#else
	debug3("frame_set_frameseek: begin at %li frames, end at %li; ignore from %li",
	       (long) fr->firstframe, (long) fr->lastframe, (long) fr->ignoreframe);
#endif
}

/* Sample accurate seek prepare for decoder. */
/* This gets unadjusted output samples and takes resampling into account */
void frame_set_seek(mpg123_handle *fr, off_t sp)
{
	fr->firstframe = frame_offset(fr, sp);
	fr->ignoreframe = fr->lay == 3 ? fr->firstframe-IGNORESHIFT : fr->firstframe;
#ifdef GAPLESS /* The sample offset is used for non-gapless mode, too! */
	fr->firstoff = sp - frame_outs(fr, fr->firstframe);
	debug5("frame_set_seek: begin at %li frames and %li samples, end at %li and %li; ignore from %li",
	       (long) fr->firstframe, (long) fr->firstoff,
	       (long) fr->lastframe,  (long) fr->lastoff, (long) fr->ignoreframe);
#else
	debug3("frame_set_seek: begin at %li frames, end at %li; ignore from %li",
	       (long) fr->firstframe, (long) fr->lastframe, (long) fr->ignoreframe);
#endif
}

/* to vanish */
void frame_outformat(mpg123_handle *fr, int format, int channels, long rate)
{
	fr->af.encoding = format;
	fr->af.rate = rate;
	fr->af.channels = channels;
}

/* set synth functions for current frame, optimizations handled by opt_* macros */
int set_synth_functions(mpg123_handle *fr)
{
	int ds = fr->down_sample;
	int p8=0;
	static func_synth funcs[2][4] = { 
		{ NULL,
		  synth_2to1,
		  synth_4to1,
		  synth_ntom } ,
		{ NULL,
		  synth_2to1_8bit,
		  synth_4to1_8bit,
		  synth_ntom_8bit } 
	};
	static func_synth_mono funcs_mono[2][2][4] = {    
		{ { NULL ,
		    synth_2to1_mono2stereo ,
		    synth_4to1_mono2stereo ,
		    synth_ntom_mono2stereo } ,
		  { NULL ,
		    synth_2to1_8bit_mono2stereo ,
		    synth_4to1_8bit_mono2stereo ,
		    synth_ntom_8bit_mono2stereo } } ,
		{ { NULL ,
		    synth_2to1_mono ,
		    synth_4to1_mono ,
		    synth_ntom_mono } ,
		  { NULL ,
		    synth_2to1_8bit_mono ,
		    synth_4to1_8bit_mono ,
		    synth_ntom_8bit_mono } }
	};

	/* possibly non-constand entries filled here */
	funcs[0][0] = (func_synth) opt_synth_1to1(fr);
	funcs[1][0] = (func_synth) opt_synth_1to1_8bit(fr);
	funcs_mono[0][0][0] = (func_synth_mono) opt_synth_1to1_mono2stereo(fr);
	funcs_mono[0][1][0] = (func_synth_mono) opt_synth_1to1_8bit_mono2stereo(fr);
	funcs_mono[1][0][0] = (func_synth_mono) opt_synth_1to1_mono(fr);
	funcs_mono[1][1][0] = (func_synth_mono) opt_synth_1to1_8bit_mono(fr);

	if(fr->af.encoding & MPG123_ENC_8) p8 = 1;
	fr->synth = funcs[p8][ds];
	fr->synth_mono = funcs_mono[fr->af.channels==2 ? 0 : 1][p8][ds];

	if(p8)
	{
		if(make_conv16to8_table(fr) != 0)
		{
			/* it's a bit more work to get proper error propagation up */
			return -1;
		}
	}
	return 0;
}

int attribute_align_arg mpg123_volume_change(mpg123_handle *mh, double change)
{
	if(mh == NULL) return MPG123_ERR;
	return mpg123_volume(mh, change + (double) mh->p.outscale / MAXOUTBURST);
}

int attribute_align_arg mpg123_volume(mpg123_handle *mh, double vol)
{
	if(mh == NULL) return MPG123_ERR;
	if(vol >= 0) mh->p.outscale = (double) MAXOUTBURST * vol;
	do_rva(mh);
	return MPG123_OK;
}

static int get_rva(mpg123_handle *fr, double *peak, double *gain)
{
	double p = -1;
	double g = 0;
	int ret = 0;
	if(fr->p.rva)
	{
		int rt = 0;
		/* Should one assume a zero RVA as no RVA? */
		if(fr->p.rva == 2 && fr->rva.level[1] != -1) rt = 1;
		if(fr->rva.level[rt] != -1)
		{
			p = fr->rva.peak[rt];
			g = fr->rva.gain[rt];
			ret = 1; /* Success. */
		}
	}
	if(peak != NULL) *peak = p;
	if(gain != NULL) *gain = g;
	return ret;
}

/* adjust the volume, taking both fr->outscale and rva values into account */
void do_rva(mpg123_handle *fr)
{
	double peak = 0;
	double gain = 0;
	scale_t newscale;
	double rvafact = 1;
	if(get_rva(fr, &peak, &gain))
	{
		if(NOQUIET && fr->p.verbose > 1) fprintf(stderr, "Note: doing RVA with gain %f\n", gain);
		rvafact = pow(10,gain/20);
	}

	newscale = fr->p.outscale*rvafact;

	/* if peak is unknown (== 0) this check won't hurt */
	if((peak*newscale) > MAXOUTBURST)
	{
		newscale = (scale_t) ((double) MAXOUTBURST/peak);
#ifdef FLOATOUT
		warning2("limiting scale value to %f to prevent clipping with indicated peak factor of %f", newscale, peak);
#else
		warning2("limiting scale value to %li to prevent clipping with indicated peak factor of %f", newscale, peak);
#endif
	}
	/* first rva setting is forced with fr->lastscale < 0 */
	if(newscale != fr->lastscale)
	{
#ifdef FLOATOUT
		debug3("changing scale value from %f to %f (peak estimated to %f)", fr->lastscale != -1 ? fr->lastscale : fr->p.outscale, newscale, (double) (newscale*peak));
#else
		debug3("changing scale value from %li to %li (peak estimated to %li)", fr->lastscale != -1 ? fr->lastscale : fr->p.outscale, newscale, (long) (newscale*peak));
#endif
		fr->lastscale = newscale;
		opt_make_decode_tables(fr); /* the actual work */
	}
}

int attribute_align_arg mpg123_getvolume(mpg123_handle *mh, double *base, double *really, double *rva_db)
{
	if(mh == NULL) return MPG123_ERR;
	if(base)   *base   = (double)mh->p.outscale/MAXOUTBURST;
	if(really) *really = (double)mh->lastscale/MAXOUTBURST;
	get_rva(mh, NULL, rva_db);
	return MPG123_OK;
}

int  frame_cpu_opt(mpg123_handle *fr, const char* cpu)
{
	char* chosen = ""; /* the chosed decoder opt as string */
	int auto_choose = 0;
	int done = 0;
	if(   (cpu == NULL)
	   || (cpu[0] == 0)
	   || !strcasecmp(cpu, "auto") )
	auto_choose = 1;
#ifndef OPT_MULTI
	{
		char **sd = mpg123_decoders(); /* this contains _one_ decoder */
		if(!auto_choose && strcasecmp(cpu, sd[0])) done = 0;
		else
		{
			chosen = sd[0];
			done = 1;
		}
	}
#else
	fr->cpu_opts.type = nodec;
	/* covers any i386+ cpu; they actually differ only in the synth_1to1 function... */
	#ifdef OPT_X86

	#ifdef OPT_MMXORSSE
	fr->cpu_opts.make_decode_tables   = make_decode_tables;
	fr->cpu_opts.init_layer3_gainpow2 = init_layer3_gainpow2;
	fr->cpu_opts.init_layer2_table    = init_layer2_table;
	#endif
	#ifdef OPT_3DNOW
	fr->cpu_opts.dct36 = dct36;
	#endif
	#ifdef OPT_3DNOWEXT
	fr->cpu_opts.dct36 = dct36;
	#endif

	if(cpu_i586(cpu_flags))
	{
		debug2("standard flags: 0x%08x\textended flags: 0x%08x", cpu_flags.std, cpu_flags.ext);
		#ifdef OPT_3DNOWEXT
		if(   !done && (auto_choose || !strcasecmp(cpu, "3dnowext"))
		   && cpu_3dnow(cpu_flags)
		   && cpu_3dnowext(cpu_flags)
		   && cpu_mmx(cpu_flags) )
		{
			int go = 1;
			if(fr->p.force_rate)
			{
				#if defined(K6_FALLBACK) || defined(PENTIUM_FALLBACK)
				if(!auto_choose){ if(NOQUIET) error("I refuse to choose 3DNowExt as this will screw up with forced rate!"); }
				else if(VERBOSE) fprintf(stderr, "Note: Not choosing 3DNowExt because flexible rate not supported.\n");

				go = 0;
				#else
				if(NOQUIET) error("You will hear some awful sound because of flexible rate being chosen with 3DNowExt decoder!");
				#endif
			}
			if(go){ /* temporary hack for flexible rate bug, not going indent this - fix it instead! */
			chosen = "3DNowExt";
			fr->cpu_opts.type = dreidnowext;
			fr->cpu_opts.class = mmxsse;
			fr->cpu_opts.dct36 = dct36_3dnowext;
			fr->cpu_opts.synth_1to1 = synth_1to1_3dnowext;
			fr->cpu_opts.dct64 = dct64_mmx; /* only use the 3dnow version in the synth_1to1_sse */
			fr->cpu_opts.make_decode_tables   = make_decode_tables_mmx;
			fr->cpu_opts.init_layer3_gainpow2 = init_layer3_gainpow2_mmx;
			fr->cpu_opts.init_layer2_table    = init_layer2_table_mmx;
			fr->cpu_opts.mpl_dct64 = dct64_3dnowext;
			done = 1;
			}
		}
		#endif
		#ifdef OPT_SSE
		if(   !done && (auto_choose || !strcasecmp(cpu, "sse"))
		   && cpu_sse(cpu_flags) && cpu_mmx(cpu_flags) )
		{
			int go = 1;
			if(fr->p.force_rate)
			{
				#ifdef PENTIUM_FALLBACK
				if(!auto_choose){ if(NOQUIET) error("I refuse to choose SSE as this will screw up with forced rate!"); }
				else if(VERBOSE) fprintf(stderr, "Note: Not choosing SSE because flexible rate not supported.\n");

				go = 0;
				#else
				if(NOQUIET) error("You will hear some awful sound because of flexible rate being chosen with SSE decoder!");
				#endif
			}
			if(go){ /* temporary hack for flexible rate bug, not going indent this - fix it instead! */
			chosen = "SSE";
			fr->cpu_opts.type = sse;
			fr->cpu_opts.class = mmxsse;
			fr->cpu_opts.synth_1to1 = synth_1to1_sse;
			fr->cpu_opts.dct64 = dct64_mmx; /* only use the sse version in the synth_1to1_sse */
			fr->cpu_opts.make_decode_tables   = make_decode_tables_mmx;
			fr->cpu_opts.init_layer3_gainpow2 = init_layer3_gainpow2_mmx;
			fr->cpu_opts.init_layer2_table    = init_layer2_table_mmx;
			fr->cpu_opts.mpl_dct64 = dct64_sse;
			done = 1;
			}
		}
		#endif
		#ifdef OPT_3DNOW
		fr->cpu_opts.dct36 = dct36;
		/* TODO: make autodetection for _all_ x86 optimizations (maybe just for i586+ and keep separate 486 build?) */
		/* check cpuflags bit 31 (3DNow!) and 23 (MMX) */
		if(    !done && (auto_choose || !strcasecmp(cpu, "3dnow"))
		    && cpu_3dnow(cpu_flags) && cpu_mmx(cpu_flags) )
		{
			chosen = "3DNow";
			fr->cpu_opts.type = dreidnow;
			fr->cpu_opts.dct36 = dct36_3dnow; /* 3DNow! optimized dct36() */
			fr->cpu_opts.synth_1to1 = synth_1to1_3dnow;
			fr->cpu_opts.dct64 = dct64_i386; /* use the 3dnow one? */
			done = 1;
		}
		#endif
		#ifdef OPT_MMX
		if(   !done && (auto_choose || !strcasecmp(cpu, "mmx"))
		   && cpu_mmx(cpu_flags) )
		{
			int go = 1;
			if(fr->p.force_rate)
			{
				#ifdef PENTIUM_FALLBACK
				if(!auto_choose){ if(NOQUIET) error("I refuse to choose MMX as this will screw up with forced rate!"); }
				else if(VERBOSE) fprintf(stderr, "Note: Not choosing MMX because flexible rate not supported.\n");

				go = 0;
				#else
				error("You will hear some awful sound because of flexible rate being chosen with MMX decoder!");
				#endif
			}
			if(go){ /* temporary hack for flexible rate bug, not going indent this - fix it instead! */
			chosen = "MMX";
			fr->cpu_opts.type = mmx;
			fr->cpu_opts.class = mmxsse;
			fr->cpu_opts.synth_1to1 = synth_1to1_mmx;
			fr->cpu_opts.dct64 = dct64_mmx;
			fr->cpu_opts.make_decode_tables   = make_decode_tables_mmx;
			fr->cpu_opts.init_layer3_gainpow2 = init_layer3_gainpow2_mmx;
			fr->cpu_opts.init_layer2_table    = init_layer2_table_mmx;
			done = 1;
			}
		}
		#endif
		#ifdef OPT_I586
		if(!done && (auto_choose || !strcasecmp(cpu, "i586")))
		{
			chosen = "i586/pentium";
			fr->cpu_opts.type = ifuenf;
			fr->cpu_opts.synth_1to1 = synth_1to1_i586;
			fr->cpu_opts.synth_1to1_i586_asm = synth_1to1_i586_asm;
			fr->cpu_opts.dct64 = dct64_i386;
			done = 1;
		}
		#endif
		#ifdef OPT_I586_DITHER
		if(!done && (auto_choose || !strcasecmp(cpu, "i586_dither")))
		{
			chosen = "dithered i586/pentium";
			fr->cpu_opts.type = ifuenf_dither;
			fr->cpu_opts.synth_1to1 = synth_1to1_i586;
			fr->cpu_opts.dct64 = dct64_i386;
			fr->cpu_opts.synth_1to1_i586_asm = synth_1to1_i586_asm_dither;
			done = 1;
		}
		#endif
	}
	#ifdef OPT_I486 /* that won't cooperate nicely in multi opt mode - forcing i486 in layer3.c */
	if(!done && (auto_choose || !strcasecmp(cpu, "i486")))
	{
		chosen = "i486";
		fr->cpu_opts.type = ivier;
		fr->cpu_opts.synth_1to1 = synth_1to1_i386; /* i486 function is special */
		fr->cpu_opts.dct64 = dct64_i386;
		done = 1;
	}
	#endif
	#ifdef OPT_I386
	if(!done && (auto_choose || !strcasecmp(cpu, "i386")))
	{
		chosen = "i386";
		fr->cpu_opts.type = idrei;
		fr->cpu_opts.synth_1to1 = synth_1to1_i386;
		fr->cpu_opts.dct64 = dct64_i386;
		done = 1;
	}
	#endif

	if(done) /* set common x86 functions */
	{
		fr->cpu_opts.synth_1to1_mono = synth_1to1_mono_i386;
		fr->cpu_opts.synth_1to1_mono2stereo = synth_1to1_mono2stereo_i386;
		fr->cpu_opts.synth_1to1_8bit = synth_1to1_8bit_i386;
		fr->cpu_opts.synth_1to1_8bit_mono = synth_1to1_8bit_mono_i386;
		fr->cpu_opts.synth_1to1_8bit_mono2stereo = synth_1to1_8bit_mono2stereo_i386;
	}
	#endif /* OPT_X86 */

	#ifdef OPT_ALTIVEC
	if(!done && (auto_choose || !strcasecmp(cpu, "altivec")))
	{
		chosen = "AltiVec";
		fr->cpu_opts.type = altivec;
		fr->cpu_opts.dct64 = dct64_altivec;
		fr->cpu_opts.synth_1to1 = synth_1to1_altivec;
		fr->cpu_opts.synth_1to1_mono = synth_1to1_mono_altivec;
		fr->cpu_opts.synth_1to1_mono2stereo = synth_1to1_mono2stereo_altivec;
		fr->cpu_opts.synth_1to1_8bit = synth_1to1_8bit_altivec;
		fr->cpu_opts.synth_1to1_8bit_mono = synth_1to1_8bit_mono_altivec;
		fr->cpu_opts.synth_1to1_8bit_mono2stereo = synth_1to1_8bit_mono2stereo_altivec;
		done = 1;
	}
	#endif

	#ifdef OPT_GENERIC
	if(!done && (auto_choose || !strcasecmp(cpu, "generic")))
	{
		chosen = "generic";
		fr->cpu_opts.type = generic;
		fr->cpu_opts.dct64 = dct64;
		fr->cpu_opts.synth_1to1 = synth_1to1;
		fr->cpu_opts.synth_1to1_mono = synth_1to1_mono;
		fr->cpu_opts.synth_1to1_mono2stereo = synth_1to1_mono2stereo;
		fr->cpu_opts.synth_1to1_8bit = synth_1to1_8bit;
		fr->cpu_opts.synth_1to1_8bit_mono = synth_1to1_8bit_mono;
		fr->cpu_opts.synth_1to1_8bit_mono2stereo = synth_1to1_8bit_mono2stereo;
		done = 1;
	}
	#endif
#endif
	if(done)
	{
		if(VERBOSE) fprintf(stderr, "Decoder: %s\n", chosen);
		return 1;
	}
	else
	{
		if(NOQUIET) error("Could not set optimization!");
		return 0;
	}
}

enum optdec dectype(const char* decoder)
{
	if(decoder == NULL) return autodec;
	if(!strcasecmp(decoder, "3dnowext"))    return dreidnowext;
	if(!strcasecmp(decoder, "3dnow"))       return dreidnow;
	if(!strcasecmp(decoder, "sse"))         return sse;
	if(!strcasecmp(decoder, "mmx"))         return mmx;
	if(!strcasecmp(decoder, "generic"))     return generic;
	if(!strcasecmp(decoder, "altivec"))     return altivec;
	if(!strcasecmp(decoder, "i386"))        return idrei;
	if(!strcasecmp(decoder, "i486"))        return ivier;
	if(!strcasecmp(decoder, "i586"))        return ifuenf;
	if(!strcasecmp(decoder, "i586_dither")) return ifuenf_dither;
	return nodec;
}

