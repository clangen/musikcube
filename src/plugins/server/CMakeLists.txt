set (server_SOURCES
  BlockingTranscoder.cpp
  HttpServer.cpp
  main.cpp
  Snapshots.cpp
  Transcoder.cpp
  TranscodingAudioDataStream.cpp
  Util.cpp
  WebSocketServer.cpp)

add_library(server SHARED ${server_SOURCES})

set (server_LINK_LIBS ${Boost_LIBRARIES})

include_directories ("${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/include")
include_directories(${Boost_INCLUDE_DIRS})

if (${LINK_STATICALLY} MATCHES "true")
  # unclear why NO_DEFAULT_PATH NO_CACHE is required; we want to use our vendor'd version,
  # but without NO_CACHE we always get the brew one. other deps are fine.
  find_library(LIBMICROHTTPD NAMES libmicrohttpd.a PATHS ${VENDOR_LINK_DIRECTORIES} NO_DEFAULT_PATH NO_CACHE)
  find_library(LIBZ NAMES libz.a PATHS ${VENDOR_LINK_DIRECTORIES})
else()
  if (APPLE)
    find_library(LIBGNUTLS NAMES gnuatls)
    set(EXTRA_LIBS "${LIBGNUTLS}")
  endif()
  find_library(LIBMICROHTTPD NAMES microhttpd)
  find_library(LIBZ NAMES z)
endif()

message(STATUS "[server] using " ${LIBMICROHTTPD} ", " ${LIBZ})

target_link_libraries(server ${server_LINK_LIBS} ${LIBZ} ${LIBMICROHTTPD} ${EXTRA_LIBS})
