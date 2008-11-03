/*
	xfermem: unidirectional fast pipe

	copyright ?-2006 by the mpg123 project - free software under the terms of the LGPL 2.1
	see COPYING and AUTHORS files in distribution or http://mpg123.org
	initially written by Oliver Fromme
	old timestamp: Sat Mar 29 04:41:34 MET 1997

	This is a stand-alone module which implements a unidirectional,
	fast pipe using mmap().  Its primary use is to transfer large
	amounts of data from a parent process to its child process,
	with a buffer in between which decouples blocking conditions
	on both sides.  Control information is transferred between the
	processes through a socketpair.  See xftest.c for an example on
	how to use this module.

	note: xftest not there anymore
*/

#ifndef _XFERMEM_H_
#define _XFERMEM_H_

#ifndef TRUE
#define FALSE 0
#define TRUE  1
#endif

typedef struct {
	size_t freeindex;	/* [W] next free index */
	size_t readindex;	/* [R] next index to read */
	int fd[2];
	int wakeme[2];
	byte *data;
	byte *metadata;
	size_t size;
	size_t metasize;
	long rate;
	int  channels;
	int  format;
	int justwait;
} txfermem;
/*
 *   [W] -- May be written to by the writing process only!
 *   [R] -- May be written to by the reading process only!
 *   All other entries are initialized once.
 */

void xfermem_init (txfermem **xf, size_t bufsize, size_t msize, size_t skipbuf);
void xfermem_init_writer (txfermem *xf);
void xfermem_init_reader (txfermem *xf);

size_t xfermem_get_freespace (txfermem *xf);
size_t xfermem_get_usedspace (txfermem *xf);
#define XF_CMD_WAKEUP_INFO  0x04
#define XF_CMD_WAKEUP    0x02
#define XF_CMD_TERMINATE 0x03
#define XF_CMD_AUDIOCAP  0x05
#define XF_CMD_RESYNC    0x06
#define XF_CMD_ABORT     0x07
#define XF_WRITER 0
#define XF_READER 1
int xfermem_getcmd (int fd, int block);
int xfermem_putcmd (int fd, byte cmd);
int xfermem_block (int fd, txfermem *xf);
/* returns TRUE for being interrupted */
int xfermem_write(txfermem *xf, byte *buffer, size_t bytes);

void xfermem_done (txfermem *xf);
#define xfermem_done_writer xfermem_init_reader
#define xfermem_done_reader xfermem_init_writer


#endif 
