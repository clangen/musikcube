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
	 { MPG123_STOP_KEY,  ' ', "interrupt/restart playback (i.e. '(un)pause')" }
	,{ MPG123_NEXT_KEY,    0, "next track" }
	,{ MPG123_PREV_KEY,    0, "previous track" }
	,{ MPG123_BACK_KEY,    0, "back to beginning of track" }
	,{ MPG123_PAUSE_KEY,   0, "loop around current position (like a damaged audio CD;-)" }
	,{ MPG123_FORWARD_KEY, 0, "forward" }
	,{ MPG123_REWIND_KEY,  0, "rewind" }
	,{ MPG123_FAST_FORWARD_KEY, 0, "fast forward" }
	,{ MPG123_FAST_REWIND_KEY,  0, "fast rewind" }
	,{ MPG123_FINE_FORWARD_KEY, 0, "fine forward" }
	,{ MPG123_FINE_REWIND_KEY,  0, "fine rewind" }
	,{ MPG123_VOL_UP_KEY,   0, "volume up" }
	,{ MPG123_VOL_DOWN_KEY, 0, "volume down" }
	,{ MPG123_RVA_KEY,      0, "RVA switch" }
	,{ MPG123_VERBOSE_KEY,  0, "verbose switch" }
	,{ MPG123_PLAYLIST_KEY, 0, "list current playlist, indicating current track there" }
	,{ MPG123_TAG_KEY,      0, "display tag info (again)" }
	,{ MPG123_MPEG_KEY,     0, "print MPEG header info (again)" }
	,{ MPG123_HELP_KEY,     0, "this help" }
	,{ MPG123_QUIT_KEY,     0, "quit" }
	,{ MPG123_PITCH_UP_KEY, MPG123_PITCH_BUP_KEY, "pitch up (small step, big step)" }
	,{ MPG123_PITCH_DOWN_KEY, MPG123_PITCH_BDOWN_KEY, "pitch down (small step, big step)" }
	,{ MPG123_PITCH_ZERO_KEY, 0, "reset pitch to zero" }
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

void term_hint(void)
{
	fprintf(stderr, "\nTerminal control enabled, press 'h' for listing of keys and functions.\n\n");
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

/* Go back to the start for the cyclic pausing. */
void pause_recycle(mpg123_handle *fr)
{
	/* Take care not to go backwards in time in steps of 1 frame
		 That is what the +1 is for. */
	pause_cycle=(int)(LOOP_CYCLES/mpg123_tpf(fr));
	offset-=pause_cycle;
}

/* Done with pausing, no offset anymore. Just continuous playback from now. */
void pause_uncycle(void)
{
	offset += pause_cycle;
}

off_t term_control(mpg123_handle *fr, audio_output_t *ao)
{
	offset = 0;
debug1("control for frame: %li", (long)mpg123_tellframe(fr));
	if(!term_enable) return 0;

	if(paused)
	{
		/* pause_cycle counts the remaining frames _after_ this one, thus <0, not ==0 . */
		if(--pause_cycle < 0)
		{
			pause_recycle(fr);
			if(param.usebuffer)
			{
				while(paused && xfermem_get_usedspace(buffermem))
				{
					buffer_ignore_lowmem();
					term_handle_input(fr, ao, TRUE);
				}
				/* Undo the cycling offset if we are done with cycling. */
				if(!paused)	pause_uncycle();
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
		fprintf(stderr, "%s", MPG123_STOPPED_STRING);
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
	case MPG123_BACK_KEY:
        if(!param.usebuffer) ao->flush(ao);
				else buffer_resync();
		if(paused) pause_cycle=(int)(LOOP_CYCLES/mpg123_tpf(fr));

		if(mpg123_seek_frame(fr, 0, SEEK_SET) < 0)
		error1("Seek to begin failed: %s", mpg123_strerror(fr));

		framenum=0;
		break;
	case MPG123_NEXT_KEY:
		if(!param.usebuffer) ao->flush(ao);
		else buffer_resync(); /* was: plain_buffer_resync */
	  next_track();
	  break;
	case MPG123_QUIT_KEY:
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
	case MPG123_PAUSE_KEY:
  	  paused=1-paused;
	  if(paused) {
			/* Not really sure if that is what is wanted
			   This jumps in audio output, but has direct reaction to pausing loop. */
			if(param.usebuffer) buffer_resync();

			pause_recycle(fr);
	  }
		if(stopped)
		{
			stopped=0;
			if(param.usebuffer) buffer_start();
		}
	  fprintf(stderr, "%s", (paused) ? MPG123_PAUSED_STRING : MPG123_EMPTY_STRING);
	  break;
	case MPG123_STOP_KEY:
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
	  fprintf(stderr, "%s", (stopped) ? MPG123_STOPPED_STRING : MPG123_EMPTY_STRING);
	  break;
	case MPG123_FINE_REWIND_KEY:
	  if(param.usebuffer) seekmode();
	  offset--;
	  break;
	case MPG123_FINE_FORWARD_KEY:
	  seekmode();
	  offset++;
	  break;
	case MPG123_REWIND_KEY:
	  seekmode();
  	  offset-=10;
	  break;
	case MPG123_FORWARD_KEY:
	  seekmode();
	  offset+=10;
	  break;
	case MPG123_FAST_REWIND_KEY:
	  seekmode();
	  offset-=50;
	  break;
	case MPG123_FAST_FORWARD_KEY:
	  seekmode();
	  offset+=50;
	  break;
	case MPG123_VOL_UP_KEY:
		mpg123_volume_change(fr, 0.02);
	break;
	case MPG123_VOL_DOWN_KEY:
		mpg123_volume_change(fr, -0.02);
	break;
	case MPG123_PITCH_UP_KEY:
	case MPG123_PITCH_BUP_KEY:
	case MPG123_PITCH_DOWN_KEY:
	case MPG123_PITCH_BDOWN_KEY:
	case MPG123_PITCH_ZERO_KEY:
	{
		double new_pitch = param.pitch;
		switch(val) /* Not tolower here! */
		{
			case MPG123_PITCH_UP_KEY:    new_pitch += MPG123_PITCH_VAL;  break;
			case MPG123_PITCH_BUP_KEY:   new_pitch += MPG123_PITCH_BVAL; break;
			case MPG123_PITCH_DOWN_KEY:  new_pitch -= MPG123_PITCH_VAL;  break;
			case MPG123_PITCH_BDOWN_KEY: new_pitch -= MPG123_PITCH_BVAL; break;
			case MPG123_PITCH_ZERO_KEY:  new_pitch = 0.0; break;
		}
		set_pitch(fr, ao, new_pitch);
		fprintf(stderr, "New pitch: %f\n", param.pitch);
	}
	break;
	case MPG123_VERBOSE_KEY:
		param.verbose++;
		if(param.verbose > VERBOSE_MAX)
		{
			param.verbose = 0;
			clear_stat();
		}
		mpg123_param(fr, MPG123_VERBOSE, param.verbose, 0);
	break;
	case MPG123_RVA_KEY:
		if(++param.rva > MPG123_RVA_MAX) param.rva = 0;
		mpg123_param(fr, MPG123_RVA, param.rva, 0);
		mpg123_volume_change(fr, 0.);
	break;
	case MPG123_PREV_KEY:
		if(!param.usebuffer) ao->flush(ao);
		else buffer_resync(); /* was: plain_buffer_resync */

		prev_track();
	break;
	case MPG123_PLAYLIST_KEY:
		fprintf(stderr, "%s\nPlaylist (\">\" indicates current track):\n", param.verbose ? "\n" : "");
		print_playlist(stderr, 1);
		fprintf(stderr, "\n");
	break;
	case MPG123_TAG_KEY:
		fprintf(stderr, "%s\n", param.verbose ? "\n" : "");
		print_id3_tag(fr, param.long_id3, stderr);
		fprintf(stderr, "\n");
	break;
	case MPG123_MPEG_KEY:
		if(param.verbose) print_stat(fr,0,0); /* Make sure that we are talking about the correct frame. */
		fprintf(stderr, "\n");
		print_header(fr);
		fprintf(stderr, "\n");
	break;
	case MPG123_HELP_KEY:
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
	case MPG123_FRAME_INDEX_KEY:
	case MPG123_VARIOUS_INFO_KEY:
		if(param.verbose) fprintf(stderr, "\n");
		switch(val) /* because of tolower() ... */
		{
			case MPG123_FRAME_INDEX_KEY:
			print_index(fr);
			{
				long accurate;
				if(mpg123_getstate(fr, MPG123_ACCURATE, &accurate, NULL) == MPG123_OK)
				fprintf(stderr, "Accurate position: %s\n", (accurate == 0 ? "no" : "yes"));
				else
				error1("Unable to get state: %s", mpg123_strerror(fr));
			}
			break;
			case MPG123_VARIOUS_INFO_KEY:
			{
				const char* curdec = mpg123_current_decoder(fr);
				if(curdec == NULL) fprintf(stderr, "Cannot get decoder info!\n");
				else fprintf(stderr, "Active decoder: %s\n", curdec);
			}
		}
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

