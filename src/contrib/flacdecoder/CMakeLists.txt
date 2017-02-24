set (flacdecoder_SOURCES
  stdafx.cpp
  flacdecoder_plugin.cpp
  FlacDecoderFactory.cpp
  FlacDecoder.cpp
)

add_library(flacdecoder SHARED ${flacdecoder_SOURCES})

# prefer static libraries on mac to make redist easier
if (${LINK_STATICALLY} MATCHES "true")
  find_library(FLACLIB NAMES libFLAC.a FLAC)
  target_link_libraries(flacdecoder ${musikbox_LINK_LIBS} ${FLACLIB})
else()
  target_link_libraries(flacdecoder ${musikbox_LINK_LIBS} FLAC) 
endif()
