/*
	audio: audio output interface

	copyright ?-2008 by the mpg123 project - free software under the terms of the LGPL 2.1
	see COPYING and AUTHORS files in distribution or http://mpg123.org
	initially written by Michael Hipp
*/

#include <errno.h>
#include "mpg123app.h"
#include "common.h"
#include "buffer.h"

#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif

#include "debug.h"

static int file_write(struct audio_output_struct* ao, unsigned char *bytes, int count)
{
	return (int)write(ao->fn, bytes, count);
}
static int wave_write(struct audio_output_struct* ao, unsigned char *bytes, int count)
{
	return wav_write(bytes, count);
}
static int builtin_get_formats(struct audio_output_struct *ao)
{
	if(param.outmode == DECODE_CDR)
	{
		if(ao->rate == 44100 && ao->channels == 2)
		return MPG123_ENC_SIGNED_16;
		else
		return 0;
	}
	else if(param.outmode == DECODE_AU) return MPG123_ENC_SIGNED_16|MPG123_ENC_UNSIGNED_8|MPG123_ENC_ULAW_8;
	else if(param.outmode == DECODE_WAV) return MPG123_ENC_SIGNED_16|MPG123_ENC_UNSIGNED_8|MPG123_ENC_FLOAT_32|MPG123_ENC_SIGNED_24|MPG123_ENC_SIGNED_32;
	else return MPG123_ENC_ANY;
}
static int builtin_close(struct audio_output_struct *ao)
{
	switch(param.outmode)
	{
		case DECODE_WAV:
		wav_close();
		break;
		case DECODE_AU:
		au_close();
		break;
		case DECODE_CDR:
		cdr_close();
		break;
	}
	return 0;
}
static int  builtin_nothingint(struct audio_output_struct *ao){ return 0; }
static void builtin_nothing(struct audio_output_struct *ao){}

audio_output_t* open_fake_module(void)
{
	audio_output_t *ao = NULL;
	ao = alloc_audio_output();
	if(ao == NULL)
	{
		error("Cannot allocate memory for audio output data.");
		return NULL;
	}
	ao->module = NULL;
	ao->open   = builtin_nothingint;
	ao->flush  = builtin_nothing;
	ao->get_formats = builtin_get_formats;
	ao->write  = wave_write;
	ao->close  = builtin_close;
	ao->device = param.filename;
	ao->is_open = FALSE;
	switch(param.outmode)
	{
		case DECODE_FILE:
			ao->fn    = OutputDescriptor;
			ao->write = file_write;
		break;
		case DECODE_WAV:
			ao->open  = wav_open;
		break;
		case DECODE_CDR:
			ao->open  = cdr_open;
		break;
		case DECODE_AU:
			ao->open  = au_open;
		break;
		case DECODE_TEST:
		break;
	}

	return ao;
}

/* Open an audio output module, trying modules in list (comma-separated). */
audio_output_t* open_output_module( const char* names )
{
	mpg123_module_t *module = NULL;
	audio_output_t *ao = NULL;
	int result = 0;
	char *curname, *modnames;

	if(param.usebuffer || names==NULL) return NULL;

	/* Use internal code. */
	if(param.outmode != DECODE_AUDIO) return open_fake_module();

	modnames = strdup(names);
	if(modnames == NULL)
	{
		error("Error allocating memory for module names.");
		return NULL;
	}
	/* Now loop over the list of possible modules to find one that works. */
	curname = strtok(modnames, ",");
	while(curname != NULL)
	{
		char* name = curname;
		curname = strtok(NULL, ",");
		if(param.verbose > 1) fprintf(stderr, "Trying output module %s.\n", name);
		/* Open the module, initial check for availability+libraries. */
		module = open_module( "output", name );
		if(module == NULL) continue;
		/* Check if module supports output */
		if(module->init_output == NULL)
		{
			error1("Module '%s' does not support audio output.", name);
			close_module(module);
			continue; /* Try next one. */
		}
		/* Allocation+initialization of memory for audio output type. */
		ao = alloc_audio_output();
		if(ao==NULL)
		{
			error("Failed to allocate audio output structure.");
			close_module(module);
			break; /* This is fatal. */
		}

		/* Call the init function */
		ao->device = param.output_device;
		ao->flags  = param.output_flags;
		/* Should I do funny stuff with stderr file descriptor instead? */
		if(curname == NULL)
		{
			if(param.verbose > 1)
			fprintf(stderr, "Note: %s is the last output option... showing you any error messages now.\n", name);
		}
		else ao->auxflags |= MPG123_OUT_QUIET; /* Probing, so don't spill stderr with errors. */
		ao->is_open = FALSE;
		ao->module = module; /* Need that to close module later. */
		result = module->init_output(ao);
		if(result == 0)
		{ /* Try to open the device. I'm only interested in actually working modules. */
			result = open_output(ao);
			close_output(ao);
		}
		else error2("Module '%s' init failed: %i", name, result);

		if(result!=0)
		{ /* Try next one... */
			close_module(module);
			free(ao);
			ao = NULL;
		}
		else 
		{ /* All good, leave the loop. */
			if(param.verbose > 1) fprintf(stderr, "Output module '%s' chosen.\n", name);

			ao->auxflags &= ~MPG123_OUT_QUIET;
			break;
		}
	}

	free(modnames);
	if(ao==NULL) error1("Unable to find a working output module in this list: %s", names);

	return ao;
}



/* Close the audio output and close the module */
void close_output_module( audio_output_t* ao ) 
{
	if (!ao) return; /* That covers buffer mode, too (ao == NULL there). */
	
	debug("closing output module");
	/* Close the audio output */
	if(ao->is_open && ao->close != NULL) ao->close(ao);

	/* Deinitialise the audio output */
	if (ao->deinit) ao->deinit( ao );
	
	/* Unload the module */
	if (ao->module) close_module( ao->module );

	/* Free up memory */
	free( ao );
}



/* allocate and initialise memory */
audio_output_t* alloc_audio_output()
{
	audio_output_t* ao = malloc( sizeof( audio_output_t ) );
	if (ao==NULL) error( "Failed to allocate memory for audio_output_t." );

	/* Initialise variables */
	ao->fn = -1;
	ao->rate = -1;
	ao->gain = param.gain;
	ao->userptr = NULL;
	ao->device = NULL;
	ao->channels = -1;
	ao->format = -1;
	ao->flags = 0;
	ao->auxflags = 0;

	/*ao->module = NULL;*/

	/* Set the callbacks to NULL */
	ao->open = NULL;
	ao->get_formats = NULL;
	ao->write = NULL;
	ao->flush = NULL;
	ao->close = NULL;
	ao->deinit = NULL;
	
	return ao;
}

/*
static void audio_output_dump(audio_output_t *ao)
{
	fprintf(stderr, "ao->fn=%d\n", ao->fn);
	fprintf(stderr, "ao->userptr=%p\n", ao->userptr);
	fprintf(stderr, "ao->rate=%ld\n", ao->rate);
	fprintf(stderr, "ao->gain=%ld\n", ao->gain);
	fprintf(stderr, "ao->device='%s'\n", ao->device);
	fprintf(stderr, "ao->channels=%d\n", ao->channels);
	fprintf(stderr, "ao->format=%d\n", ao->format);
}
*/

struct enc_desc
{
	int code; /* MPG123_ENC_SOMETHING */
	const char *longname; /* signed bla bla */
	const char *name; /* sXX, short name */
	const unsigned char nlen; /* significant characters in short name */
};

static const struct enc_desc encdesc[] =
{
	{ MPG123_ENC_SIGNED_16, "signed 16 bit", "s16 ", 3 },
	{ MPG123_ENC_UNSIGNED_16, "unsigned 16 bit", "u16 ", 3 },
	{ MPG123_ENC_UNSIGNED_8, "unsigned 8 bit", "u8  ", 2 },
	{ MPG123_ENC_SIGNED_8, "signed 8 bit", "s8  ", 2 },
	{ MPG123_ENC_ULAW_8, "mu-law (8 bit)", "ulaw ", 4 },
	{ MPG123_ENC_ALAW_8, "a-law (8 bit)", "alaw ", 4 },
	{ MPG123_ENC_FLOAT_32, "float (32 bit)", "f32 ", 3 },
	{ MPG123_ENC_SIGNED_32, "signed 32 bit", "s32 ", 3 },
	{ MPG123_ENC_UNSIGNED_32, "unsigned 32 bit", "u32 ", 3 },
	{ MPG123_ENC_SIGNED_24, "signed 24 bit", "s24 ", 3 },
	{ MPG123_ENC_UNSIGNED_24, "unsigned 24 bit", "u24 ", 3 }
};
#define KNOWN_ENCS (sizeof(encdesc)/sizeof(struct enc_desc))

void audio_enclist(char** list)
{
	size_t length = 0;
	int i;
	*list = NULL;
	for(i=0;i<KNOWN_ENCS;++i) length += encdesc[i].nlen;

	length += KNOWN_ENCS-1; /* spaces between the encodings */
	*list = malloc(length+1); /* plus zero */
	if(*list != NULL)
	{
		size_t off = 0;
		(*list)[length] = 0;
		for(i=0;i<KNOWN_ENCS;++i)
		{
			if(i>0) (*list)[off++] = ' ';
			memcpy(*list+off, encdesc[i].name, encdesc[i].nlen);
			off += encdesc[i].nlen;
		}
	}
}

/* Safer as function... */
const char* audio_encoding_name(const int encoding, const int longer)
{
	const char *name = longer ? "unknown" : "???";
	int i;
	for(i=0;i<KNOWN_ENCS;++i)
	if(encdesc[i].code == encoding)
	name = longer ? encdesc[i].longname : encdesc[i].name;

	return name;
}

static void capline(mpg123_handle *mh, long rate)
{
	int enci;
	const int  *encs;
	size_t      num_encs;
	mpg123_encodings(&encs, &num_encs);
	fprintf(stderr," %5ld |", pitch_rate(rate));
	for(enci=0; enci<num_encs; ++enci)
	{
		switch(mpg123_format_support(mh, rate, encs[enci]))
		{
			case MPG123_MONO:               fprintf(stderr, "   M   |"); break;
			case MPG123_STEREO:             fprintf(stderr, "   S   |"); break;
			case MPG123_MONO|MPG123_STEREO: fprintf(stderr, "  M/S  |"); break;
			default:                        fprintf(stderr, "       |");
		}
	}
	fprintf(stderr, "\n");
}

void print_capabilities(audio_output_t *ao, mpg123_handle *mh)
{
	int r,e;
	const long *rates;
	size_t      num_rates;
	const int  *encs;
	size_t      num_encs;
	const char *name = "<buffer>";
	const char *dev  = "<none>";
	if(!param.usebuffer)
	{
		name = ao->module ? ao->module->name : "file/raw/test";
		if(ao->device != NULL) dev = ao->device;
	}
	mpg123_rates(&rates, &num_rates);
	mpg123_encodings(&encs, &num_encs);
	fprintf(stderr,"\nAudio driver: %s\nAudio device: %s\nAudio capabilities:\n(matrix of [S]tereo or [M]ono support for sample format and rate in Hz)\n       |", name, dev);
	for(e=0;e<num_encs;e++) fprintf(stderr," %5s |",audio_encoding_name(encs[e], 0));

	fprintf(stderr,"\n ------|");
	for(e=0;e<num_encs;e++) fprintf(stderr,"-------|");

	fprintf(stderr, "\n");
	for(r=0; r<num_rates; ++r) capline(mh, rates[r]);

	if(param.force_rate) capline(mh, param.force_rate);

	fprintf(stderr,"\n");
}

/* This uses the currently opened audio device, queries its caps.
   In case of buffered playback, this works _once_ by querying the buffer for the caps before entering the main loop. */
void audio_capabilities(audio_output_t *ao, mpg123_handle *mh)
{
	int force_fmt = 0;
	int fmts;
	size_t ri;
	/* Pitching introduces a difference between decoder rate and playback rate. */
	long rate, decode_rate;
	int channels;
	const long *rates;
	size_t      num_rates, rlimit;
	debug("audio_capabilities");
	mpg123_rates(&rates, &num_rates);
	mpg123_format_none(mh); /* Start with nothing. */
	if(param.force_encoding != NULL)
	{
		int i;
		if(!param.quiet) fprintf(stderr, "Note: forcing output encoding %s\n", param.force_encoding);

		for(i=0;i<KNOWN_ENCS;++i)
		if(!strncasecmp(encdesc[i].name, param.force_encoding, encdesc[i].nlen))
		{
			force_fmt = encdesc[i].code;
			break;
		}

		if(i==KNOWN_ENCS)
		{
			error1("Failed to find an encoding to match requested \"%s\"!\n", param.force_encoding);
			return; /* No capabilities at all... */
		}
		else if(param.verbose > 2) fprintf(stderr, "Note: forcing encoding code 0x%x\n", force_fmt);
	}
	rlimit = param.force_rate > 0 ? num_rates+1 : num_rates;
	for(channels=1; channels<=2; channels++)
	for(ri = 0;ri<rlimit;ri++)
	{
		decode_rate = ri < num_rates ? rates[ri] : param.force_rate;
		rate = pitch_rate(decode_rate);
		if(param.verbose > 2) fprintf(stderr, "Note: checking support for %liHz/%ich.\n", rate, channels);
#ifndef NOXFERMEM
		if(param.usebuffer)
		{ /* Ask the buffer process. It is waiting for this. */
			buffermem->rate     = rate; 
			buffermem->channels = channels;
			buffermem->format   = 0; /* Just have it initialized safely. */
			debug2("asking for formats for %liHz/%ich", rate, channels);
			xfermem_putcmd(buffermem->fd[XF_WRITER], XF_CMD_AUDIOCAP);
			xfermem_getcmd(buffermem->fd[XF_WRITER], TRUE);
			fmts = buffermem->format;
		}
		else
#endif
		{ /* Check myself. */
			ao->rate     = rate;
			ao->channels = channels;
			fmts = ao->get_formats(ao);
		}
		if(param.verbose > 2) fprintf(stderr, "Note: result 0x%x\n", fmts);
		if(force_fmt)
		{ /* Filter for forced encoding. */
			if((fmts & force_fmt) == force_fmt) fmts = force_fmt;
			else fmts = 0; /* Nothing else! */

			if(param.verbose > 2) fprintf(stderr, "Note: after forcing 0x%x\n", fmts);
		}

		if(fmts < 0) continue;
		else mpg123_format(mh, decode_rate, channels, fmts);
	}

#ifndef NOXFERMEM
	/* Buffer loop shall start normal operation now. */
	if(param.usebuffer)
	{
		xfermem_putcmd(buffermem->fd[XF_WRITER], XF_CMD_WAKEUP);
		xfermem_getcmd(buffermem->fd[XF_WRITER], TRUE);
	}
#endif

	if(param.verbose > 1) print_capabilities(ao, mh);
}

#if !defined(WIN32) && !defined(GENERIC)
#ifndef NOXFERMEM
static void catch_child(void)
{
  while (waitpid(-1, NULL, WNOHANG) > 0);
}
#endif
#endif


/* FIXME: Old output initialization code that needs updating */

int init_output(audio_output_t **ao)
{
	static int init_done = FALSE;
	
	if (init_done) return 1;
	init_done = TRUE;
  
#ifndef NOXFERMEM
	if (param.usebuffer)
	{
		unsigned int bufferbytes;
		sigset_t newsigset, oldsigset;
		bufferbytes = (param.usebuffer * 1024);
		if (bufferbytes < bufferblock)
		{
			bufferbytes = 2*bufferblock;
			if(!param.quiet) fprintf(stderr, "Note: raising buffer to minimal size %liKiB\n", (unsigned long) bufferbytes>>10);
		}
		bufferbytes -= bufferbytes % bufferblock;
		/* No +1024 for NtoM rounding problems anymore! */
		xfermem_init (&buffermem, bufferbytes ,0,0);
		sigemptyset (&newsigset);
		sigaddset (&newsigset, SIGUSR1);
		sigprocmask (SIG_BLOCK, &newsigset, &oldsigset);
#if !defined(WIN32) && !defined(GENERIC)
		catchsignal (SIGCHLD, catch_child);
#endif
		switch ((buffer_pid = fork()))
		{
			case -1: /* error */
			error("cannot fork!");
			return -1;
			case 0: /* child */
			{
				/* Buffer process handles all audio stuff itself. */
				audio_output_t *bao = NULL; /* To be clear: That's the buffer's pointer. */
				param.usebuffer = 0; /* The buffer doesn't use the buffer. */
				/* Open audio output module */
				bao = open_output_module(param.output_module);
				if(!bao)
				{
					error("Failed to open audio output module.");
					exit(1); /* communicate failure? */
				}
				if(open_output(bao) < 0)
				{
					error("Unable to open audio output.");
					close_output_module(bao);
					exit(2);
				}
				xfermem_init_reader (buffermem);
				buffer_loop(bao, &oldsigset); /* Here the work happens. */
				xfermem_done_reader (buffermem);
				xfermem_done (buffermem);
				close_output(bao);
				close_output_module(bao);
				exit(0);
			}
			default: /* parent */
			xfermem_init_writer (buffermem);
		}
	}
#else
	if(param.usebuffer)
	{
		error("Buffer not available in this build!");
		return -1;
	}
#endif
	if(!param.usebuffer)
	{ /* Only if I handle audio device output: Get that module. */
		*ao = open_output_module(param.output_module);
		if(!(*ao))
		{
			error("Failed to open audio output module");
			return -1;
		}
	}
	else *ao = NULL; /* That ensures we won't try to free it later... */
#ifndef NOXFERMEM
	if(param.usebuffer)
	{ /* Check if buffer is alive. */
		int res = xfermem_getcmd(buffermem->fd[XF_WRITER], TRUE);
		if(res < 0)
		{
			error("Buffer process didn't initialize!");
			return -1;
		}
	}
#endif
	/* This has internal protection for buffer mode. */
	if(open_output(*ao) < 0) return -1;

	return 0;
}

void exit_output(audio_output_t *ao, int rude)
{
	debug("exit output");
#ifndef NOXFERMEM
	if (param.usebuffer)
	{
		debug("ending buffer");
		buffer_stop(); /* Puts buffer into waiting-for-command mode. */
		buffer_end(rude);  /* Gives command to end operation. */
		xfermem_done_writer(buffermem);
		waitpid (buffer_pid, NULL, 0);
		xfermem_done (buffermem);
	}
#endif
	/* Close the output... doesn't matter if buffer handled it, that's taken care of. */
	close_output(ao);
	close_output_module(ao);
}

void output_pause(audio_output_t *ao)
{
	if(param.usebuffer) buffer_stop();
	else ao->flush(ao);
}

void output_unpause(audio_output_t *ao)
{
	if(param.usebuffer) buffer_start();
}

int flush_output(audio_output_t *ao, unsigned char *bytes, size_t count)
{
	if(count)
	{
		/* Error checks? */
#ifndef NOXFERMEM
		if(param.usebuffer){ if(xfermem_write(buffermem, bytes, count)) return -1; }
		else
#endif
		if(param.outmode != DECODE_TEST)
		{
			int sum = 0;
			int written;
			do
			{ /* Be in a loop for SIGSTOP/CONT */
				written = ao->write(ao, bytes, (int)count);
				if(written >= 0){ sum+=written; count -= written; }
				else error1("Error in writing audio (%s?)!", strerror(errno));
			}	while(count>0 && written>=0);
			return sum;
		}
	}
	return (int)count; /* That is for DECODE_TEST */
}

int open_output(audio_output_t *ao)
{
	if(param.usebuffer) return 0;

	if(ao == NULL)
	{
		error("ao should not be NULL here!");
		exit(110);
	}

	switch(param.outmode)
	{
		case DECODE_AUDIO:
		case DECODE_WAV:
		case DECODE_AU:
		case DECODE_CDR:
		case DECODE_FILE:
			debug("opening normal audio/file");
			ao->is_open = ao->open(ao) < 0 ? FALSE : TRUE;
			if(!ao->is_open)
			{
				if(!AOQUIET) error("failed to open audio device");
				return -1;
			}
			else return 0;
		break;
		case DECODE_TEST:
			debug("decoding to nowhere");
			return 0;
		break;
	}
	debug("nothing");
	return -1; /* That's an error ... unknown outmode? */
}

/* is this used? */
void close_output(audio_output_t *ao)
{
	if(param.usebuffer) return;

	debug("closing output");
	switch(param.outmode)
	{
		case DECODE_AUDIO:
		case DECODE_WAV:
		case DECODE_AU:
		case DECODE_CDR:
		/* Guard that close call; could be nasty. */
		if(ao->is_open)
		{
			ao->is_open = FALSE;
			if(ao->close != NULL) ao->close(ao);
		}
		break;
	}
}

/* Also for WAV decoding? */
int reset_output(audio_output_t *ao)
{
	if(!param.usebuffer)
	{
		close_output(ao);
		return open_output(ao);
	}
	else return 0;
}

int set_pitch(mpg123_handle *fr, audio_output_t *ao, double new_pitch)
{
	int ret = 1;
	double old_pitch = param.pitch;
	long rate;
	int channels, format;
	int smode = 0;

	if(param.usebuffer)
	{
		error("No runtime pitch change with output buffer, sorry.");
		return 0;
	}

	param.pitch = new_pitch;
	if(param.pitch < -0.99) param.pitch = -0.99;

	/* Be safe, check support. */
	mpg123_getformat(fr, &rate, &channels, &format);
	if(channels == 1) smode = MPG123_MONO;
	if(channels == 2) smode = MPG123_STEREO;

	output_pause(ao);
	/* Remember: This takes param.pitch into account. */
	audio_capabilities(ao, fr);
	if(!(mpg123_format_support(fr, rate, format) & smode))
	{
		/* Note: When using --pitch command line parameter, you can go higher
		   because a lower decoder sample rate is automagically chosen.
		   Here, we'd need to switch decoder rate during track... good? */
		error("Reached a hardware limit there with pitch!");
		param.pitch = old_pitch;
		audio_capabilities(ao, fr);
		ret = 0;
	}
	ao->format   = format;
	ao->channels = channels;
	ao->rate     = pitch_rate(rate);
	reset_output(ao);
	output_unpause(ao);
	return ret;
}

