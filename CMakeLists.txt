cmake_minimum_required(VERSION 3.0)

project(musikbox)

set (musikbox_VERSION_MAJOR 0)
set (musikbox_VERSION_MINOR 1)

include(ExternalProject)

ExternalProject_Add(taglib
    SOURCE_DIR "${musikbox_SOURCE_DIR}/src/contrib/taglib_plugin/taglib-1.11"
    BUILD_IN_SOURCE 0
    DOWNLOAD_COMMAND ""
    UPDATE_COMMAND ""
    CMAKE_ARGS "-DCMAKE_INSTALL_PREFIX:PATH=${musikbox_SOURCE_DIR}/src/contrib/taglib_plugin/taglib-1.11/stage/"
    INSTALL_COMMAND make install
)

set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} "-std=c++11 -fpermissive -g")

set (BOOST_LIBS
  system
  date_time
  chrono
  filesystem
  iostreams
  thread
  regex
)

if (CMAKE_SYSTEM_NAME MATCHES "Linux")
  find_package(Boost 1.58.0 REQUIRED ${BOOST_LIBS})
else (CMAKE_SYSTEM_NAME MATCHES "Linux")
  find_package(Boost 1.60.0 REQUIRED ${BOOST_LIBS})
endif (CMAKE_SYSTEM_NAME MATCHES "Linux")

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

link_directories (
  ${musikbox_SOURCE_DIR}/bin/plugins
  ${musikbox_SOURCE_DIR}/src/contrib/taglib_plugin/taglib-1.11/stage/lib
)

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
  "${musikbox_SOURCE_DIR}/src/contrib/taglib_plugin/taglib-1.11/stage/include"
)

add_subdirectory(src/core)
add_subdirectory(src/musikbox)
add_subdirectory(src/contrib/taglib_plugin)
add_subdirectory(src/contrib/m4adecoder)
add_subdirectory(src/contrib/oggdecoder)
add_subdirectory(src/contrib/mpg123decoder)
add_subdirectory(src/contrib/flacdecoder)

if (CMAKE_SYSTEM_NAME MATCHES "Linux")
  add_subdirectory(src/contrib/alsaout)
else (CMAKE_SYSTEM_NAME MATCHES "Linux")
  add_subdirectory(src/contrib/coreaudioout)
endif (CMAKE_SYSTEM_NAME MATCHES "Linux")
