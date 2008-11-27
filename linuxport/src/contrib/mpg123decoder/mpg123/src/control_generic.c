/*
	control_generic.c: control interface for frontends and real console warriors

	copyright 1997-99,2004-8 by the mpg123 project - free software under the terms of the LGPL 2.1
	see COPYING and AUTHORS files in distribution or http://mpg123.org
	initially written by Andreas Neuhaus and Michael Hipp
	reworked by Thomas Orgis - it was the entry point for eventually becoming maintainer...
*/

#include "mpg123app.h"
#include <stdarg.h>
#ifndef WIN32
#include <sys/wait.h>
#include <sys/socket.h>
#else
#include <winsock.h>
#endif
#include <fcntl.h>
#include <errno.h>
#include <string.h>

#include "common.h"
#include "buffer.h"
#include "genre.h"
#define MODE_STOPPED 0
#define MODE_PLAYING 1
#define MODE_PAUSED 2

extern int buffer_pid;
#ifdef FIFO
#include <sys/stat.h>
int control_file = STDIN_FILENO;
#else
#define control_file STDIN_FILENO
#endif
FILE *outstream;
static int mode = MODE_STOPPED;
static int init = 0;

#include "debug.h"

void generic_sendmsg (const char *fmt, ...)
{
	va_list ap;
	fprintf(outstream, "@");
	va_start(ap, fmt);
	vfprintf(outstream, fmt, ap);
	va_end(ap);
	fprintf(outstream, "\n");
}

/* Split up a number of lines separated by \n, \r, both or just zero byte
   and print out each line with specified prefix. */
static void generic_send_lines(const char* fmt, mpg123_string *inlines)
{
	size_t i;
	int hadcr = 0, hadlf = 0;
	char *lines = NULL;
	char *line  = NULL;
	size_t len = 0;

	if(inlines != NULL && inlines->fill)
	{
		lines = inlines->p;
		len   = inlines->fill;
	}
	else return;

	line = lines;
	for(i=0; i<len; ++i)
	{
		if(lines[i] == '\n' || lines[i] == '\r' || lines[i] == 0)
		{
			char save = lines[i]; /* saving, changing, restoring a byte in the data */
			if(save == '\n') ++hadlf;
			if(save == '\r') ++hadcr;
			if((hadcr || hadlf) && hadlf % 2 == 0 && hadcr % 2 == 0) line = "";

			if(line)
			{
				lines[i] = 0;
				generic_sendmsg(fmt, line);
				line = NULL;
				lines[i] = save;
			}
		}
		else
		{
			hadlf = hadcr = 0;
			if(line == NULL) line = lines+i;
		}
	}
}

void generic_sendstat (mpg123_handle *fr)
{
	off_t current_frame, frames_left;
	double current_seconds, seconds_left;
	if(!mpg123_position(fr, 0, xfermem_get_usedspace(buffermem), &current_frame, &frames_left, &current_seconds, &seconds_left))
	generic_sendmsg("F %li %li %3.2f %3.2f", current_frame, frames_left, current_seconds, seconds_left);
}

static void generic_sendv1(mpg123_id3v1 *v1, const char *prefix)
{
	int i;
	char info[125] = "";
	memcpy(info,    v1->title,   30);
	memcpy(info+30, v1->artist,  30);
	memcpy(info+60, v1->album,   30);
	memcpy(info+90, v1->year,     4);
	memcpy(info+94, v1->comment, 30);

	for(i=0;i<124; ++i) if(info[i] == 0) info[i] = ' ';
	info[i] = 0;
	generic_sendmsg("%s ID3:%s%s", prefix, info, (v1->genre<=genre_count) ? genre_table[v1->genre] : "Unknown");
	generic_sendmsg("%s ID3.genre:%i", prefix, v1->genre);
	if(v1->comment[28] == 0 && v1->comment[29] != 0)
	generic_sendmsg("%s ID3.track:%i", prefix, (unsigned char)v1->comment[29]);
}

static void generic_sendinfoid3(mpg123_handle *mh)
{
	mpg123_id3v1 *v1;
	mpg123_id3v2 *v2;
	if(MPG123_OK != mpg123_id3(mh, &v1, &v2))
	{
		error1("Cannot get ID3 data: %s", mpg123_strerror(mh));
		return;
	}
	if(v1 != NULL)
	{
		generic_sendv1(v1, "I");
	}
	if(v2 != NULL)
	{
		generic_send_lines("I ID3v2.title:%s",   v2->title);
		generic_send_lines("I ID3v2.artist:%s",  v2->artist);
		generic_send_lines("I ID3v2.album:%s",   v2->album);
		generic_send_lines("I ID3v2.year:%s",    v2->year);
		generic_send_lines("I ID3v2.comment:%s", v2->comment);
		generic_send_lines("I ID3v2.genre:%s",   v2->genre);
	}
}

void generic_sendalltag(mpg123_handle *mh)
{
	mpg123_id3v1 *v1;
	mpg123_id3v2 *v2;
	generic_sendmsg("T {");
	if(MPG123_OK != mpg123_id3(mh, &v1, &v2))
	{
		error1("Cannot get ID3 data: %s", mpg123_strerror(mh));
		v2 = NULL;
		v1 = NULL;
	}
	if(v1 != NULL) generic_sendv1(v1, "T");

	if(v2 != NULL)
	{
		size_t i;
		for(i=0; i<v2->texts; ++i)
		{
			char id[5];
			memcpy(id, v2->text[i].id, 4);
			id[4] = 0;
			generic_sendmsg("T ID3v2.%s:", id);
			generic_send_lines("T =%s", &v2->text[i].text);
		}
		for(i=0; i<v2->extras; ++i)
		{
			char id[5];
			memcpy(id, v2->extra[i].id, 4);
			id[4] = 0;
			generic_sendmsg("T ID3v2.%s desc(%s):",
			        id,
			        v2->extra[i].description.fill ? v2->extra[i].description.p : "" );
			generic_send_lines("T =%s", &v2->extra[i].text);
		}
		for(i=0; i<v2->comments; ++i)
		{
			char id[5];
			char lang[4];
			memcpy(id, v2->comment_list[i].id, 4);
			id[4] = 0;
			memcpy(lang, v2->comment_list[i].lang, 3);
			lang[3] = 0;
			generic_sendmsg("T ID3v2.%s lang(%s) desc(%s):",
			                id, lang,
			                v2->comment_list[i].description.fill ? v2->comment_list[i].description.p : "");
			generic_send_lines("T =%s", &v2->comment_list[i].text);
		}
	}
	generic_sendmsg("T }");
}

void generic_sendinfo (char *filename)
{
	char *s, *t;
	s = strrchr(filename, '/');
	if (!s)
		s = filename;
	else
		s++;
	t = strrchr(s, '.');
	if (t)
		*t = 0;
	generic_sendmsg("I %s", s);
}

static void generic_load(mpg123_handle *fr, char *arg, int state)
{
	if(param.usebuffer)
	{
		buffer_resync();
		if(mode == MODE_PAUSED && state != MODE_PAUSED) buffer_start();
	}
	if(mode != MODE_STOPPED)
	{
		close_track();
		mode = MODE_STOPPED;
	}
	if(!open_track(arg))
	{
		generic_sendmsg("E Error opening stream: %s", arg);
		generic_sendmsg("P 0");
		return;
	}
	mpg123_seek(fr, 0, SEEK_SET); /* This finds ID3v2 at beginning. */
	if(mpg123_meta_check(fr) & MPG123_NEW_ID3) generic_sendinfoid3(fr);
	else generic_sendinfo(arg);

	if(htd.icy_name.fill) generic_sendmsg("I ICY-NAME: %s", htd.icy_name.p);
	if(htd.icy_url.fill)  generic_sendmsg("I ICY-URL: %s", htd.icy_url.p);

	mode = state;
	init = 1;
	generic_sendmsg(mode == MODE_PAUSED ? "P 1" : "P 2");
}

int control_generic (mpg123_handle *fr)
{
	struct timeval tv;
	fd_set fds;
	int n;

	/* ThOr */
	char alive = 1;
	char silent = 0;

	/* responses to stderr for frontends needing audio data from stdout */
	if (param.remote_err)
 		outstream = stderr;
 	else
 		outstream = stdout;
 		
#ifndef WIN32
 	setlinebuf(outstream);
#else /* perhaps just use setvbuf as it's C89 */
	fprintf(outstream, "You are on Win32 and want to use the control interface... tough luck: We need a replacement for select on STDIN first.\n");
	return 0;
	setvbuf(outstream, (char*)NULL, _IOLBF, 0);
#endif
	/* the command behaviour is different, so is the ID */
	/* now also with version for command availability */
	fprintf(outstream, "@R MPG123 (ThOr) v4\n");
#ifdef FIFO
	if(param.fifo)
	{
		if(param.fifo[0] == 0)
		{
			error("You wanted an empty FIFO name??");
			return 1;
		}
		unlink(param.fifo);
		if(mkfifo(param.fifo, 0666) == -1)
		{
			error2("Failed to create FIFO at %s (%s)", param.fifo, strerror(errno));
			return 1;
		}
		debug("going to open named pipe ... blocking until someone gives command");
		control_file = open(param.fifo,O_RDONLY);
		debug("opened");
	}
#endif

	while (alive)
	{
		tv.tv_sec = 0;
		tv.tv_usec = 0;
		FD_ZERO(&fds);
		FD_SET(control_file, &fds);
		/* play frame if no command needs to be processed */
		if (mode == MODE_PLAYING) {
			n = select(32, &fds, NULL, NULL, &tv);
			if (n == 0) {
				if (!play_frame())
				{
					/* When the track ended, user may want to keep it open (to seek back),
					   so there is a decision between stopping and pausing at the end. */
					if(param.keep_open)
					{
						mode = MODE_PAUSED;
						/* Hm, buffer should be stopped already, shouldn't it? */
						if(param.usebuffer) buffer_stop();
						generic_sendmsg("P 1");
					}
					else
					{
						mode = MODE_STOPPED;
						close_track();
						generic_sendmsg("P 0");
					}
					continue;
				}
				if (init) {
					print_remote_header(fr);
					init = 0;
				}
				if(silent == 0)
				{
					generic_sendstat(fr);
					if(mpg123_meta_check(fr) & MPG123_NEW_ICY)
					{
						char *meta;
						if(mpg123_icy(fr, &meta) == MPG123_OK)
						generic_sendmsg("I ICY-META: %s", meta != NULL ? meta : "<nil>");
					}
				}
			}
		}
		else {
			/* wait for command */
			while (1) {
				n = select(32, &fds, NULL, NULL, NULL);
				if (n > 0)
					break;
			}
		}

		/*  on error */
		if (n < 0) {
			fprintf(stderr, "Error waiting for command: %s\n", strerror(errno));
			return 1;
		}

		/* read & process commands */
		if (n > 0)
		{
			short int len = 1; /* length of buffer */
			char *cmd, *arg; /* variables for parsing, */
			char *comstr = NULL; /* gcc thinks that this could be used uninitialited... */ 
			char buf[REMOTE_BUFFER_SIZE];
			short int counter;
			char *next_comstr = buf; /* have it initialized for first command */

			/* read as much as possible, maybe multiple commands */
			/* When there is nothing to read (EOF) or even an error, it is the end */
			if((len = read(control_file, buf, REMOTE_BUFFER_SIZE)) < 1)
			{
#ifdef FIFO
				if(len == 0 && param.fifo)
				{
					debug("fifo ended... reopening");
					close(control_file);
					control_file = open(param.fifo,O_RDONLY|O_NONBLOCK);
					if(control_file < 0){ error1("open of fifo failed... %s", strerror(errno)); break; }
					continue;
				}
#endif
				if(len < 0) error1("command read error: %s", strerror(errno));
				break;
			}

			debug1("read %i bytes of commands", len);
			/* one command on a line - separation by \n -> C strings in a row */
			for(counter = 0; counter < len; ++counter)
			{
				/* line end is command end */
				if( (buf[counter] == '\n') || (buf[counter] == '\r') )
				{
					debug1("line end at counter=%i", counter);
					buf[counter] = 0; /* now it's a properly ending C string */
					comstr = next_comstr;

					/* skip the additional line ender of \r\n or \n\r */
					if( (counter < (len - 1)) && ((buf[counter+1] == '\n') || (buf[counter+1] == '\r')) ) buf[++counter] = 0;

					/* next "real" char is first of next command */
					next_comstr = buf + counter+1;

					/* directly process the command now */
					debug1("interpreting command: %s", comstr);
				if(strlen(comstr) == 0) continue;

				/* PAUSE */
				if (!strcasecmp(comstr, "P") || !strcasecmp(comstr, "PAUSE")) {
					if(mode != MODE_STOPPED)
					{	
						if (mode == MODE_PLAYING) {
							mode = MODE_PAUSED;
							if(param.usebuffer) buffer_stop();
							generic_sendmsg("P 1");
						} else {
							mode = MODE_PLAYING;
							if(param.usebuffer) buffer_start();
							generic_sendmsg("P 2");
						}
					} else generic_sendmsg("P 0");
					continue;
				}

				/* STOP */
				if (!strcasecmp(comstr, "S") || !strcasecmp(comstr, "STOP")) {
					if (mode != MODE_STOPPED) {
						if(param.usebuffer)
						{
							buffer_stop();
							buffer_resync();
						}
						close_track();
						mode = MODE_STOPPED;
						generic_sendmsg("P 0");
					} else generic_sendmsg("P 0");
					continue;
				}

				/* SILENCE */
				if(!strcasecmp(comstr, "SILENCE")) {
					silent = 1;
					generic_sendmsg("silence");
					continue;
				}

				if(!strcasecmp(comstr, "T") || !strcasecmp(comstr, "TAG")) {
					generic_sendalltag(fr);
					continue;
				}

				if(!strcasecmp(comstr, "SCAN"))
				{
					if(mode != MODE_STOPPED)
					{
						if(mpg123_scan(fr) == MPG123_OK)
						generic_sendmsg("SCAN done");
						else
						generic_sendmsg("E %s", mpg123_strerror(fr));
					}
					else generic_sendmsg("E No track loaded!");

					continue;
				}

				if(!strcasecmp(comstr, "SAMPLE"))
				{
					off_t pos = mpg123_tell(fr);
					off_t len = mpg123_length(fr);
					/* I need to have portable printf specifiers that do not truncate the type... more autoconf... */
					generic_sendmsg("SAMPLE %li %li", (long)pos, (long)len);
					continue;
				}

				if(!strcasecmp(comstr, "SHOWEQ"))
				{
					int i;
					generic_sendmsg("SHOWEQ {");
					for(i=0; i<32; ++i)
					{
						generic_sendmsg("SHOWEQ %i : %i : %f", MPG123_LEFT, i, mpg123_geteq(fr, MPG123_LEFT, i));
						generic_sendmsg("SHOWEQ %i : %i : %f", MPG123_RIGHT, i, mpg123_geteq(fr, MPG123_RIGHT, i));
					}
					generic_sendmsg("SHOWEQ }");
					continue;
				}

				/* QUIT */
				if (!strcasecmp(comstr, "Q") || !strcasecmp(comstr, "QUIT")){
					alive = FALSE; continue;
				}

				/* some HELP */
				if (!strcasecmp(comstr, "H") || !strcasecmp(comstr, "HELP")) {
					generic_sendmsg("H {");
					generic_sendmsg("H HELP/H: command listing (LONG/SHORT forms), command case insensitve");
					generic_sendmsg("H LOAD/L <trackname>: load and start playing resource <trackname>");
					generic_sendmsg("H LOADPAUSED/LP <trackname>: load but do not start playing resource <trackname>");
					generic_sendmsg("H PAUSE/P: pause playback");
					generic_sendmsg("H STOP/S: stop playback (closes file)");
					generic_sendmsg("H JUMP/J <frame>|<+offset>|<-offset>|<[+|-]seconds>s: jump to mpeg frame <frame> or change position by offset, same in seconds if number followed by \"s\"");
					generic_sendmsg("H VOLUME/V <percent>: set volume in % (0..100...); float value");
					generic_sendmsg("H RVA off|(mix|radio)|(album|audiophile): set rva mode");
					generic_sendmsg("H EQ/E <channel> <band> <value>: set equalizer value for frequency band 0 to 31 on channel %i (left) or %i (right) or %i (both)", MPG123_LEFT, MPG123_RIGHT, MPG123_LR);
					 generic_sendmsg("H EQFILE <filename>: load EQ settings from a file");
					generic_sendmsg("H SHOWEQ: show all equalizer settings (as <channel> <band> <value> lines in a SHOWEQ block (like TAG))");
					generic_sendmsg("H SEEK/K <sample>|<+offset>|<-offset>: jump to output sample position <samples> or change position by offset");
					generic_sendmsg("H SCAN: scan through the file, building seek index");
					generic_sendmsg("H SAMPLE: print out the sample position and total number of samples");
					generic_sendmsg("H SEQ <bass> <mid> <treble>: simple eq setting...");
					generic_sendmsg("H SILENCE: be silent during playback (meaning silence in text form)");
					generic_sendmsg("H TAG/T: Print all available (ID3) tag info, for ID3v2 that gives output of all collected text fields, using the ID3v2.3/4 4-character names.");
					generic_sendmsg("H    The output is multiple lines, begin marked by \"@T {\", end by \"@T }\".");
					generic_sendmsg("H    ID3v1 data is like in the @I info lines (see below), just with \"@T\" in front.");
					generic_sendmsg("H    An ID3v2 data field is introduced via ([ ... ] means optional):");
					generic_sendmsg("H     @T ID3v2.<NAME>[ [lang(<LANG>)] desc(<description>)]:");
					generic_sendmsg("H    The lines of data follow with \"=\" prefixed:");
					generic_sendmsg("H     @T =<one line of content in UTF-8 encoding>");
					generic_sendmsg("H meaning of the @S stream info:");
					generic_sendmsg("H %s", remote_header_help);
					generic_sendmsg("H The @I lines after loading a track give some ID3 info, the format:");
					generic_sendmsg("H      @I ID3:artist  album  year  comment genretext");
					generic_sendmsg("H     where artist,album and comment are exactly 30 characters each, year is 4 characters, genre text unspecified.");
					generic_sendmsg("H     You will encounter \"@I ID3.genre:<number>\" and \"@I ID3.track:<number>\".");
					generic_sendmsg("H     Then, there is an excerpt of ID3v2 info in the structure");
					generic_sendmsg("H      @I ID3v2.title:Blabla bla Bla");
					generic_sendmsg("H     for every line of the \"title\" data field. Likewise for other fields (author, album, etc).");
					generic_sendmsg("H }");
					continue;
				}

				/* commands with arguments */
				cmd = NULL;
				arg = NULL;
				cmd = strtok(comstr," \t"); /* get the main command */
				arg = strtok(NULL,""); /* get the args */

				if (cmd && strlen(cmd) && arg && strlen(arg))
				{
					/* Simple EQ: SEQ <BASS> <MID> <TREBLE>  */
					if (!strcasecmp(cmd, "SEQ")) {
						double b,m,t;
						int cn;
						if(sscanf(arg, "%lf %lf %lf", &b, &m, &t) == 3)
						{
							/* Consider adding mpg123_seq()... but also, on could define a nicer courve for that. */
							if ((t >= 0) && (t <= 3))
							for(cn=0; cn < 1; ++cn)	mpg123_eq(fr, MPG123_LEFT|MPG123_RIGHT, cn, b);

							if ((m >= 0) && (m <= 3))
							for(cn=1; cn < 2; ++cn) mpg123_eq(fr, MPG123_LEFT|MPG123_RIGHT, cn, m);

							if ((b >= 0) && (b <= 3))
							for(cn=2; cn < 32; ++cn) mpg123_eq(fr, MPG123_LEFT|MPG123_RIGHT, cn, t);

							generic_sendmsg("bass: %f mid: %f treble: %f", b, m, t);
						}
						else generic_sendmsg("E invalid arguments for SEQ: %s", arg);
						continue;
					}

					/* Equalizer control :) (JMG) */
					if (!strcasecmp(cmd, "E") || !strcasecmp(cmd, "EQ")) {
						double e; /* ThOr: equalizer is of type real... whatever that is */
						int c, v;
						/*generic_sendmsg("%s",updown);*/
						if(sscanf(arg, "%i %i %lf", &c, &v, &e) == 3)
						{
							if(mpg123_eq(fr, c, v, e) == MPG123_OK)
							generic_sendmsg("%i : %i : %f", c, v, e);
							else
							generic_sendmsg("E failed to set eq: %s", mpg123_strerror(fr));
						}
						else generic_sendmsg("E invalid arguments for EQ: %s", arg);
						continue;
					}

					if(!strcasecmp(cmd, "EQFILE"))
					{
						equalfile = arg;
						if(load_equalizer(fr) == 0)
						generic_sendmsg("EQFILE done");
						else
						generic_sendmsg("E failed to parse given eq file");

						continue;
					}

					/* SEEK to a sample offset */
					if(!strcasecmp(cmd, "K") || !strcasecmp(cmd, "SEEK"))
					{
						off_t soff;
						char *spos = arg;
						int whence = SEEK_SET;
						if(mode == MODE_STOPPED)
						{
							generic_sendmsg("E No track loaded!");
							continue;
						}

						soff = atol(spos);
						if(spos[0] == '-' || spos[0] == '+') whence = SEEK_CUR;
						if(0 > (soff = mpg123_seek(fr, soff, whence)))
						{
							generic_sendmsg("E Error while seeking: %s", mpg123_strerror(fr));
							mpg123_seek(fr, 0, SEEK_SET);
						}
						if(param.usebuffer) buffer_resync();

						generic_sendmsg("K %li", (long)mpg123_tell(fr));
						continue;
					}
					/* JUMP */
					if (!strcasecmp(cmd, "J") || !strcasecmp(cmd, "JUMP")) {
						char *spos;
						off_t offset;
						double secs;

						spos = arg;
						if(mode == MODE_STOPPED)
						{
							generic_sendmsg("E No track loaded!");
							continue;
						}

						if(spos[strlen(spos)-1] == 's' && sscanf(arg, "%lf", &secs) == 1) offset = mpg123_timeframe(fr, secs);
						else offset = atol(spos);
						/* totally replaced that stuff - it never fully worked
						   a bit usure about why +pos -> spos+1 earlier... */
						if (spos[0] == '-' || spos[0] == '+') offset += framenum;

						if(0 > (framenum = mpg123_seek_frame(fr, offset, SEEK_SET)))
						{
							generic_sendmsg("E Error while seeking");
							mpg123_seek_frame(fr, 0, SEEK_SET);
						}
						if(param.usebuffer)	buffer_resync();

						generic_sendmsg("J %d", framenum);
						continue;
					}

					/* VOLUME in percent */
					if(!strcasecmp(cmd, "V") || !strcasecmp(cmd, "VOLUME"))
					{
						double v;
						mpg123_volume(fr, atof(arg)/100);
						mpg123_getvolume(fr, &v, NULL, NULL); /* Necessary? */
						generic_sendmsg("V %f%%", v * 100);
						continue;
					}

					/* RVA mode */
					if(!strcasecmp(cmd, "RVA"))
					{
						if(!strcasecmp(arg, "off")) param.rva = MPG123_RVA_OFF;
						else if(!strcasecmp(arg, "mix") || !strcasecmp(arg, "radio")) param.rva = MPG123_RVA_MIX;
						else if(!strcasecmp(arg, "album") || !strcasecmp(arg, "audiophile")) param.rva = MPG123_RVA_ALBUM;
						mpg123_volume(fr, -1);
						generic_sendmsg("RVA %s", rva_name[param.rva]);
						continue;
					}

					/* LOAD - actually play */
					if (!strcasecmp(cmd, "L") || !strcasecmp(cmd, "LOAD")){ generic_load(fr, arg, MODE_PLAYING); continue; }

					/* LOADPAUSED */
					if (!strcasecmp(cmd, "LP") || !strcasecmp(cmd, "LOADPAUSED")){ generic_load(fr, arg, MODE_PAUSED); continue; }

					/* no command matched */
					generic_sendmsg("E Unknown command: %s", cmd); /* unknown command */
				} /* end commands with arguments */
				else generic_sendmsg("E Unknown command or no arguments: %s", comstr); /* unknown command */

				} /* end of single command processing */
			} /* end of scanning the command buffer */

			/*
			   when last command had no \n... should I discard it?
			   Ideally, I should remember the part and wait for next
				 read() to get the rest up to a \n. But that can go
				 to infinity. Too long commands too quickly are just
				 bad. Cannot/Won't change that. So, discard the unfinished
				 command and have fingers crossed that the rest of this
				 unfinished one qualifies as "unknown". 
			*/
			if(buf[len-1] != 0)
			{
				char lasti = buf[len-1];
				buf[len-1] = 0;
				generic_sendmsg("E Unfinished command: %s%c", comstr, lasti);
			}
		} /* end command reading & processing */
	} /* end main (alive) loop */
	debug("going to end");
	/* quit gracefully */
#ifndef NOXFERMEM
	if (param.usebuffer) {
		kill(buffer_pid, SIGINT);
		xfermem_done_writer(buffermem);
		waitpid(buffer_pid, NULL, 0);
		xfermem_done(buffermem);
	}
#endif
	debug("closing control");
#ifdef FIFO
	close(control_file); /* be it FIFO or STDIN */
	if(param.fifo) unlink(param.fifo);
#endif
	debug("control_generic returning");
	return 0;
}

/* EOF */
