set (mpris_SOURCES
  mpris.cpp
  dbus.cpp
  metadata.cpp
  playback.cpp
  plugin.cpp)

if (USE_ELOGIND)
  set(SDBUS "libelogind >= 239.3")
  set(SDBUS_HEADER "<elogind/sd-bus.h>")
elseif (USE_BASU)
  set(SDBUS "basu")
  set(SDBUS_HEADER "<basu/sd-bus.h>")
else()
  set(SDBUS "libsystemd")
  set(SDBUS_HEADER "<systemd/sd-bus.h>")
endif()

find_package(PkgConfig)
pkg_check_modules(SDBUS IMPORTED_TARGET ${SDBUS})

if (NOT DEFINED SDBUS_LINK_LIBRARIES)
  disable_plugin(mpris)
else()
  message(STATUS "[mpris] plugin enabled. using " ${SDBUS_LINK_LIBRARIES})
  add_library(mpris SHARED ${mpris_SOURCES})
  musikcube_configure_c_plugin(mpris)
  target_compile_features(mpris PRIVATE cxx_std_20)
  target_compile_definitions(mpris PUBLIC SDBUS_HEADER=${SDBUS_HEADER})
  target_link_libraries(mpris musikcore PkgConfig::SDBUS Threads::Threads)
endif()
