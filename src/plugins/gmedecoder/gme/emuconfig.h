/////////////////////////////////////////////////////////////////////////////
//
// Configuration for emulation libraries
//
/////////////////////////////////////////////////////////////////////////////

#ifndef __EMUCONFIG_H__
#define __EMUCONFIG_H__

/////////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

/////////////////////////////////////////////////////////////////////////////
//
// WIN32 native project definitions
//
/////////////////////////////////////////////////////////////////////////////
#if defined(WIN32) && !defined(__GNUC__)

#define EMU_CALL   __fastcall
#define EMU_CALL_  __cdecl
#define EMU_INLINE __inline

#define uint8  unsigned char
#define uint16 unsigned short
#define uint32 unsigned int
#define uint64 unsigned __int64
#define sint8    signed char
#define sint16   signed short
#define sint32   signed int
#define sint64   signed __int64

/////////////////////////////////////////////////////////////////////////////
//
// LINUX / other platform definitions
//
/////////////////////////////////////////////////////////////////////////////
#else

//#if defined(__GNUC__) && defined(__i386__)
//#define EMU_CALL __attribute__((__regparm__(2)))
//#else
#define EMU_CALL
//#endif

#define EMU_CALL_
#define EMU_INLINE __inline

#ifdef HAVE_STDINT_H
#include <stdint.h>
#define uint8  uint8_t
#define uint16 uint16_t
#define uint32 uint32_t
#define uint64 uint64_t
#define sint8  int8_t
#define sint16 int16_t
#define sint32 int32_t
#define sint64 int64_t
#else
#define uint8  unsigned char
#define uint16 unsigned short
#define uint32 unsigned int
#define uint64 unsigned long long
#define sint8    signed char
#define sint16   signed short
#define sint32   signed int
#define sint64   signed long long
#endif

#endif

/////////////////////////////////////////////////////////////////////////////

#endif
