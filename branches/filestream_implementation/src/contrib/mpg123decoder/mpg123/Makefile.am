## Makefile.am: produce Makefile.in from this

## copyright by the mpg123 project - free software under the terms of the LGPL 2.1
## see COPYING and AUTHORS files in distribution or http://mpg123.org
## initially written by Nicholas J. Humfrey
dist_man_MANS = man1/mpg123.1

# libltdl first as this recursive make mess cannot resolve interdependencies
# Why is recursive make the standard procedure when MAKE DOES NOT WORK THAT WAY??!
SUBDIRS = @LTDLDIR@ src doc
DIST_SUBDIRS = libltdl src doc

# pkg-config file for the mpg123 library
pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libmpg123.pc

EXTRA_DIST = \
	MakeLegacy.sh \
	libmpg123.pc.in \
	mpg123.spec.in \
	mpg123.spec \
	xmms2-plugin/README \
	xmms2-plugin/mpg123/mpg123.c \
	xmms2-plugin/mpg123/wscript \
	makedll.sh \
	test/forkfaint.c \
	test/rms16.c \
	NEWS.libmpg123 \
	autogen.sh \
	ports/MSVC++/libMPG123/libMPG123.vcproj \
	ports/MSVC++/libMPG123/PLACE_LIBMPG123_SOURCES_HERE \
	ports/MSVC++/README \
	ports/MSVC++/SOURCE/CORE_Log.CPP \
	ports/MSVC++/SOURCE/CORE_FileIn.CPP \
	ports/MSVC++/SOURCE/SourceFilter_MP3Stream.CPP \
	ports/MSVC++/SOURCE/CORE_Mutex.CPP \
	ports/MSVC++/INCLUDE/CORE/CORE_FileIn.H \
	ports/MSVC++/INCLUDE/CORE/SourceFilter_MP3.H \
	ports/MSVC++/INCLUDE/IIEP_FileIn.H \
	ports/MSVC++/INCLUDE/IIEP_Def.H \
	ports/README \
	ports/Sony_PSP/config.h \
	ports/Sony_PSP/README \
	ports/Sony_PSP/Makefile.psp \
	ports/Sony_PSP/readers.c.patch \
	ports/mpg123_.pas
	ports/mpg123_msvc.h
