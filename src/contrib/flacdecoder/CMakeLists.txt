set (flacdecoder_SOURCES
  stdafx.cpp
  flacdecoder_plugin.cpp
  FlacDecoderFactory.cpp
  FLACDecoder.cpp
)

add_definitions(
  -D_DEBUG
)

add_library(flacdecoder SHARED ${flacdecoder_SOURCES})
target_link_libraries(flacdecoder ${musikbox_LINK_LIBS} flac)
