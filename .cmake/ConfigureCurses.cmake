if(
    NO_NCURSESW OR
    EXISTS "/etc/arch-release" OR
    EXISTS "/etc/manjaro-release" OR
    CMAKE_SYSTEM_NAME MATCHES "FreeBSD" OR
    CMAKE_SYSTEM_NAME MATCHES "OpenBSD" OR
    CMAKE_SYSTEM_NAME MATCHES "Haiku"
)
  add_definitions (-DNO_NCURSESW)
endif()