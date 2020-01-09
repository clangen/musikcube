set (server_SOURCES
  BlockingTranscoder.cpp
  HttpServer.cpp
  main.cpp
  Snapshots.cpp
  Transcoder.cpp
  TranscodingAudioDataStream.cpp
  Util.cpp
  WebSocketServer.cpp)

set (BOOST_LIBS
  system
  filesystem
  thread)

find_package(Boost 1.55.0 REQUIRED ${BOOST_LIBS})
add_definitions (-DHAVE_BOOST -D_FILE_OFFSET_BITS=64)
set (BOOST_LINK_LIBS ${Boost_LIBRARIES})

add_library(server SHARED ${server_SOURCES})

set (server_LINK_LIBS ${BOOST_LINK_LIBS})

include_directories ("${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/include")

if (${LINK_STATICALLY} MATCHES "true")
  # libmicrohttpd on macOS now depends on `gnutls`. when we build statically,
  # we also need to build libmicrohttpd ourselves and disable TLS to avoid this
  # homebrew-only dependency
  if (CMAKE_SYSTEM_NAME MATCHES "Darwin")
    include (ExternalProject)

    ExternalProject_Add(libmicrohttpd
      URL https://ftp.gnu.org/gnu/libmicrohttpd/libmicrohttpd-0.9.62.tar.gz
      BUILD_IN_SOURCE 1
      CONFIGURE_COMMAND ./configure --enable-https=no --disable-curl --prefix=${CMAKE_CURRENT_SOURCE_DIR}/microhttpd/
      BUILD_COMMAND make
      INSTALL_COMMAND make install
      TEST_COMMAND "")

    add_dependencies(server libmicrohttpd)
    include_directories ("${CMAKE_CURRENT_SOURCE_DIR}/microhttpd/include")
    file(GLOB EXTRA_OBJS "${CMAKE_CURRENT_SOURCE_DIR}/libmicrohttpd-prefix/src/libmicrohttpd/src/microhttpd/*.o")
    target_link_libraries(server ${server_LINK_LIBS} z crypto ${EXTRA_OBJS})
  else()
    find_library(MICROHTTPDLIB NAMES libmicrohttpd.a microhttpd)
    target_link_libraries(server ${server_LINK_LIBS} ${MICROHTTPDLIB} z)
  endif()
else()
  set(EXTRA_LIBS "")
  if (CMAKE_SYSTEM_NAME MATCHES "Darwin")
    set(EXTRA_LIBS "gnutls")
  endif()
  target_link_libraries(server ${server_LINK_LIBS} microhttpd z ${EXTRA_LIBS})
endif()
