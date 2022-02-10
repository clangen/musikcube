if (CMAKE_SYSTEM_NAME MATCHES "Linux")
  find_library(LIBDL NAMES dl)
  list(APPEND DEFAULT_OS_SYSTEM_LIBS ${LIBDL})
endif()

