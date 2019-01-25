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

set(EXTRA_LIBS "")
if (CMAKE_SYSTEM_NAME MATCHES "Darwin")
  set(EXTRA_LIBS "gnutls")
endif()

if (${LINK_STATICALLY} MATCHES "true")
  # prefer static libraries on mac to make redist easier
  find_library(MICROHTTPDLIB NAMES libmicrohttpd.a microhttpd)
  target_link_libraries(server ${server_LINK_LIBS} ${MICROHTTPDLIB} z ${EXTRA_LIBS})
else()
  target_link_libraries(server ${server_LINK_LIBS} microhttpd z ${EXTRA_LIBS})
endif()
