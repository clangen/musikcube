set (m4adecoder_SOURCES
  m4adecoder_plugin.cpp
  M4aDecoderFactory.cpp
  M4aDecoder.cpp
  stdafx.cpp
  mp4ff/mp4atom.c
  mp4ff/mp4ff.c
  mp4ff/mp4meta.c
  mp4ff/mp4sample.c
  mp4ff/mp4tagupdate.c
  mp4ff/mp4util.c
)

include_directories(
  "${CMAKE_CURRENT_SOURCE_DIR}/drms"
  "${CMAKE_CURRENT_SOURCE_DIR}/mp4ff"
)

add_library(m4adecoder SHARED ${m4adecoder_SOURCES})

# prefer static libraries on mac to make redist easier
if (${LINK_STATICALLY} MATCHES "true")
  find_library(FAADLIB NAMES libfaad.a faad)
  target_link_libraries(m4adecoder ${musikcube_LINK_LIBS} ${FAADLIB})
else()
  target_link_libraries(m4adecoder ${musikcube_LINK_LIBS} faad)
endif()

