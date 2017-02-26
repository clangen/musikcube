set (websocket_remote_SOURCES
  main.cpp
)

set (BOOST_LIBS
  system
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
target_link_libraries(websocket_remote ${websocket_remote_LINK_LIBS} Z)
