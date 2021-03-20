set (pipewireout_SOURCES
  pipewireout_plugin.cpp
  PipeWireOut.cpp
)

message(STATUS "[pipewireout] plugin enabled")

include_directories("/usr/include/spa-0.2")
include_directories("/usr/local/include/spa-0.2")
include_directories("/usr/include/pipewire-0.3")
include_directories("/usr/local/include/pipewire-0.3")

add_library(pipewireout SHARED ${pipewireout_SOURCES})
target_link_libraries(pipewireout ${musikcube_LINK_LIBS} pipewire-0.3)
