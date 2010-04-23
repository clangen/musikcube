DEFS = -DMAKE_TAGLIB_LIB @DEFS@
INCLUDES = \
	-I$(top_srcdir)/taglib \
	-I$(top_srcdir)/taglib/toolkit \
	-I$(top_srcdir)/taglib/mpeg/id3v1 \
	$(all_includes)

noinst_LTLIBRARIES = libmp4.la

libmp4_la_SOURCES = mp4atom.cpp mp4file.cpp mp4item.cpp mp4properties.cpp mp4tag.cpp mp4coverart.cpp

taglib_include_HEADERS = mp4atom.h mp4file.h mp4item.h mp4properties.h mp4tag.h mp4coverart.h
taglib_includedir = $(includedir)/taglib
