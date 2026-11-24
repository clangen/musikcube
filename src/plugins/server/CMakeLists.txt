set (server_SOURCES
  BlockingTranscoder.cpp
  HttpServer.cpp
  main.cpp
  ServerControls.cpp
  ServerMedia.cpp
  Snapshots.cpp
  Transcoder.cpp
  TranscodingAudioDataStream.cpp
  Util.cpp
  WebSocketServer.cpp)

add_library(server SHARED ${server_SOURCES})
musikcube_configure_c_plugin(server)

target_include_directories(server BEFORE PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/include")
target_include_directories(server BEFORE PRIVATE ${VENDOR_INCLUDE_DIRECTORIES})
target_include_directories(server PRIVATE "${PROJECT_SOURCE_DIR}/include")

if (${BUILD_STANDALONE} MATCHES "true")
  find_vendor_library(LIBMICROHTTPD microhttpd)
else()
  if (APPLE)
    find_library(LIBGNUTLS NAMES gnutls)
    set(EXTRA_LIBS "${LIBGNUTLS}")
  endif()
  find_library_and_header(LIBMICROHTTPD microhttpd microhttpd.h)
endif()

find_library(LIBZ NAMES z)
message(STATUS "[server] using " ${LIBMICROHTTPD} ", " ${LIBZ})

target_link_libraries(server musikcore ${LIBZ} ${LIBMICROHTTPD} ${EXTRA_LIBS})
