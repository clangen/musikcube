set (alsaout_SOURCES
  alsaout_plugin.cpp
  AlsaOut.cpp
)

ensure_library_exists(asound)

add_library(alsaout SHARED ${alsaout_SOURCES})
target_link_libraries(alsaout asound)

