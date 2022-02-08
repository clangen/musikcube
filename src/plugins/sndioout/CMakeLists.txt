set (sndioout_SOURCES
  plugin.cpp
  SndioOut.cpp
)

add_library(sndioout SHARED ${sndioout_SOURCES})

find_library(LIBSNDIO sndio)
target_link_libraries(sndioout ${LIBSNDIO})
