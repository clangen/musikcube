set (sndioout_SOURCES
  plugin.cpp
  SndioOut.cpp
)

find_library(LIBSNDIO sndio)

if ("${LIBSNDIO}" STREQUAL "LIBSNDIO-NOTFOUND")
  disable_plugin(sndioout)
else()
  include(CheckSymbolExists)
  include(CMakePushCheckState)
  cmake_push_check_state(RESET)
  set(CMAKE_REQUIRED_LIBRARIES "${LIBSNDIO}")
  check_symbol_exists(sio_flush "sndio.h" MUSIKCUBE_SNDIO_HAS_FLUSH)
  cmake_pop_check_state()
  add_library(sndioout SHARED ${sndioout_SOURCES})
  musikcube_configure_c_plugin(sndioout)
  target_compile_features(sndioout PRIVATE cxx_std_20)
  target_compile_definitions(sndioout PRIVATE
    MUSIKCUBE_SNDIO_HAS_FLUSH=$<BOOL:${MUSIKCUBE_SNDIO_HAS_FLUSH}>)
  target_link_libraries(sndioout ${LIBSNDIO})
endif()
