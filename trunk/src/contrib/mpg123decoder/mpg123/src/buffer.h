/*
	buffer.h: output buffer

	copyright 1999-2006 by the mpg123 project - free software under the terms of the LGPL 2.1
	see COPYING and AUTHORS files in distribution or http://mpg123.org
	initially written by Daniel Kobras / Oliver Fromme
*/

/*
 * Application specific interaction between main and buffer
 * process. This is much less generic than the functions in
 * xfermem so I chose to put it in buffer.[hc].
 * 01/28/99 [dk]
 */

#ifndef _MPG123_BUFFER_H_
#define _MPG123_BUFFER_H_

#ifndef NOXFERMEM
void real_buffer_ignore_lowmem(void);
void real_buffer_end(int rude);
void real_buffer_resync(void);
void real_plain_buffer_resync(void);
void real_buffer_reset(void);
void real_buffer_start(void);
void real_buffer_stop(void);
/* Hm, that's funny preprocessor weirdness. */
#define buffer_start()         (param.usebuffer ? real_buffer_start(),0         : 0)
#define buffer_stop()          (param.usebuffer ? real_buffer_stop(),0          : 0)
#define buffer_reset()         (param.usebuffer ? real_buffer_reset(),0         : 0)
#define buffer_resync()        (param.usebuffer ? real_buffer_resync(),0        : 0)
#define plain_buffer_resync()  (param.usebuffer ? real_plain_buffer_resync(),0  : 0)
#define buffer_end(a)          (param.usebuffer ? real_buffer_end(a),0           : 0)
#define buffer_ignore_lowmem() (param.usebuffer ? real_buffer_ignore_lowmem(),0 : 0)
#else
#define buffer_start()
#define buffer_stop()
#define buffer_reset()
#define buffer_resync()
#define plain_buffer_resync()
#define buffer_end()
#define buffer_ignore_lowmem()
#endif

#endif
