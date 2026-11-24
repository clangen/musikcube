set (pipewireout_SOURCES
  pipewireout_plugin.cpp
  PipeWireOut.cpp
)

if (${ENABLE_PIPEWIRE} MATCHES "false")
  disable_plugin(pipewireout)
else()
  find_package(PkgConfig)
  pkg_check_modules(PIPEWIRE IMPORTED_TARGET libpipewire-0.3)
  if (NOT DEFINED PIPEWIRE_LINK_LIBRARIES)
    disable_plugin(pipewireout)
  else()
    add_library(pipewireout SHARED ${pipewireout_SOURCES})
    musikcube_configure_c_plugin(pipewireout)
    target_compile_features(pipewireout PRIVATE cxx_std_20)
    target_link_libraries(pipewireout PkgConfig::PIPEWIRE)
  endif()
endif()

