/*
	alsa: sound output with Advanced Linux Sound Architecture 1.x API

	copyright 2006-8 by the mpg123 project - free software under the terms of the LGPL 2.1
	see COPYING and AUTHORS files in distribution or http://mpg123.org

	initially written by Clemens Ladisch <clemens@ladisch.de>
*/

#include "mpg123app.h"
#include "audio.h"
#include "module.h"
#include <errno.h>

/* make ALSA 0.9.x compatible to the 1.0.x API */
#define ALSA_PCM_NEW_HW_PARAMS_API
#define ALSA_PCM_NEW_SW_PARAMS_API

#include <alsa/asoundlib.h>

#include "debug.h"

/* My laptop has probs playing low-sampled files with only 0.5s buffer... this should be a user setting -- ThOr */
#define BUFFER_LENGTH 0.5	/* in seconds */

static const struct {
	snd_pcm_format_t alsa;
	int mpg123;
} format_map[] = {
	{ SND_PCM_FORMAT_S16,    MPG123_ENC_SIGNED_16   },
	{ SND_PCM_FORMAT_U16,    MPG123_ENC_UNSIGNED_16 },
	{ SND_PCM_FORMAT_U8,     MPG123_ENC_UNSIGNED_8  },
	{ SND_PCM_FORMAT_S8,     MPG123_ENC_SIGNED_8    },
	{ SND_PCM_FORMAT_A_LAW,  MPG123_ENC_ALAW_8      },
	{ SND_PCM_FORMAT_MU_LAW, MPG123_ENC_ULAW_8      },
};
#define NUM_FORMATS (sizeof format_map / sizeof format_map[0])


static int rates_match(long int desired, unsigned int actual)
{
	return actual * 100 > desired * (100 - AUDIO_RATE_TOLERANCE) &&
	       actual * 100 < desired * (100 + AUDIO_RATE_TOLERANCE);
}

static int initialize_device(audio_output_t *ao)
{
	snd_pcm_hw_params_t *hw=NULL;
	snd_pcm_sw_params_t *sw=NULL;
	snd_pcm_uframes_t buffer_size;
	snd_pcm_uframes_t period_size;
	snd_pcm_format_t format;
	snd_pcm_t *pcm=(snd_pcm_t*)ao->userptr;
	unsigned int rate;
	int i;

	snd_pcm_hw_params_alloca(&hw); /* Ignore GCC warning here... alsa-lib>=1.0.16 doesn't trigger that anymore, too. */
	if (snd_pcm_hw_params_any(pcm, hw) < 0) {
		if(!AOQUIET) error("initialize_device(): no configuration available");
		return -1;
	}
	if (snd_pcm_hw_params_set_access(pcm, hw, SND_PCM_ACCESS_RW_INTERLEAVED) < 0) {
		if(!AOQUIET) error("initialize_device(): device does not support interleaved access");
		return -1;
	}
	format = SND_PCM_FORMAT_UNKNOWN;
	for (i = 0; i < NUM_FORMATS; ++i) {
		if (ao->format == format_map[i].mpg123) {
			format = format_map[i].alsa;
			break;
		}
	}
	if (format == SND_PCM_FORMAT_UNKNOWN) {
		if(!AOQUIET) error1("initialize_device(): invalid sample format %d", ao->format);
		errno = EINVAL;
		return -1;
	}
	if (snd_pcm_hw_params_set_format(pcm, hw, format) < 0) {
		if(!AOQUIET) error1("initialize_device(): cannot set format %s", snd_pcm_format_name(format));
		return -1;
	}
	if (snd_pcm_hw_params_set_channels(pcm, hw, ao->channels) < 0) {
		if(!AOQUIET) error1("initialize_device(): cannot set %d channels", ao->channels);
		return -1;
	}
	rate = ao->rate;
	if (snd_pcm_hw_params_set_rate_near(pcm, hw, &rate, NULL) < 0) {
		if(!AOQUIET) error1("initialize_device(): cannot set rate %u", rate);
		return -1;
	}
	if (!rates_match(ao->rate, rate)) {
		if(!AOQUIET) error2("initialize_device(): rate %ld not available, using %u", ao->rate, rate);
		/* return -1; */
	}
	buffer_size = rate * BUFFER_LENGTH;
	if (snd_pcm_hw_params_set_buffer_size_near(pcm, hw, &buffer_size) < 0) {
		if(!AOQUIET) error("initialize_device(): cannot set buffer size");
		return -1;
	}
	period_size = buffer_size / 4;
	if (snd_pcm_hw_params_set_period_size_near(pcm, hw, &period_size, NULL) < 0) {
		if(!AOQUIET) error("initialize_device(): cannot set period size");
		return -1;
	}
	if (snd_pcm_hw_params(pcm, hw) < 0) {
		if(!AOQUIET) error("initialize_device(): cannot set hw params");
		return -1;
	}

	snd_pcm_sw_params_alloca(&sw);
	if (snd_pcm_sw_params_current(pcm, sw) < 0) {
		if(!AOQUIET) error("initialize_device(): cannot get sw params");
		return -1;
	}
	/* start playing after the first write */
	if (snd_pcm_sw_params_set_start_threshold(pcm, sw, 1) < 0) {
		if(!AOQUIET) error("initialize_device(): cannot set start threshold");
		return -1;
	}
	/* wake up on every interrupt */
	if (snd_pcm_sw_params_set_avail_min(pcm, sw, 1) < 0) {
		if(!AOQUIET) error("initialize_device(): cannot set min available");
		return -1;
	}
#if SND_LIB_VERSION < ((1<<16)|16)
	/* Always write as many frames as possible (deprecated since alsa-lib 1.0.16) */
	if (snd_pcm_sw_params_set_xfer_align(pcm, sw, 1) < 0) {
		if(!AOQUIET) error("initialize_device(): cannot set transfer alignment");
		return -1;
	}
#endif
	if (snd_pcm_sw_params(pcm, sw) < 0) {
		if(!AOQUIET) error("initialize_device(): cannot set sw params");
		return -1;
	}
	return 0;
}

void error_ignorer(const char *file, int line, const char *function, int err, const char *fmt,...)
{
	/* I can make ALSA silent. */
}

static int open_alsa(audio_output_t *ao)
{
	const char *pcm_name;
	snd_pcm_t *pcm=NULL;
	debug1("open_alsa with %p", ao->userptr);

	snd_lib_error_set_handler(AOQUIET ? error_ignorer : NULL);
	pcm_name = ao->device ? ao->device : "default";
	if (snd_pcm_open(&pcm, pcm_name, SND_PCM_STREAM_PLAYBACK, 0) < 0) {
		if(!AOQUIET) error1("cannot open device %s", pcm_name);
		return -1;
	}
	ao->userptr = pcm;
	if (ao->format != -1) {
		/* we're going to play: initalize sample format */
		return initialize_device(ao);
	} else {
		/* query mode; sample format will be set for each query */
		return 0;
	}
}


static int get_formats_alsa(audio_output_t *ao)
{
	snd_pcm_t *pcm=(snd_pcm_t*)ao->userptr;
	snd_pcm_hw_params_t *hw;
	unsigned int rate;
	int supported_formats, i;

	snd_pcm_hw_params_alloca(&hw);
	if (snd_pcm_hw_params_any(pcm, hw) < 0) {
		if(!AOQUIET) error("get_formats_alsa(): no configuration available");
		return -1;
	}
	if (snd_pcm_hw_params_set_access(pcm, hw, SND_PCM_ACCESS_RW_INTERLEAVED) < 0)
		return -1;
	if (snd_pcm_hw_params_set_channels(pcm, hw, ao->channels) < 0)
		return 0;
	rate = ao->rate;
	if (snd_pcm_hw_params_set_rate_near(pcm, hw, &rate, NULL) < 0)
		return -1;
	if (!rates_match(ao->rate, rate))
		return 0;
	supported_formats = 0;
	for (i = 0; i < NUM_FORMATS; ++i) {
		if (snd_pcm_hw_params_test_format(pcm, hw, format_map[i].alsa) == 0)
			supported_formats |= format_map[i].mpg123;
	}
	return supported_formats;
}

static int write_alsa(audio_output_t *ao, unsigned char *buf, int bytes)
{
	snd_pcm_t *pcm=(snd_pcm_t*)ao->userptr;
	snd_pcm_uframes_t frames;
	snd_pcm_sframes_t written;

	frames = snd_pcm_bytes_to_frames(pcm, bytes);
	written = snd_pcm_writei(pcm, buf, frames);
	if (written == -EINTR) /* interrupted system call */
		written = 0;
	else if (written == -EPIPE) { /* underrun */
		if (snd_pcm_prepare(pcm) >= 0)
			written = snd_pcm_writei(pcm, buf, frames);
	}
	if (written >= 0)
		return snd_pcm_frames_to_bytes(pcm, written);
	else
	{
		if(snd_pcm_state(pcm) == SND_PCM_STATE_SUSPENDED)
		{
			/* Iamnothappyabouthisnothappyreallynot. */
			snd_pcm_resume(pcm);
			if(snd_pcm_state(pcm) == SND_PCM_STATE_SUSPENDED)
			{
				error("device still suspended after resume hackery... giving up");
				return -1;
			}
		}
		return 0;
	}
}

static void flush_alsa(audio_output_t *ao)
{
	snd_pcm_t *pcm=(snd_pcm_t*)ao->userptr;

	/* is this the optimal solution? - we should figure out what we really whant from this function */
	snd_pcm_drop(pcm);
	snd_pcm_prepare(pcm);
}

static int close_alsa(audio_output_t *ao)
{
	snd_pcm_t *pcm=(snd_pcm_t*)ao->userptr;
	debug1("close_alsa with %p", ao->userptr);
	if(pcm != NULL) /* be really generous for being called without any device opening */
	{
		if (snd_pcm_state(pcm) == SND_PCM_STATE_RUNNING)
			snd_pcm_drain(pcm);
		ao->userptr = NULL; /* Should alsa do this or the module wrapper? */
		return snd_pcm_close(pcm);
	}
	else return 0;
}


static int init_alsa(audio_output_t* ao)
{
	if (ao==NULL) return -1;

	/* Set callbacks */
	ao->open = open_alsa;
	ao->flush = flush_alsa;
	ao->write = write_alsa;
	ao->get_formats = get_formats_alsa;
	ao->close = close_alsa;

	/* Success */
	return 0;
}



/* 
	Module information data structure
*/
mpg123_module_t mpg123_output_module_info = {
	/* api_version */	MPG123_MODULE_API_VERSION,
	/* name */			"alsa",						
	/* description */	"Output audio using Advanced Linux Sound Architecture (ALSA).",
	/* revision */		"$Rev:$",						
	/* handle */		NULL,
	
	/* init_output */	init_alsa,						
};

