set (alsaout_SOURCES
  alsaout_plugin.cpp
  AlsaOut.cpp
)

find_library(LIBASOUND asound)

if ("${LIBASOUND}" STREQUAL "LIBASOUND-NOTFOUND")
  disable_plugin(alsaout)
else()
  add_library(alsaout SHARED ${alsaout_SOURCES})
  target_link_libraries(alsaout ${LIBASOUND})
endif()
