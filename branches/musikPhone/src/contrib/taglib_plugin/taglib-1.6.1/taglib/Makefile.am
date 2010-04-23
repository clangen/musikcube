SUBDIRS = toolkit mpeg ogg flac ape mpc wavpack trueaudio riff asf mp4

DEFS = -DMAKE_TAGLIB_LIB @DEFS@
INCLUDES = \
	-I$(top_srcdir)/taglib \
	-I$(top_srcdir)/taglib/toolkit \
	-I$(top_srcdir)/taglib/mpeg \
	-I$(top_srcdir)/taglib/ogg \
	-I$(top_srcdir)/taglib/ogg/flac \
	-I$(top_srcdir)/taglib/flac \
	-I$(top_srcdir)/taglib/mpc \
	-I$(top_srcdir)/taglib/asf \
	-I$(top_srcdir)/taglib/mp4 \
	-I$(top_srcdir)/taglib/ogg/vorbis \
	-I$(top_srcdir)/taglib/ogg/speex \
	-I$(top_srcdir)/taglib/wavpack \
	-I$(top_srcdir)/taglib/trueaudio \
	-I$(top_srcdir)/taglib/riff \
	-I$(top_srcdir)/taglib/riff/aiff \
	-I$(top_srcdir)/taglib/riff/wav \
	-I$(top_srcdir)/taglib/mpeg/id3v2 \
	$(all_includes)

lib_LTLIBRARIES = libtag.la

libtag_la_SOURCES = tag.cpp tagunion.cpp fileref.cpp audioproperties.cpp
taglib_include_HEADERS = tag.h fileref.h audioproperties.h taglib_export.h taglib_config.h
taglib_includedir = $(includedir)/taglib

# Here are a set of rules to help you update your library version information:
#  Scheme: current:revsion:age
#  1. Start with version information of `0:0:0' for each libtool library.
#  2. Update the version information only immediately before a public release 
#     of your software. More frequent updates are unnecessary, and only 
#     guarantee that the current interface number gets larger faster.
#  3. If the library source code has changed at all since the last update, 
#     then increment revision (`c:r:a' becomes `c:r+1:a').
#  4. If any interfaces have been added, removed, or changed since the last 
#     update, increment current, and set revision to 0.
#  5. If any interfaces have been added since the last public release, then 
#     increment age.
#  6. If any interfaces have been removed since the last public release, then 
#     set age to 0. 
#  Version history:
#  6:0:5 -- TagLib 1.5
#  7:0:6 -- TagLib 1.6 RC1
#  7:1:6 -- TagLib 1.6
#  8:0:7 -- TagLib 1.6.1

libtag_la_LDFLAGS = $(all_libraries) -no-undefined -version-info 8:0:7
libtag_la_LIBADD = ./mpeg/libmpeg.la ./ogg/libogg.la ./flac/libflac.la ./mpc/libmpc.la \
	./ape/libape.la ./toolkit/libtoolkit.la ./wavpack/libwavpack.la \
	./trueaudio/libtrueaudio.la ./riff/libriff.la \
	./mp4/libmp4.la ./asf/libasf.la
