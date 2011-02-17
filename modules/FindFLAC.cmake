# Copyright (c) 2009, Whispersoft s.r.l.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
# * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
# * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
# * Neither the name of Whispersoft s.r.l. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# Finds FLAC library
#
#  FLAC_INCLUDE_DIR - where to find flac.h, etc.
#  FLAC_LIBRARIES   - List of libraries when using FLAC.
#  FLAC_FOUND       - True if FLAC found.
#

if (FLAC_INCLUDE_DIR)
  # Already in cache, be silent
  set(FLAC_FIND_QUIETLY TRUE)
endif (FLAC_INCLUDE_DIR)

find_path(FLAC_INCLUDE_DIR FLAC/all.h
  /opt/local/include
  /usr/local/include
  /usr/include
)

set(FLAC_NAMES FLAC)
find_library(FLAC_LIBRARY
  NAMES ${FLAC_NAMES}
  PATHS /usr/lib /usr/local/lib /opt/local/lib
)

if (FLAC_INCLUDE_DIR AND FLAC_LIBRARY)
   set(FLAC_FOUND TRUE)
   set( FLAC_LIBRARIES ${FLAC_LIBRARY} )
else (FLAC_INCLUDE_DIR AND FLAC_LIBRARY)
   set(FLAC_FOUND FALSE)
   set(FLAC_LIBRARIES)
endif (FLAC_INCLUDE_DIR AND FLAC_LIBRARY)

if (FLAC_FOUND)
   if (NOT FLAC_FIND_QUIETLY)
      message(STATUS "Found FLAC: ${FLAC_LIBRARY}")
   endif (NOT FLAC_FIND_QUIETLY)
else (FLAC_FOUND)
   if (FLAC_FIND_REQUIRED)
      message(STATUS "Looked for FLAC libraries named ${FLAC_NAMES}.")
      message(STATUS "Include file detected: [${FLAC_INCLUDE_DIR}].")
      message(STATUS "Lib file detected: [${FLAC_LIBRARY}].")
      message(FATAL_ERROR "=========> Could NOT find FLAC library")
   endif (FLAC_FIND_REQUIRED)
endif (FLAC_FOUND)

mark_as_advanced(
  FLAC_LIBRARY
  FLAC_INCLUDE_DIR
  )
