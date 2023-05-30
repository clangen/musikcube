set (mpris_SOURCES
  mpris.cpp
  dbus.cpp)

if (USE_ELOGIND)
  set(SDBUS "libelogind >= 239.3")
elseif (USE_BASU)
  set(SDBUS "basu")
else()
  set(SDBUS "libsystemd")
endif()

find_package(PkgConfig)
pkg_check_modules(SDBUS IMPORTED_TARGET ${SDBUS})

if (NOT DEFINED SDBUS_LINK_LIBRARIES)
  disable_plugin(mpris)
else()
  message(STATUS "[mpris] plugin enabled. using " ${SDBUS_LINK_LIBRARIES})
  add_library(mpris SHARED ${mpris_SOURCES})
  target_link_libraries(mpris PkgConfig::SDBUS)
endif()
