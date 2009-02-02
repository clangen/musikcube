/*
	playlist: playlist logic

	copyright 1995-2007 by the mpg123 project - free software under the terms of the LGPL 2.1
	see COPYING and AUTHORS files in distribution or http://mpg123.org
	initially written by Michael Hipp, outsourced/reorganized by Thomas Orgis
*/
#ifndef MPG123_PLAYLIST_H
#define MPG123_PLAYLIST_H

#include "mpg123app.h"

enum playlist_type { UNKNOWN = 0, M3U, PLS, NO_LIST };

typedef struct listitem
{
	char* url; /* the filename */
	char freeit; /* if it was allocated and should be free()d here */
} listitem;

typedef struct playlist_struct
{
	FILE* file; /* the current playlist stream */
	size_t entry; /* entry in the playlist file */
	long loop;    /* repeat a track n times */
	size_t size;
	size_t fill;
	size_t pos;
	size_t alloc_step;
	struct listitem* list;
	mpg123_string linebuf;
	mpg123_string dir;
	enum playlist_type type;
} playlist_struct;

extern struct playlist_struct pl;

/* create playlist form argv including reading of playlist file */
void prepare_playlist(int argc, char** argv);
/* returns the next url to play or NULL when there is none left */
char *get_next_file();
/* frees memory that got allocated in prepare_playlist */
void free_playlist();
/* Print out the playlist, with optional position indicator. */
void print_playlist(FILE* out, int showpos);

#endif
