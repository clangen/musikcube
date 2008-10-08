/*
	term: terminal control

	copyright ?-2008 by the mpg123 project - free software under the terms of the LGPL 2.1
	see COPYING and AUTHORS files in distribution or http://mpg123.org
	initially written by Michael Hipp
*/

#include "mpg123app.h"

#ifdef HAVE_TERMIOS

#include <termios.h>
#include <ctype.h>

#include "buffer.h"
#include "term.h"
#include "common.h"
#include "playlist.h"
#include "metaprint.h"
#include "debug.h"

extern int buffer_pid;
extern audio_output_t *ao;

static int term_enable = 0;
static struct termios old_tio;
int seeking = FALSE;

/* Hm, next step would be some system in this, plus configurability...
   Two keys for everything? It's just stop/pause for now... */
struct keydef { const char key; const char key2; const char* desc; };
struct keydef term_help[] =
{
	 { STOP_KEY,  ' ', "interrupt/restart playback (i.e. 'pause')" }
	,{ NEXT_KEY,    0, "next track" }
	,{ PREV_KEY,    0, "previous track" }
	,{ BACK_KEY,    0, "back to beginning of track" }
	,{ PAUSE_KEY,   0, "pause while looping current sound chunk" }
	,{ FORWARD_KEY, 0, "forward" }
	,{ REWIND_KEY,  0, "rewind" }
	,{ FAST_FORWARD_KEY, 0, "fast forward" }
	,{ FAST_REWIND_KEY,  0, "fast rewind" }
	,{ FINE_FORWARD_KEY, 0, "fine forward" }
	,{ FINE_REWIND_KEY,  0, "fine rewind" }
	,{ VOL_UP_KEY,   0, "volume up" }
	,{ VOL_DOWN_KEY, 0, "volume down" }
	,{ RVA_KEY,      0, "RVA switch" }
	,{ VERBOSE_KEY,  0, "verbose switch" }
	,{ PLAYLIST_KEY, 0, "list current playlist, indicating current track there" }
	,{ TAG_KEY,      0, "display tag info (again)" }
	,{ MPEG_KEY,     0, "print MPEG header info (again)" }
	,{ HELP_KEY,     0, "this help" }
	,{ QUIT_KEY,     0, "quit" }
	,{ PITCH_UP_KEY, PITCH_BUP_KEY, "pitch up (small step, big step)" }
	,{ PITCH_DOWN_KEY, PITCH_BDOWN_KEY, "pitch down (small step, big step)" }
	,{ PITCH_ZERO_KEY, 0, "reset pitch to zero" }
};

void term_sigcont(int sig);

/* This must call only functions safe inside a signal handler. */
int term_setup(struct termios *pattern)
{
  struct termios tio = *pattern;

  signal(SIGCONT, term_sigcont);

  tio.c_lflag &= ~(ICANON|ECHO); 
  tio.c_cc[VMIN] = 1;
  tio.c_cc[VTIME] = 0;
  return tcsetattr(0,TCSANOW,&tio);
}

void term_sigcont(int sig)
{
  term_enable = 0;

  if (term_setup(&old_tio) < 0) {
    fprintf(stderr,"Can't set terminal attributes\n");
    return;
  }

  term_enable = 1;
}

/* initialze terminal */
void term_init(void)
{
  debug("term_init");

  term_enable = 0;

  if(tcgetattr(0,&old_tio) < 0) {
    fprintf(stderr,"Can't get terminal attributes\n");
    return;
  }
  if(term_setup(&old_tio) < 0) {
    fprintf(stderr,"Can't set terminal attributes\n");
    return;
  }

  term_enable = 1;
}

static void term_handle_input(mpg123_handle *, audio_output_t *, int);

static int stopped = 0;
static int paused = 0;
static int pause_cycle;

static int print_index(mpg123_handle *mh)
{
	int err;
	size_t c, fill;
	off_t *index;
	off_t  step;
	err = mpg123_index(mh, &index, &step, &fill);
	if(err == MPG123_ERR)
	{
		fprintf(stderr, "Error accessing frame index: %s\n", mpg123_strerror(mh));
		return err;
	}
	for(c=0; c < fill;++c) 
		fprintf(stderr, "[%lu] %lu: %li (+%li)\n",
		(unsigned long) c,
		(unsigned long) (c*step), 
		(long) index[c], 
		(long) (c ? index[c]-index[c-1] : 0));
	return MPG123_OK;
}

static off_t offset = 0;

off_t term_control(mpg123_handle *fr, audio_output_t *ao)
{
	offset = 0;

	if(!term_enable) return 0;

	if(paused)
	{
		if(!--pause_cycle)
		{
			pause_cycle=(int)(LOOP_CYCLES/mpg123_tpf(fr));
			offset-=pause_cycle;
			if(param.usebuffer)
			{
				while(paused && xfermem_get_usedspace(buffermem))
				{
					buffer_ignore_lowmem();
					term_handle_input(fr, ao, TRUE);
				}
				if(!paused)	offset += pause_cycle;
			}
		}
	}

	do
	{
		term_handle_input(fr, ao, stopped|seeking);
		if((offset < 0) && (-offset > framenum)) offset = - framenum;
		if(param.verbose && offset != 0)
		print_stat(fr,offset,0);
	} while (stopped);

	/* Make the seeking experience with buffer less annoying.
	   No sound during seek, but at least it is possible to go backwards. */
	if(offset)
	{
		if((offset = mpg123_seek_frame(fr, offset, SEEK_CUR)) >= 0)
		debug1("seeked to %li", (long)offset);
		else error1("seek failed: %s!", mpg123_strerror(fr));
		/* Buffer resync already happened on un-stop? */
		/* if(param.usebuffer) buffer_resync();*/
	}
	return 0;
}

/* Stop playback while seeking if buffer is involved. */
static void seekmode(void)
{
	if(param.usebuffer && !stopped)
	{
		stopped = TRUE;
		buffer_stop();
		fprintf(stderr, "%s", STOPPED_STRING);
	}
}

static void term_handle_input(mpg123_handle *fr, audio_output_t *ao, int do_delay)
{
  int n = 1;
  /* long offset = 0; */
  
  while(n > 0) {
    fd_set r;
    struct timeval t;
    char val;

    t.tv_sec=0;
    t.tv_usec=(do_delay) ? 10*1000 : 0;
    
    FD_ZERO(&r);
    FD_SET(0,&r);
    n = select(1,&r,NULL,NULL,&t);
    if(n > 0 && FD_ISSET(0,&r)) {
      if(read(0,&val,1) <= 0)
        break;

      switch(tolower(val)) {
	case BACK_KEY:
        if(!param.usebuffer) ao->flush(ao);
				else buffer_resync();
		if(paused) pause_cycle=(int)(LOOP_CYCLES/mpg123_tpf(fr));

		if(mpg123_seek_frame(fr, 0, SEEK_SET) < 0)
		error1("Seek to begin failed: %s", mpg123_strerror(fr));

		framenum=0;
		break;
	case NEXT_KEY:
		if(!param.usebuffer) ao->flush(ao);
		else buffer_resync(); /* was: plain_buffer_resync */
	  next_track();
	  break;
	case QUIT_KEY:
		debug("QUIT");
		if(stopped)
		{
			stopped = 0;
			if(param.usebuffer)
			{
				buffer_resync();
				buffer_start();
			}
		}
		set_intflag();
		offset = 0;
	  break;
	case PAUSE_KEY:
  	  paused=1-paused;
	  if(paused) {
			/* Not really sure if that is what is wanted
			   This jumps in audio output, but has direct reaction to pausing loop. */
			if(param.usebuffer) buffer_resync();

		  pause_cycle=(int)(LOOP_CYCLES/mpg123_tpf(fr));
		  offset -= pause_cycle;
	  }
		if(stopped)
		{
			stopped=0;
			if(param.usebuffer) buffer_start();
		}
	  fprintf(stderr, "%s", (paused) ? PAUSED_STRING : EMPTY_STRING);
	  break;
	case STOP_KEY:
	case ' ':
		/* when seeking while stopped and then resuming, I want to prevent the chirp from the past */
		if(!param.usebuffer) ao->flush(ao);
	  stopped=1-stopped;
	  if(paused) {
		  paused=0;
		  offset -= pause_cycle;
	  }
		if(param.usebuffer)
		{
			if(stopped) buffer_stop();
			else
			{
				/* When we stopped buffer for seeking, we must resync. */
				if(offset) buffer_resync();

				buffer_start();
			}
		}
	  fprintf(stderr, "%s", (stopped) ? STOPPED_STRING : EMPTY_STRING);
	  break;
	case FINE_REWIND_KEY:
	  if(param.usebuffer) seekmode();
	  offset--;
	  break;
	case FINE_FORWARD_KEY:
	  seekmode();
	  offset++;
	  break;
	case REWIND_KEY:
	  seekmode();
  	  offset-=10;
	  break;
	case FORWARD_KEY:
	  seekmode();
	  offset+=10;
	  break;
	case FAST_REWIND_KEY:
	  seekmode();
	  offset-=50;
	  break;
	case FAST_FORWARD_KEY:
	  seekmode();
	  offset+=50;
	  break;
	case VOL_UP_KEY:
		mpg123_volume_change(fr, 0.02);
	break;
	case VOL_DOWN_KEY:
		mpg123_volume_change(fr, -0.02);
	break;
	case PITCH_UP_KEY:
	case PITCH_BUP_KEY:
	case PITCH_DOWN_KEY:
	case PITCH_BDOWN_KEY:
	case PITCH_ZERO_KEY:
	{
		double old_pitch = param.pitch;
		long rate;
		int channels, format;
		int smode = 0;
		if(param.usebuffer)
		{
			error("No runtime pitch change with output buffer, sorry.");
			break;
		}
		switch(val) /* Not tolower here! */
		{
			case PITCH_UP_KEY:    param.pitch += PITCH_VAL;  break;
			case PITCH_BUP_KEY:   param.pitch += PITCH_BVAL; break;
			case PITCH_DOWN_KEY:  param.pitch -= PITCH_VAL;  break;
			case PITCH_BDOWN_KEY: param.pitch -= PITCH_BVAL; break;
			case PITCH_ZERO_KEY:  param.pitch = 0.0; break;
		}
		if(param.pitch < -0.99) param.pitch = -0.99;
		/* Be safe, check support. */
		mpg123_getformat(fr, &rate, &channels, &format);
		if(channels == 1) smode = MPG123_MONO;
		if(channels == 2) smode = MPG123_STEREO;

		output_pause(ao);
		audio_capabilities(ao, fr);
		if(!(mpg123_format_support(fr, rate, format) & smode))
		{
			/* Note: When using --pitch command line parameter, you can go higher
			   because a lower decoder sample rate is automagically chosen.
			   Here, we'd need to switch decoder rate during track... good? */
			error("Reached a hardware limit there with pitch!");
			param.pitch = old_pitch;
			audio_capabilities(ao, fr);
		}
		ao->format   = format;
		ao->channels = channels;
		ao->rate     = pitch_rate(rate); 
		reset_output(ao);
		output_unpause(ao);
		fprintf(stderr, "New pitch: %f\n", param.pitch);
	}
	break;
	case VERBOSE_KEY:
		param.verbose++;
		if(param.verbose > VERBOSE_MAX)
		{
			param.verbose = 0;
			clear_stat();
		}
		mpg123_param(fr, MPG123_VERBOSE, param.verbose, 0);
	break;
	case RVA_KEY:
		if(++param.rva > MPG123_RVA_MAX) param.rva = 0;
		mpg123_param(fr, MPG123_RVA, param.rva, 0);
		mpg123_volume(fr, -1);
	break;
	case PREV_KEY:
		if(!param.usebuffer) ao->flush(ao);
		else buffer_resync(); /* was: plain_buffer_resync */

		prev_track();
	break;
	case PLAYLIST_KEY:
		fprintf(stderr, "%s\nPlaylist (\">\" indicates current track):\n", param.verbose ? "\n" : "");
		print_playlist(stderr, 1);
		fprintf(stderr, "\n");
	break;
	case TAG_KEY:
		fprintf(stderr, "%s\n", param.verbose ? "\n" : "");
		print_id3_tag(fr, param.long_id3, stderr);
		fprintf(stderr, "\n");
	break;
	case MPEG_KEY:
		if(param.verbose) print_stat(fr,0,0); /* Make sure that we are talking about the correct frame. */
		fprintf(stderr, "\n");
		print_header(fr);
		fprintf(stderr, "\n");
	break;
	case HELP_KEY:
	{ /* This is more than the one-liner before, but it's less spaghetti. */
		int i;
		fprintf(stderr,"\n\n -= terminal control keys =-\n");
		for(i=0; i<(sizeof(term_help)/sizeof(struct keydef)); ++i)
		{
			if(term_help[i].key2) fprintf(stderr, "[%c] or [%c]", term_help[i].key, term_help[i].key2);
			else fprintf(stderr, "[%c]", term_help[i].key);

			fprintf(stderr, "\t%s\n", term_help[i].desc);
		}
		fprintf(stderr, "\n");
	}
	break;
	case FRAME_INDEX_KEY:
		print_index(fr);
	break;
	default:
	  ;
      }
    }
  }
}

void term_restore(void)
{
  
  if(!term_enable)
    return;

  tcsetattr(0,TCSAFLUSH,&old_tio);
}

#endif

