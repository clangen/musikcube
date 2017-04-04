#cmake -DCMAKE_BUILD_TYPE=Release .
#cmake -DCMAKE_BUILD_TYPE=Debug .
#cmake -DGENERATE_DEB=1 -DDEB_ARCHITECTURE=i386|amd64|armhf -DDEB_BOOST_VERSION=1.55.0 -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=Release .
#cmake -DCMAKE_BUILD_TYPE=Release -DLINK_STATICALLY=true .

cmake_minimum_required(VERSION 3.0)

project(musikbox)
set (musikbox_VERSION_MAJOR 0)
set (musikbox_VERSION_MINOR 11)
set (musikbox_VERSION_PATCH 0)
set (musikbox_VERSION "${musikbox_VERSION_MAJOR}.${musikbox_VERSION_MINOR}.${musikbox_VERSION_PATCH}")

include(ExternalProject)

ExternalProject_Add(taglib
    SOURCE_DIR "${musikbox_SOURCE_DIR}/src/contrib/taglib_plugin/taglib-1.11"
    BUILD_IN_SOURCE 0
    DOWNLOAD_COMMAND ""
    UPDATE_COMMAND ""
    CMAKE_ARGS "-DCMAKE_INSTALL_PREFIX:PATH=${musikbox_SOURCE_DIR}/src/contrib/taglib_plugin/taglib-1.11/stage/"
    INSTALL_COMMAND make install
)

set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} "-std=c++11 -Wno-unused-result -Wno-deprecated-declarations")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O2")

if (${LINK_STATICALLY} MATCHES "true")
  set(Boost_USE_STATIC_LIBS ON)
endif()

set (BOOST_LIBS
  system
  filesystem
  thread
)

find_package(Boost 1.55.0 REQUIRED ${BOOST_LIBS})

add_definitions (-DHAVE_BOOST -D_FILE_OFFSET_BITS=64 -DSIGSLOT_USE_POSIX_THREADS)
set (BOOST_LINK_LIBS ${Boost_LIBRARIES})

#message(STATUS "boost libs: " ${BOOST_LINK_LIBS})
#message(STATUS "boost includes: " ${Boost_INCLUDE_DIRS})

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/Modules)

# SET (CMAKE_BUILD_TYPE DEBUG)
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

# "/usr/local" doesn't seem to be included by default on macOS 10.12+
if (CMAKE_SYSTEM_NAME MATCHES "Darwin")
  link_directories ("/usr/local/lib")
  include_directories("/usr/local/include")
endif ()

add_subdirectory(src/core)
add_subdirectory(src/glue)
add_subdirectory(src/musikbox)
add_subdirectory(src/contrib/taglib_plugin)
add_subdirectory(src/contrib/m4adecoder)
add_subdirectory(src/contrib/oggdecoder)
add_subdirectory(src/contrib/nomaddecoder)
add_subdirectory(src/contrib/flacdecoder)
add_subdirectory(src/contrib/nullout)
add_subdirectory(src/contrib/websocket_remote)

add_dependencies(taglibreader taglib)

if (CMAKE_SYSTEM_NAME MATCHES "Linux")
  add_subdirectory(src/contrib/alsaout)
  add_subdirectory(src/contrib/pulseout)
else (CMAKE_SYSTEM_NAME MATCHES "Linux")
  # macos
  add_subdirectory(src/contrib/coreaudioout)
endif (CMAKE_SYSTEM_NAME MATCHES "Linux")

# install the binary
install(
  FILES bin/musikbox DESTINATION share/musikcube
  PERMISSIONS
    OWNER_EXECUTE OWNER_READ OWNER_WRITE
    GROUP_EXECUTE GROUP_READ GROUP_WRITE
    WORLD_EXECUTE WORLD_READ
)

# install the plugins
if (CMAKE_SYSTEM_NAME MATCHES "Linux")
  # linux
  install(
    FILES
      bin/plugins/libalsaout.so
      bin/plugins/libpulseout.so
      bin/plugins/libnullout.so
      bin/plugins/libflacdecoder.so
      bin/plugins/libm4adecoder.so
      bin/plugins/libnomaddecoder.so
      bin/plugins/liboggdecoder.so
      bin/plugins/libtaglibreader.so
      bin/plugins/libwebsocket_remote.so
    DESTINATION share/musikcube/plugins
  )
else (CMAKE_SYSTEM_NAME MATCHES "Linux")
  # macos
  install(
    FILES
      bin/plugins/libcoreaudioout.dylib
      bin/plugins/libnullout.dylib
      bin/plugins/libflacdecoder.dylib
      bin/plugins/libm4adecoder.dylib
      bin/plugins/libnomaddecoder.dylib
      bin/plugins/liboggdecoder.dylib
      bin/plugins/libtaglibreader.dylib
      bin/plugins/libwebsocket_remote.dylib
    DESTINATION share/musikcube/plugins
  )
endif (CMAKE_SYSTEM_NAME MATCHES "Linux")

file(GLOB sdk_headers "src/core/sdk/*.h")
file(GLOB themes "src/musikbox/data/themes/*.json")
file(GLOB locales "src/musikbox/data/locales/*.json")

file(COPY ${themes}
    DESTINATION bin/themes
)

install(
    FILES ${themes}
    DESTINATION share/musikcube/themes
)

file(COPY ${locales}
    DESTINATION bin/locales
)

install(
    FILES ${locales}
    DESTINATION share/musikcube/locales
)

install(
    FILES ${sdk_headers}
    DESTINATION include/musikcube/core/sdk
)

# shell script that starts the app.
install(
  FILES src/musikbox/musikbox
  DESTINATION bin/
  PERMISSIONS
    OWNER_EXECUTE OWNER_READ OWNER_WRITE
    GROUP_EXECUTE GROUP_READ GROUP_WRITE
    WORLD_EXECUTE WORLD_READ
)

if (GENERATE_DEB MATCHES "1")
  if (CMAKE_SYSTEM_NAME MATCHES "Linux")
    set(CPACK_GENERATOR "DEB")
    set(CPACK_PACKAGE_DESCRIPTION "musikbox, a terminal-based music player and library")
    set(CPACK_PACKAGE_VERSION_MAJOR "${musikbox_VERSION_MAJOR}")
    set(CPACK_PACKAGE_VERSION_MINOR "${musikbox_VERSION_MINOR}")
    set(CPACK_PACKAGE_VERSION_PATCH "${musikbox_VERSION_PATCH}")
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "libboost-thread${DEB_BOOST_VERSION}, libboost-system${DEB_BOOST_VERSION}, libboost-filesystem${DEB_BOOST_VERSION}, libboost-date-time${DEB_BOOST_VERSION}, libboost-atomic${DEB_BOOST_VERSION}, libboost-chrono${DEB_BOOST_VERSION}, libogg0, libvorbis0a, libvorbisfile3, libflac8, libfaad2, libncursesw5, libasound2, libpulse0, pulseaudio")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "casey langen")
    set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE ${DEB_ARCHITECTURE})
    set(CPACK_PACKAGE_FILE_NAME "musikbox_${musikbox_VERSION}_${DEB_ARCHITECTURE}")
    include(CPack)
  endif()
endif()
