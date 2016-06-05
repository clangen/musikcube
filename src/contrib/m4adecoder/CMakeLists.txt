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
target_link_libraries(m4adecoder ${musikbox_LINK_LIBS} faad)
