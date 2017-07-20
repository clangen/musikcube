set (nomaddecoder_SOURCES
  stdafx.cpp
  nomaddecoder_plugin.cpp
  NomadDecoderFactory.cpp
  NomadDecoder.cpp
  mad/bit.c
  mad/decoder.c
  mad/fixed.c
  mad/frame.c
  mad/huffman.c
  mad/layer3.c
  mad/layer12.c
  mad/stream.c
  mad/synth.c
  mad/timer.c
  mad/version.c
  nomad/id3.c
  nomad/misc.c
  nomad/nomad.c
)

include_directories (
  "${CMAKE_CURRENT_SOURCE_DIR}/mad"
  "${CMAKE_CURRENT_SOURCE_DIR}/nomad"
)

add_definitions(-DFPM_DEFAULT)
add_definitions(-DASO_ZEROCHECK)
add_definitions(-DHAVE_CONFIG_H)

add_library(nomaddecoder SHARED ${nomaddecoder_SOURCES})

target_link_libraries(nomaddecoder ${musikcube_LINK_LIBS})
