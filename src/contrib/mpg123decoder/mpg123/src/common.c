/*
	common: misc stuff... audio flush, status display...

	copyright ?-2008 by the mpg123 project - free software under the terms of the LGPL 2.1
	see COPYING and AUTHORS files in distribution or http://mpg123.org
	initially written by Michael Hipp
*/

#include "mpg123app.h"
#include <sys/stat.h>
#include <fcntl.h>
#include "common.h"

#include "debug.h"

const char* rva_name[3] = { "off", "mix", "album" };
static const char *modes[5] = {"Stereo", "Joint-Stereo", "Dual-Channel", "Single-Channel", "Invalid" };
static const char *smodes[5] = { "stereo", "joint-stereo", "dual-channel", "mono", "invalid" };
static const char *layers[4] = { "Unknown" , "I", "II", "III" };
static const char *versions[4] = {"1.0", "2.0", "2.5", "x.x" };
static const int samples_per_frame[4][4] =
{
	{ -1,384,1152,1152 },	/* MPEG 1 */
	{ -1,384,1152,576 },	/* MPEG 2 */
	{ -1,384,1152,576 },	/* MPEG 2.5 */
	{ -1,-1,-1,-1 },		/* Unknown */
};


#if (!defined(WIN32) || defined (__CYGWIN__)) && defined(HAVE_SIGNAL_H)
void (*catchsignal(int signum, void(*handler)()))()
{
  struct sigaction new_sa;
  struct sigaction old_sa;

#ifdef DONT_CATCH_SIGNALS
  fprintf (stderr, "Not catching any signals.\n");
  return ((void (*)()) -1);
#endif

  new_sa.sa_handler = handler;
  sigemptyset(&new_sa.sa_mask);
  new_sa.sa_flags = 0;
  if (sigaction(signum, &new_sa, &old_sa) == -1)
    return ((void (*)()) -1);
  return (old_sa.sa_handler);
}
#endif

/* concurring to print_rheader... here for control_generic */
const char* remote_header_help = "S <mpeg-version> <layer> <sampling freq> <mode(stereo/mono/...)> <mode_ext> <framesize> <stereo> <copyright> <error_protected> <emphasis> <bitrate> <extension> <vbr(0/1=yes/no)>";
void print_remote_header(mpg123_handle *mh)
{
	struct mpg123_frameinfo i;
	mpg123_info(mh, &i);
	if(i.mode >= 4 || i.mode < 0) i.mode = 4;
	if(i.version >= 3 || i.version < 0) i.version = 3;
	generic_sendmsg("S %s %d %ld %s %d %d %d %d %d %d %d %d %d",
		versions[i.version],
		i.layer,
		i.rate,
		modes[i.mode],
		i.mode_ext,
		i.framesize,
		i.mode == MPG123_M_MONO ? 1 : 2,
		i.flags & MPG123_COPYRIGHT ? 1 : 0,
		i.flags & MPG123_CRC ? 1 : 0,
		i.emphasis,
		i.bitrate,
		i.flags & MPG123_PRIVATE ? 1 : 0,
		i.vbr);
}

void print_header(mpg123_handle *mh)
{
	struct mpg123_frameinfo i;
	mpg123_info(mh, &i);
	if(i.mode > 4 || i.mode < 0) i.mode = 4;
	if(i.version > 3 || i.version < 0) i.version = 3;
	if(i.layer > 3 || i.layer < 0) i.layer = 0;
	fprintf(stderr,"MPEG %s, Layer: %s, Freq: %ld, mode: %s, modext: %d, BPF : %d\n", 
		versions[i.version],
		layers[i.layer], i.rate,
		modes[i.mode],i.mode_ext,i.framesize);
	fprintf(stderr,"Channels: %d, copyright: %s, original: %s, CRC: %s, emphasis: %d.\n",
		i.mode == MPG123_M_MONO ? 1 : 2,i.flags & MPG123_COPYRIGHT ? "Yes" : "No",
		i.flags & MPG123_ORIGINAL ? "Yes" : "No", i.flags & MPG123_CRC ? "Yes" : "No",
		i.emphasis);
	fprintf(stderr,"Bitrate: ");
	switch(i.vbr)
	{
		case MPG123_CBR:
			if(i.bitrate) fprintf(stderr, "%d kbit/s", i.bitrate);
			else fprintf(stderr, "%d kbit/s (free format)", (int)((double)(i.framesize+4)*8*i.rate*0.001/samples_per_frame[i.version][i.layer]+0.5));
			break;
		case MPG123_VBR: fprintf(stderr, "VBR"); break;
		case MPG123_ABR: fprintf(stderr, "%d kbit/s ABR", i.abr_rate); break;
		default: fprintf(stderr, "???");
	}
	fprintf(stderr, " Extension value: %d\n",	i.flags & MPG123_PRIVATE ? 1 : 0);
}

void print_header_compact(mpg123_handle *mh)
{
	struct mpg123_frameinfo i;
	mpg123_info(mh, &i);
	if(i.mode > 4 || i.mode < 0) i.mode = 4;
	if(i.version > 3 || i.version < 0) i.version = 3;
	if(i.layer > 3 || i.layer < 0) i.layer = 0;
	
	fprintf(stderr,"MPEG %s layer %s, ", versions[i.version], layers[i.layer]);
	switch(i.vbr)
	{
		case MPG123_CBR:
			if(i.bitrate) fprintf(stderr, "%d kbit/s", i.bitrate);
			else fprintf(stderr, "%d kbit/s (free format)", (int)((double)i.framesize*8*i.rate*0.001/samples_per_frame[i.version][i.layer]+0.5));
			break;
		case MPG123_VBR: fprintf(stderr, "VBR"); break;
		case MPG123_ABR: fprintf(stderr, "%d kbit/s ABR", i.abr_rate); break;
		default: fprintf(stderr, "???");
	}
	fprintf(stderr,", %ld Hz %s\n", i.rate, smodes[i.mode]);
}

#if 0
/* removed the strndup for better portability */
/*
 *   Allocate space for a new string containing the first
 *   "num" characters of "src".  The resulting string is
 *   always zero-terminated.  Returns NULL if malloc fails.
 */
char *strndup (const char *src, int num)
{
	char *dst;

	if (!(dst = (char *) malloc(num+1)))
		return (NULL);
	dst[num] = '\0';
	return (strncpy(dst, src, num));
}
#endif

/*
 *   Split "path" into directory and filename components.
 *
 *   Return value is 0 if no directory was specified (i.e.
 *   "path" does not contain a '/'), OR if the directory
 *   is the same as on the previous call to this function.
 *
 *   Return value is 1 if a directory was specified AND it
 *   is different from the previous one (if any).
 */

int split_dir_file (const char *path, char **dname, char **fname)
{
	static char *lastdir = NULL;
	char *slashpos;

	if ((slashpos = strrchr(path, '/'))) {
		*fname = slashpos + 1;
		*dname = strdup(path); /* , 1 + slashpos - path); */
		if(!(*dname)) {
			perror("failed to allocate memory for dir name");
			return 0;
		}
		(*dname)[1 + slashpos - path] = 0;
		if (lastdir && !strcmp(lastdir, *dname)) {
			/***   same as previous directory   ***/
			free (*dname);
			*dname = lastdir;
			return 0;
		}
		else {
			/***   different directory   ***/
			if (lastdir)
				free (lastdir);
			lastdir = *dname;
			return 1;
		}
	}
	else {
		/***   no directory specified   ***/
		if (lastdir) {
			free (lastdir);
			lastdir = NULL;
		};
		*dname = NULL;
		*fname = (char *)path;
		return 0;
	}
}

unsigned int roundui(double val)
{
	double base = floor(val);
	return (unsigned int) ((val-base) < 0.5 ? base : base + 1 );
}

void print_stat(mpg123_handle *fr, long offset, long buffsize)
{
	double tim1,tim2;
	off_t rno, no;
	double basevol, realvol;
	char *icy;
#ifndef WIN32
#ifndef GENERIC
/* Only generate new stat line when stderr is ready... don't overfill... */
	{
		struct timeval t;
		fd_set serr;
		int n,errfd = fileno(stderr);

		t.tv_sec=t.tv_usec=0;

		FD_ZERO(&serr);
		FD_SET(errfd,&serr);
		n = select(errfd+1,NULL,&serr,NULL,&t);
		if(n <= 0) return;
	}
#endif
#endif
	if(    MPG123_OK == mpg123_position(fr, offset, buffsize, &no, &rno, &tim1, &tim2)
	    && MPG123_OK == mpg123_getvolume(fr, &basevol, &realvol, NULL) )
	{
		fprintf(stderr, "\rFrame# %5"OFF_P" [%5"OFF_P"], Time: %02lu:%02u.%02u [%02u:%02u.%02u], RVA:%6s, Vol: %3u(%3u)",
		        (off_p)no, (off_p)rno,
		        (unsigned long) tim1/60, (unsigned int)tim1%60, (unsigned int)(tim1*100)%100,
		        (unsigned int)tim2/60, (unsigned int)tim2%60, (unsigned int)(tim2*100)%100,
		        rva_name[param.rva], roundui(basevol*100), roundui(realvol*100) );
		if(param.usebuffer) fprintf(stderr,", [%8ld] ",(long)buffsize);
	}
	/* Check for changed tags here too? */
	if( mpg123_meta_check(fr) & MPG123_NEW_ICY && MPG123_OK == mpg123_icy(fr, &icy) )
	fprintf(stderr, "\nICY-META: %s\n", icy);
}

void clear_stat()
{
	fprintf(stderr, "\r                                                                                       \r");
}
