set (mpris_SOURCES
  mpris.cpp
  dbus.cpp)

if (USE_ELOGIND MATCHES "true")
  ensure_library_exists(elogind >= 239.3)
  set(SDBUS "libelogind >= 239.3")

elseif (USE_BASU MATCHES "true")
  ensure_library_exists(basu)
  set(SDBUS "basu")

else()
  ensure_library_exists(systemd)
  set(SDBUS "libsystemd")
endif()

message(STATUS "[mpris] plugin enabled")

find_package(PkgConfig)
pkg_check_modules(SDBUS REQUIRED IMPORTED_TARGET ${SDBUS})

add_library(mpris SHARED ${mpris_SOURCES})
target_link_libraries(mpris ${musikcube_LINK_LIBS} PkgConfig::SDBUS)
