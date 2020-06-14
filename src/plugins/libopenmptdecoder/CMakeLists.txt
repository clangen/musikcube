set (openmptdecoder_SOURCES
  plugin.cpp
  OpenMptDecoder.cpp
)

add_library(openmptdecoder SHARED ${openmptdecoder_SOURCES})
target_link_libraries(openmptdecoder ${musikcube_LINK_LIBS} openmpt)
