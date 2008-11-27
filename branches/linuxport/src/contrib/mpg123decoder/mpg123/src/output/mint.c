/*
	mint: audio output for MINT

	copyright ?-2006 by the mpg123 project - free software under the terms of the LGPL 2.1
	see COPYING and AUTHORS files in distribution or http://mpg123.org
	initially written by Petr Stehlik
*/

/* derived from LINUX, VOXWARE and SUN for MiNT Audio Device by Petr Stehlik */
#include <fcntl.h>
#include "mpg123app.h"
#include <ioctl.h>
#include <audios.h>
#include "debug.h"

/* Globals */
extern int outburst;
int real_rate_printed = 0;



static int rate_best_match(audio_output_t *ao)
{
	int ret,dsp_rate;
	
	if(!ai || ao->fn < 0 || ao->rate < 0)
		return -1;
	
	dsp_rate = ao->rate;
	ret = ioctl(ao->fn,AIOCSSPEED, (void *)dsp_rate);
	ret = ioctl(ao->fn,AIOCGSPEED,&dsp_rate);
	if(ret < 0) return ret;
	ao->rate = dsp_rate;
	return 0;
}

static int set_rate(audio_output_t *ao)
{
	int dsp_rate = ao->rate;
	
	if(ao->rate >= 0) {
		int ret, real_rate;
		ret = ioctl(ao->fn, AIOCSSPEED, (void *)dsp_rate);
		if (ret >= 0 && !real_rate_printed) {
			ioctl(ao->fn,AIOCGSPEED,&real_rate);
			if (real_rate != dsp_rate) {
				fprintf(stderr, "Replay rate: %d Hz\n", real_rate);
				real_rate_printed = 1;
			}
		}
		return ret;
	}
	
	return 0;
}

static int set_channels(audio_output_t *ao)
{
	int chan = ao->channels;
	
	if(ao->channels < 1) return 0;
	
	return ioctl(ao->fn, AIOCSCHAN, (void *)chan);
}

static int set_format(audio_output_t *ao)
{
	int fmts;
	
	if(ao->format == -1)
		return 0;
	
	switch(ao->format) {
		case MPG123_ENC_SIGNED_16:
		default:
			fmts = AFMT_S16;
		break;
		case MPG123_ENC_UNSIGNED_8:
			fmts = AFMT_U8;
		break;
		case MPG123_ENC_SIGNED_8:
			fmts = AFMT_S8;
		break;
		case MPG123_ENC_ULAW_8:
			fmts = AFMT_ULAW;
		break;
	}
	
	return ioctl(ao->fn, AIOCSFMT, (void *)fmts);
}

static int reset_parameters(audio_output_t *ao)
{
	int ret;
	ret = ioctl(ao->fn,AIOCRESET,NULL);
	if(ret >= 0) ret = set_format(ai);
	if(ret >= 0) ret = set_channels(ai);
	if(ret >= 0) ret = set_rate(ai);
	return ret;
}



static int open_mint(audio_output_t *ao)
{
	if(!ai) return -1;

	if(!ao->device)
		ao->device = "/dev/audio";
	
	ao->fn = open(ao->device,O_WRONLY);  
	
	if(ao->fn < 0)
	{
		error1("Can't open %s!",ao->device);
		return -1;
	}
	ioctl(ao->fn, AIOCGBLKSIZE, &outburst);
	if(outburst > MAXOUTBURST)
		outburst = MAXOUTBURST;
	if(audio_reset_parameters(ai) < 0) {
		close(ao->fn);
		return -1;
	}
	
	return ao->fn;
}

static int get_formats_mint(audio_output_t *ao)
{
	int ret = 0;
	int fmts;
	
	if(ioctl(ao->fn,AIOCGFMTS,&fmts) < 0)
		return -1;
	
	if(fmts & AFMT_ULAW)
		ret |= MPG123_ENC_ULAW_8;
	if(fmts & AFMT_S16)
		ret |= MPG123_ENC_SIGNED_16;
	if(fmts & AFMT_U8)
		ret |= MPG123_ENC_UNSIGNED_8;
	if(fmts & AFMT_S8)
		ret |= MPG123_ENC_SIGNED_8;
	
	return ret;
}

static int write_mint(audio_output_t *ao,unsigned char *buf,int len)
{
	return write(ao->fn,buf,len);
}

static int close_mint(audio_output_t *ao)
{
	close (ao->fn);
	return 0;
}

static void flush_mint(audio_output_t *ao)
{
}


static int init_mint(audio_output_t* ao)
{
	if (ao==NULL) return -1;

	/* Set callbacks */
	ao->open = open_mint;
	ao->flush = flush_mint;
	ao->write = write_mint;
	ao->get_formats = get_formats_mint;
	ao->close = close_mint;

	/* Success */
	return 0;
}





/* 
	Module information data structure
*/
mpg123_module_t mpg123_output_module_info = {
	/* api_version */	MPG123_MODULE_API_VERSION,
	/* name */			"mint",						
	/* description */	"Audio output for MINT.",
	/* revision */		"$Rev:$",						
	/* handle */		NULL,
	
	/* init_output */	init_mint,						
};


