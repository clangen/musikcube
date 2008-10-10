/*
	mpg123: main code of the program (not of the decoder...)

	copyright 1995-2008 by the mpg123 project - free software under the terms of the LGPL 2.1
	see COPYING and AUTHORS files in distribution or http://mpg123.org
	initially written by Michael Hipp

	mpg123 defines 
	used source: musicout.h from mpegaudio package
*/

#ifndef MPG123_H
#define MPG123_H

/* everyone needs it */
#include "config.h"
#include "httpget.h"
/* import DLL symbols on windows */
#if defined(WIN32) && defined(DYNAMIC_BUILD)
#define LINK_MPG123_DLL
#endif
#include "mpg123.h"
#include "compat.h"
#define MPG123_REMOTE
#define REMOTE_BUFFER_SIZE 2048
#define MAXOUTBURST 32768

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include        <stdio.h>
#include        <string.h>
#ifdef HAVE_SIGNAL_H
#include        <signal.h>
#endif
#include        <math.h>

#ifndef WIN32
#include        <sys/signal.h>
#endif
/* For select(), I need select.h according to POSIX 2001, else: sys/time.h sys/types.h unistd.h */
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
/* want to suport large files in future */
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

/* More integer stuff, just take what we can get... */
#ifdef HAVE_INTTYPES_H
#include <inttypes.h>
#endif
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif
#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif
 
#ifndef SIZE_MAX
#define SIZE_MAX ((size_t)-1)
#endif
#ifndef ULONG_MAX
#define ULONG_MAX ((unsigned long)-1)
#endif

#ifdef OS2
#include <float.h>
#endif

typedef unsigned char byte;
#include "xfermem.h"

#ifdef __GNUC__
#define INLINE inline
#else
#define INLINE
#endif

#include "module.h"
#include "audio.h"
#include "local.h"

extern size_t bufferblock;

#define VERBOSE_MAX 3

extern char* binpath; /* argv[0], actually... */

struct parameter
{
  int aggressive; /* renice to max. priority */
  int shuffle;	/* shuffle/random play */
  int remote;	/* remote operation */
  int remote_err;	/* remote operation to stderr */
  int outmode;	/* where to out the decoded sampels */
  int quiet;	/* shut up! */
  int xterm_title;	/* Change xterm title to song names? */
  long usebuffer;	/* second level buffer size */
  int verbose;    /* verbose level */
  char* output_module;	/* audio output module to use */
  char* output_device;	/* audio output device to use */
  int   output_flags;	/* legacy output destination for AIX/HP/Sun */
#ifdef HAVE_TERMIOS
  int term_ctrl;
#endif
  int checkrange;
  int force_reopen;
  int test_cpu;
  long realtime;
  char *filename;
  long listentry; /* possibility to choose playback of one entry in playlist (0: off, > 0 : select, < 0; just show list*/
  char* listname; /* name of playlist */
  int long_id3;
  int list_cpu;
	char *cpu;
#ifdef FIFO
	char* fifo;
#endif
#ifndef WIN32
	long timeout; /* timeout for reading in seconds */
#endif
	long loop;    /* looping of tracks */
	int delay;
	int index;    /* index / scan through files before playback */
	/* parameters for mpg123 handle */
	int down_sample;
	long rva; /* (which) rva to do: 0: nothing, 1: radio/mix/track 2: album/audiophile */
	long halfspeed;
	long doublespeed;
	long start_frame;  /* frame offset to begin with */
	long frame_number; /* number of frames to decode */
#ifdef FLOATOUT
	double outscale;
#else
	long outscale;
#endif
	int flags;
	long force_rate;
	int talk_icy;
	long resync_limit;
	int smooth;
	double pitch; /* <0 or >0, 0.05 for 5% speedup. */
	int ignore_mime; /* An mpg123 app flag field in future? */
	char *proxyurl;
	int keep_open; /* Whether to keep files open after end reached, for remote control mode, perhaps terminal control, too. */
	int force_utf8; /* Regardless of environment, always print out verbatim UTF for metadata. */
};

extern char *equalfile;
extern off_t framenum;
extern struct httpdata htd;

extern int buffer_fd[2];
extern txfermem *buffermem;
extern int buffer_pid;

#ifndef NOXFERMEM
extern void buffer_loop(audio_output_t *ao,sigset_t *oldsigset);
#endif

extern int OutputDescriptor;

#ifdef VARMODESUPPORT
extern int varmode;
extern int playlimit;
#endif

/* why extern? */
extern void prepare_audioinfo(mpg123_handle *mh, audio_output_t *ao);
extern int play_frame(void);

extern int control_generic(mpg123_handle *fr);

/* Eh... I see duplicated definitions. Clean up after merge! */
extern int cdr_open(audio_output_t *);
extern int au_open(audio_output_t *);
extern int wav_open(audio_output_t *);
extern int wav_write(unsigned char *buf,int len);
extern int cdr_close(void);
extern int au_close(void);
extern int wav_close(void);

extern struct parameter param;

/* avoid the SIGINT in terminal control */
void next_track(void);
void prev_track(void);
int  open_track(char *fname);
void close_track(void);
void set_intflag(void);

/* equalizer... success is 0, failure -1 */
int load_equalizer(mpg123_handle *mh);

#endif 
