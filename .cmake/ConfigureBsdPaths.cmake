if (CMAKE_SYSTEM_NAME MATCHES "Darwin" OR CMAKE_SYSTEM_NAME MATCHES "FreeBSD" OR CMAKE_SYSTEM_NAME MATCHES "OpenBSD")
  set(BSD_PATH_PREFIX "/usr/local")
  if (CMAKE_SYSTEM_NAME MATCHES "Darwin")
    execute_process(
      COMMAND brew config
      COMMAND grep -i HOMEBREW_PREFIX
      COMMAND awk "{print $2}"
      OUTPUT_STRIP_TRAILING_WHITESPACE
      OUTPUT_VARIABLE BSD_PATH_PREFIX)
  endif()
  message(STATUS "[configure-bsd-paths] resolved BSD_PATH_PREFIX to: '${BSD_PATH_PREFIX}'")
  list(
    APPEND
    VENDOR_LINK_DIRECTORIES
    "${BSD_PATH_PREFIX}/lib"
    "${BSD_PATH_PREFIX}/opt/openssl/lib"
    "${BSD_PATH_PREFIX}/opt/ncurses/lib")
  include_directories("${BSD_PATH_PREFIX}/include")
  include_directories("${BSD_PATH_PREFIX}/opt/openssl/include")
  include_directories("${BSD_PATH_PREFIX}/opt/ncurses/include")
endif ()