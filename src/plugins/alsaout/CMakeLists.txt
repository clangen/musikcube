set (alsaout_SOURCES
  alsaout_plugin.cpp
  AlsaOut.cpp
)

add_library(alsaout SHARED ${alsaout_SOURCES})

find_library(LIBASOUND asound)
target_link_libraries(alsaout ${LIBASOUND})

