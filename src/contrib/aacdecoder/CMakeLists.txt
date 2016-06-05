set (aacdecoder_SOURCES
  aacdecoder_plugin.cpp
  AacDecoderFactory.cpp
  AacDecoder.cpp
  stdafx.cpp
  drms/drms.c
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

add_library(aacdecoder SHARED ${aacdecoder_SOURCES})
target_link_libraries(aacdecoder ${musikbox_LINK_LIBS} faad)
