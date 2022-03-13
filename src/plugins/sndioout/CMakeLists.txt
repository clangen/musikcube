set (sndioout_SOURCES
  plugin.cpp
  SndioOut.cpp
)

find_library(LIBSNDIO sndio)

if ("${LIBSNDIO}" STREQUAL "LIBSNDIO-NOTFOUND")
  disable_plugin(sndioout)
else()
  add_library(sndioout SHARED ${sndioout_SOURCES})
  target_link_libraries(sndioout ${LIBSNDIO})
endif()
