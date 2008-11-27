/*
	httpget: HTTP input routines (the header)

	copyright 2007 by the mpg123 project - free software under the terms of the LGPL 2.1
	see COPYING and AUTHORS files in distribution or http://mpg123.org
	initially written by Thomas Orgis

	Note about MIME types:
	You feed debunk_mime() a MIME string and it classifies it as it is relevant for mpg123.
	In httpget.c are the MIME class lists, which may be appended to to support more bogus MIME types.
*/

#ifndef _HTTPGET_H_
#define _HTTPGET_H_

#include "mpg123.h"

struct httpdata
{
	mpg123_string content_type;
	mpg123_string icy_name;
	mpg123_string icy_url;
	off_t icy_interval;
	mpg123_string proxyhost;
	mpg123_string proxyport;
	/* Partly dummy for now... later proxy host resolution will be cached (PROXY_ADDR). */
	enum { PROXY_UNKNOWN=0, PROXY_NONE, PROXY_HOST, PROXY_ADDR } proxystate;
};

void httpdata_init(struct httpdata *e);
void httpdata_reset(struct httpdata *e);
void httpdata_free(struct httpdata *e);

/* There is a whole lot of MIME types for the same thing.
   the function will reduce it to a combination of these flags */
#define IS_FILE 1
#define IS_LIST 2
#define IS_M3U  4
#define IS_PLS  8
int debunk_mime(const char* mime);

extern char *proxyurl;
extern unsigned long proxyip;
/* takes url and content type string address, opens resource, returns fd for data, allocates and sets content type */
extern int http_open (char* url, struct httpdata *hd);
extern char *httpauth;

#endif
