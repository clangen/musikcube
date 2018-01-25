set (server_SOURCES
  HttpServer.cpp
  main.cpp
  Snapshots.cpp
  Transcoder.cpp
  TranscodingDataStream.cpp
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


if (CMAKE_SYSTEM_NAME MATCHES "FreeBSD")
  # the version of libmicrohttpd that ships with FreeBSD hangs during shutdown,
  # but newer versions is fine. snag the sources and compile them on-demand.
  include (ExternalProject)

  ExternalProject_Add(libmicrohttpd
    URL https://ftp.gnu.org/gnu/libmicrohttpd/libmicrohttpd-0.9.58.tar.gz
    BUILD_IN_SOURCE 1
    CONFIGURE_COMMAND ./configure --enable-https=no --disable-curl --prefix=${CMAKE_CURRENT_SOURCE_DIR}/microhttpd/
    BUILD_COMMAND make
    INSTALL_COMMAND make install
    TEST_COMMAND "")

  add_dependencies(server libmicrohttpd)

  include_directories ("${CMAKE_CURRENT_SOURCE_DIR}/microhttpd/include")
  file(GLOB OBJS "${CMAKE_CURRENT_SOURCE_DIR}/libmicrohttpd-prefix/src/libmicrohttpd/src/microhttpd/.libs/*.o")

  target_link_libraries(server ${server_LINK_LIBS} "${OBJS}" z)
elseif (${LINK_STATICALLY} MATCHES "true")
  # prefer static libraries on mac to make redist easier
  find_library(MICROHTTPDLIB NAMES libmicrohttpd.a microhttpd)
  target_link_libraries(server ${server_LINK_LIBS} ${MICROHTTPDLIB} z)
else()
  target_link_libraries(server ${server_LINK_LIBS} microhttpd z)
endif()
