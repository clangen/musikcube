set (pipewireout_SOURCES
  pipewireout_plugin.cpp
  PipeWireOut.cpp
)

ensure_library_exists(pipewire-0.3)

message(STATUS "[pipewireout] plugin enabled")

find_package(PkgConfig)
pkg_check_modules(PIPEWIRE REQUIRED IMPORTED_TARGET libpipewire-0.3)

add_library(pipewireout SHARED ${pipewireout_SOURCES})
target_link_libraries(pipewireout ${musikcube_LINK_LIBS} PkgConfig::PIPEWIRE)
