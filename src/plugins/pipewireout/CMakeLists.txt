set (pipewireout_SOURCES
  pipewireout_plugin.cpp
  PipeWireOut.cpp
)

message(STATUS "[pipewireout] plugin enabled")

add_library(pipewireout SHARED ${pipewireout_SOURCES})
target_link_libraries(pipewireout ${musikcube_LINK_LIBS} pipewire-0.3)
