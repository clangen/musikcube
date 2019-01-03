set (gmedecoder_SOURCES
  plugin.cpp
  GmeDecoder.cpp
)

add_library(gmedecoder SHARED ${gmedecoder_SOURCES})
target_link_libraries(gmedecoder ${musikcube_LINK_LIBS})
