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

ensure_library_exists(microhttpd)

# if (${LINK_STATICALLY} MATCHES "true")
#   find_library(MICROHTTPDLIB NAMES libmicrohttpd.a)
#   target_link_libraries(server ${server_LINK_LIBS} ${MICROHTTPDLIB})
# else()
  set(EXTRA_LIBS "")
  if (CMAKE_SYSTEM_NAME MATCHES "Darwin")
    set(EXTRA_LIBS "gnutls")
  endif()
  target_link_libraries(server ${server_LINK_LIBS} microhttpd z ${EXTRA_LIBS})
# endif()
