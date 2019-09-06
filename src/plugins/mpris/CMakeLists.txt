set (mpris_SOURCES
  mpris.cpp
  dbus.cpp)

find_package(PkgConfig)
pkg_check_modules (SYSTEMD REQUIRED libsystemd)

add_library(mpris SHARED ${mpris_SOURCES})
target_link_libraries(mpris ${musikcube_LINK_LIBS} systemd)
