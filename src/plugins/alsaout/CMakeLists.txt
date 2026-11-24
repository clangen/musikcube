set (alsaout_SOURCES
  alsaout_plugin.cpp
  AlsaOut.cpp
)

find_library(LIBASOUND asound)

if ("${LIBASOUND}" STREQUAL "LIBASOUND-NOTFOUND")
  disable_plugin(alsaout)
else()
  add_library(alsaout SHARED ${alsaout_SOURCES})
  musikcube_configure_c_plugin(alsaout)
  target_compile_features(alsaout PRIVATE cxx_std_20)
  target_link_libraries(alsaout ${LIBASOUND})
endif()
