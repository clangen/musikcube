set (libopenmptdecoder_SOURCES
  plugin.cpp
  OpenMptDecoder.cpp
)

add_library(libopenmptdecoder SHARED ${libopenmptdecoder_SOURCES})
target_link_libraries(libopenmptdecoder ${musikcube_LINK_LIBS})
