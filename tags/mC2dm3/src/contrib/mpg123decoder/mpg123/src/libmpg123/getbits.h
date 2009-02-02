/*
	getbits

	copyright ?-2006 by the mpg123 project - free software under the terms of the LGPL 2.1
	see COPYING and AUTHORS files in distribution or http://mpg123.org
	initially written by Michael Hipp
*/

#ifndef _MPG123_GETBITS_H_
#define _MPG123_GETBITS_H_

/* that's the same file as getits.c but with defines to
  force inlining */

#define backbits(fr,nob) ((void)( \
  fr->bitindex    -= nob, \
  fr->wordpointer += (fr->bitindex>>3), \
  fr->bitindex    &= 0x7 ))

#define getbitoffset(fr) ((-fr->bitindex)&0x7)
#define getbyte(fr)      (*fr->wordpointer++)

#define getbits(fr, nob) ( \
  fr->ultmp = fr->wordpointer[0], fr->ultmp <<= 8, fr->ultmp |= fr->wordpointer[1], \
  fr->ultmp <<= 8, fr->ultmp |= fr->wordpointer[2], fr->ultmp <<= fr->bitindex, \
  fr->ultmp &= 0xffffff, fr->bitindex += nob, \
  fr->ultmp >>= (24-nob), fr->wordpointer += (fr->bitindex>>3), \
  fr->bitindex &= 7,fr->ultmp)

#define skipbits(fr, nob) fr->ultmp = ( \
  fr->ultmp = fr->wordpointer[0], fr->ultmp <<= 8, fr->ultmp |= fr->wordpointer[1], \
  fr->ultmp <<= 8, fr->ultmp |= fr->wordpointer[2], fr->ultmp <<= fr->bitindex, \
  fr->ultmp &= 0xffffff, fr->bitindex += nob, \
  fr->ultmp >>= (24-nob), fr->wordpointer += (fr->bitindex>>3), \
  fr->bitindex &= 7 )

#define getbits_fast(fr, nob) ( \
  fr->ultmp = (unsigned char) (fr->wordpointer[0] << fr->bitindex), \
  fr->ultmp |= ((unsigned long) fr->wordpointer[1]<<fr->bitindex)>>8, \
  fr->ultmp <<= nob, fr->ultmp >>= 8, \
  fr->bitindex += nob, fr->wordpointer += (fr->bitindex>>3), \
  fr->bitindex &= 7, fr->ultmp )

#define get1bit(fr) ( \
  fr->uctmp = *fr->wordpointer << fr->bitindex, fr->bitindex++, \
  fr->wordpointer += (fr->bitindex>>3), fr->bitindex &= 7, fr->uctmp>>7 )


#endif
