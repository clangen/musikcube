/*
	arts: audio output via aRts Sound Daemon

	copyright 2007-8 by the mpg123 project - free software under the terms of the LGPL 2.1
	see COPYING and AUTHORS files in distribution or http://mpg123.org

	initially written by Stefan Lenselink (Stefan@lenselink.org)
*/


#include "mpg123app.h"
#include <artsc.h>
#include "debug.h"

static int open_arts(audio_output_t *ao)
{
	short bits = 0;
	if(!ao) return -1;

	if(ao->format < 0)
	{
		ao->format = MPG123_ENC_SIGNED_16;
		ao->rate   = 44100;
		ao->channels = 2;
	}
	/* Trial and error revealed these two formats to work with aRts. */
	if(ao->format == MPG123_ENC_SIGNED_16) bits = 16;
	else if(ao->format == MPG123_ENC_UNSIGNED_8) bits = 8;
	else return -1;

	/* Initialize the aRts lib*/
	arts_init();
	/* Open a stream to the aRts server */
	ao->fn = (int)arts_play_stream( ao->rate, bits, ao->channels, "mpg123" );
	/* Postive numbers, please. */
	return ao->fn < 0 ? ao->fn : ao->fn < 1 ? -1 : 0;
}

static int get_formats_arts(audio_output_t *ao)
{
	/* aRts runs not everything, but any rate. */
	return MPG123_ENC_SIGNED_16|MPG123_ENC_UNSIGNED_8;
}

static int write_arts(audio_output_t *ao,unsigned char *buf,int len)
{
	/* PIPE the PCM forward to the aRts Sound Daemon */
	return arts_write( (arts_stream_t)ao->fn , buf, len);
}

static int close_arts(audio_output_t *ao)
{
	/* Close the connection! */
	arts_close_stream( (arts_stream_t)ao->fn );
	/* Free the memory allocated*/
	arts_free();
	return 0;
}

static void flush_arts(audio_output_t *ao)
{
  /* aRts doesn't have a flush statement! */
}

static int init_arts(audio_output_t* ao)
{
	if (ao==NULL) return -1;

	/* Set callbacks */
	ao->open = open_arts;
	ao->flush = flush_arts;
	ao->write = write_arts;
	ao->get_formats = get_formats_arts;
	ao->close = close_arts;
	/* Success */
	return 0;
}

/*
	Module information data structure
*/
mpg123_module_t mpg123_output_module_info = {
	/* api_version */	MPG123_MODULE_API_VERSION,
	/* name */			"arts",
	/* description */	"Output audio using aRts Sound Daemon",
	/* revision */		"$Rev: $",
	/* handle */		NULL,
	
	/* init_output */	init_arts,
};


