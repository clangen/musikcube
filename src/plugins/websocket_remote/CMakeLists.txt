set (websocket_remote_SOURCES
  HttpServer.cpp
  main.cpp
  Transcoder.cpp
  TranscodingDataStream.cpp
  Util.cpp
  WebSocketServer.cpp
)

set (BOOST_LIBS
  system
  filesystem
  thread
)

find_package(Boost 1.55.0 REQUIRED ${BOOST_LIBS})

add_definitions (-DHAVE_BOOST -D_FILE_OFFSET_BITS=64)
set (BOOST_LINK_LIBS ${Boost_LIBRARIES})

#message(STATUS "boost libs: " ${BOOST_LINK_LIBS})
#message(STATUS "boost includes: " ${Boost_INCLUDE_DIRS})

set (websocket_remote_LINK_LIBS ${BOOST_LINK_LIBS})

include_directories (
    "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/include"
)

add_library(websocket_remote SHARED ${websocket_remote_SOURCES})

# prefer static libraries on mac to make redist easier
if (${LINK_STATICALLY} MATCHES "true")
  find_library(MICROHTTPDLIB NAMES libmicrohttpd.a microhttpd)
  find_library(MP3LAMELIB NAMES libmp3lame.a mp3lame)
  target_link_libraries(websocket_remote ${websocket_remote_LINK_LIBS} ${MICROHTTPDLIB} ${MP3LAMELIB} z)
else()
  target_link_libraries(websocket_remote ${websocket_remote_LINK_LIBS} microhttpd mp3lame z)
endif()
