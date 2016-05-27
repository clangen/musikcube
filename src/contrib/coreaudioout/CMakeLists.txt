set (coreaudioout_SOURCES
  coreaudioout_plugin.cpp
  CoreAudioOut.cpp
)

add_definitions(
  -D_DEBUG
)

add_library(coreaudioout SHARED ${coreaudioout_SOURCES})
target_link_libraries(coreaudioout ${musikbox_LINK_LIBS})
