cmake_minimum_required(VERSION 3.0)

project(musikbox)

set (musikbox_VERSION_MAJOR 0)
set (musikbox_VERSION_MINOR 1)

set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} "-std=c++11 -fpermissive -g")

set (BOOST_LIBS
  system
  date_time
  filesystem
  iostreams
  thread
  regex
)

find_package(Boost 1.60.0 REQUIRED ${BOOST_LIBS})

add_definitions (-DHAVE_BOOST
  -D_FILE_OFFSET_BITS=64)
set (BOOST_LINK_LIBS
  ${Boost_LIBRARIES})

#message(STATUS "boost libs: " ${BOOST_LINK_LIBS})
#message(STATUS "boost includes: " ${Boost_INCLUDE_DIRS})

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/Modules)

SET (CMAKE_BUILD_TYPE DEBUG)
set (LIBRARY_OUTPUT_PATH ${musikbox_SOURCE_DIR}/bin/plugins)
set (EXECUTABLE_OUTPUT_PATH ${musikbox_SOURCE_DIR}/bin)

link_directories (${musikbox_SOURCE_DIR}/bin/plugins)

set (musikbox_LINK_LIBS
  ${linux_LINK_LIBS}
  ${BOOST_LINK_LIBS}
  dl
)

include_directories (
  "${musikbox_SOURCE_DIR}/src"
  "${musikbox_SOURCE_DIR}/src/core"
  "${musikbox_SOURCE_DIR}/src/musikbox"
  "${musikbox_SOURCE_DIR}/src/3rdparty/include"
)

add_subdirectory(src/core)
add_subdirectory(src/musikbox)
add_subdirectory(src/contrib/taglib_plugin)
add_subdirectory(src/contrib/m4adecoder)
add_subdirectory(src/contrib/oggdecoder)
add_subdirectory(src/contrib/mpg123decoder)
add_subdirectory(src/contrib/flacdecoder)
#add_subdirectory(src/contrib/coreaudioout)
add_subdirectory(src/contrib/pulseout)
