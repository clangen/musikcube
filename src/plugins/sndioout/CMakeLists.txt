set (sndioout_SOURCES
  plugin.cpp
  SndioOut.cpp
)

add_library(sndioout SHARED ${sndioout_SOURCES})
target_link_libraries(sndioout ${musikcube_LINK_LIBS} sndio)
