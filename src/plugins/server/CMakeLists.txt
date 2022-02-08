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

target_include_directories(
  server
  BEFORE PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/include"
  ${VENDOR_INCLUDE_DIRECTORIES})

if (${BUILD_STANDALONE} MATCHES "true")
  find_library(LIBMICROHTTPD NAMES libmicrohttpd.a PATHS ${VENDOR_LINK_DIRECTORIES} NO_DEFAULT_PATH NO_CACHE)
else()
  if (APPLE)
    find_library(LIBGNUTLS NAMES gnuatls)
    set(EXTRA_LIBS "${LIBGNUTLS}")
  endif()
  find_library(LIBMICROHTTPD NAMES microhttpd)
endif()

find_library(LIBZ NAMES z)
message(STATUS "[server] using " ${LIBMICROHTTPD} ", " ${LIBZ})

target_link_libraries(server ${LIBZ} ${LIBMICROHTTPD} ${EXTRA_LIBS})
