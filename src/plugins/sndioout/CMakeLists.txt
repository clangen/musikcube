set (sndioout_SOURCES
  plugin.cpp
  SndioOut.cpp
)

find_library(LIBSNDIO sndio)

if ("${LIBSNDIO}" STREQUAL "LIBSNDIO-NOTFOUND")
  disable_plugin(sndioout)
else()
  add_library(sndioout SHARED ${sndioout_SOURCES})
  musikcube_configure_c_plugin(sndioout)
  target_compile_features(sndioout PRIVATE cxx_std_20)
  target_link_libraries(sndioout ${LIBSNDIO})
endif()
