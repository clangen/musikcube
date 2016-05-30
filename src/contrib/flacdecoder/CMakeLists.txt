set (flacdecoder_SOURCES
  stdafx.cpp
  flacdecoder_plugin.cpp
  FlacDecoderFactory.cpp
  FlacDecoder.cpp
)

add_library(flacdecoder SHARED ${flacdecoder_SOURCES})
target_link_libraries(flacdecoder ${musikbox_LINK_LIBS} FLAC)
