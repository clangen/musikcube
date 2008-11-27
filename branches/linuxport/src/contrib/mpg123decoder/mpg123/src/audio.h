/*
	audio: audio output interface

	copyright ?-2006 by the mpg123 project - free software under the terms of the LGPL 2.1
	see COPYING and AUTHORS files in distribution or http://mpg123.org
	initially written by Michael Hipp
*/

/* 
 * Audio 'LIB' defines
 */


#ifndef _MPG123_AUDIO_H_
#define _MPG123_AUDIO_H_

#include "mpg123.h"
#include "module.h"

#define AUDIO_OUT_HEADPHONES       0x01
#define AUDIO_OUT_INTERNAL_SPEAKER 0x02
#define AUDIO_OUT_LINE_OUT         0x04

enum {
	DECODE_TEST,
	DECODE_AUDIO,
	DECODE_FILE,
	DECODE_BUFFER,
	DECODE_WAV,
	DECODE_AU,
	DECODE_CDR,
	DECODE_AUDIOFILE
};

/* 3% rate tolerance */
#define AUDIO_RATE_TOLERANCE	  3

typedef struct audio_output_struct
{
	int fn;			/* filenumber */
	void *userptr;	/* driver specific pointer */
	
	/* Callbacks */
	int (*open)(struct audio_output_struct *);
	int (*get_formats)(struct audio_output_struct *);
	int (*write)(struct audio_output_struct *, unsigned char *,int);
	void (*flush)(struct audio_output_struct *);
	int (*close)(struct audio_output_struct *);
	int (*deinit)(struct audio_output_struct *);
	
	/* the module this belongs to */
	mpg123_module_t *module;
	
	char *device;	/* device name */
	int   flags;	/* some bits; namely headphone/speaker/line */
	long rate;		/* sample rate */
	long gain;		/* output gain */
	int channels;	/* number of channels */
	int format;		/* format flags */
	int is_open;	/* something opened? */
#define MPG123_OUT_QUIET 1
	int auxflags; /* For now just one: quiet mode (for probing). */
} audio_output_t;

/* Lazy. */
#define AOQUIET (ao->auxflags & MPG123_OUT_QUIET)

struct audio_format_name {
	int  val;
	char *name;
	char *sname;
};

#define pitch_rate(rate)	(param.pitch == 0 ? (rate) : (long) ((param.pitch+1.0)*(rate)))

/* ------ Declarations from "audio.c" ------ */

audio_output_t* open_output_module( const char* name );
void close_output_module( audio_output_t* ao );
audio_output_t* alloc_audio_output();
void audio_capabilities(audio_output_t *ao, mpg123_handle *mh);
int audio_fit_capabilities(audio_output_t *ao,int c,int r);
const char* audio_encoding_name(const int encoding, const int longer);
void print_capabilities(audio_output_t *ao, mpg123_handle *mh);

int init_output(audio_output_t **ao);
void exit_output(audio_output_t *ao, int rude);
int flush_output(audio_output_t *ao, unsigned char *bytes, size_t count);
int open_output(audio_output_t *ao);
void close_output(audio_output_t *ao );
int reset_output(audio_output_t *ao);
void output_pause(audio_output_t *ao);  /* Prepare output for inactivity. */
void output_unpause(audio_output_t *ao); /* Reactivate output (buffer process). */

#endif

