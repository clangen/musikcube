#cmake -DCMAKE_BUILD_TYPE=Release .
#cmake -DCMAKE_BUILD_TYPE=Debug .
#cmake -DGENERATE_DEB=1 -DDEB_ARCHITECTURE=i386|amd64|armhf -DDEB_DISTRO=stretch -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=Release .
#cmake -DCMAKE_BUILD_TYPE=Release -DLINK_STATICALLY=true .

cmake_minimum_required(VERSION 3.0)

project(musikcube)
set (musikcube_VERSION_MAJOR 0)
set (musikcube_VERSION_MINOR 51)
set (musikcube_VERSION_PATCH 0)
set (musikcube_VERSION "${musikcube_VERSION_MAJOR}.${musikcube_VERSION_MINOR}.${musikcube_VERSION_PATCH}")

include(CMakeToolsHelpers OPTIONAL)

include(ExternalProject)

ExternalProject_Add(taglib
    SOURCE_DIR "${musikcube_SOURCE_DIR}/src/plugins/taglib_plugin/taglib-1.11"
    BUILD_IN_SOURCE 0
    DOWNLOAD_COMMAND ""
    UPDATE_COMMAND ""
    CMAKE_ARGS "-DCMAKE_INSTALL_PREFIX:PATH=${musikcube_SOURCE_DIR}/src/plugins/taglib_plugin/taglib-1.11/stage/"
    INSTALL_COMMAND make install)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -Wno-unused-result -Wno-deprecated-declarations")

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g")
# enable for additional memory checking with fsanitize
# set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g3 -fsanitize=address,undefined")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O2")

if (${LINK_STATICALLY} MATCHES "true")
  set(Boost_USE_STATIC_LIBS ON)
endif()

set (BOOST_LIBS system filesystem thread)

find_package(Boost 1.55.0 REQUIRED ${BOOST_LIBS})

add_definitions (-DHAVE_BOOST -D_FILE_OFFSET_BITS=64 -DSIGSLOT_USE_POSIX_THREADS)
set (BOOST_LINK_LIBS ${Boost_LIBRARIES})

#message(STATUS "boost libs: " ${BOOST_LINK_LIBS})
#message(STATUS "boost includes: " ${Boost_INCLUDE_DIRS})

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/Modules)

# SET (CMAKE_BUILD_TYPE DEBUG)
set (LIBRARY_OUTPUT_PATH ${musikcube_SOURCE_DIR}/bin/plugins)
set (EXECUTABLE_OUTPUT_PATH ${musikcube_SOURCE_DIR}/bin)

link_directories (
  ${musikcube_SOURCE_DIR}/bin/plugins
  ${musikcube_SOURCE_DIR}/src/plugins/taglib_plugin/taglib-1.11/stage/lib)

if (CMAKE_SYSTEM_NAME MATCHES "FreeBSD")
  set (musikcube_LINK_LIBS ${BOOST_LINK_LIBS} curl pthread crypto)
else()
  set (musikcube_LINK_LIBS ${BOOST_LINK_LIBS} dl curl pthread crypto)
endif()

include_directories (
  "${musikcube_SOURCE_DIR}/src"
  "${musikcube_SOURCE_DIR}/src/core"
  "${musikcube_SOURCE_DIR}/src/musikcube"
  "${musikcube_SOURCE_DIR}/src/3rdparty/include"
  "${musikcube_SOURCE_DIR}/src/plugins/taglib_plugin/taglib-1.11/stage/include")

# ensure the binaries can find libmusikcore.so, which lives in the
# same directory.
if (NOT CMAKE_SYSTEM_NAME MATCHES "Darwin")
  set(CMAKE_INSTALL_RPATH "$ORIGIN")
  set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
endif()

# "/usr/local" doesn't seem to be included by default on macOS 10.12+
# "/opt/local" is the default installation location for MacPorts
if (CMAKE_SYSTEM_NAME MATCHES "Darwin" OR CMAKE_SYSTEM_NAME MATCHES "FreeBSD")
  link_directories ("/usr/local/lib")
  include_directories("/usr/local/include")
  link_directories ("/opt/local/lib")
  include_directories("/opt/local/include")
  include_directories("/usr/local/opt/openssl/include")
endif ()

if (EXISTS "/etc/arch-release" OR EXISTS "/etc/manjaro-release" OR NO_NCURSESW)
  add_definitions (-DNO_NCURSESW)
elseif(CMAKE_SYSTEM_NAME MATCHES "FreeBSD")
  add_definitions (-DNO_NCURSESW)
endif()

add_subdirectory(src/core)
add_subdirectory(src/musikcube)
add_subdirectory(src/musikcubed)
add_subdirectory(src/plugins/taglib_plugin)
add_subdirectory(src/plugins/nullout)
add_subdirectory(src/plugins/server)
add_subdirectory(src/plugins/httpdatastream)
add_subdirectory(src/plugins/stockencoders)

if (${FFMPEG_DECODER} MATCHES "true")
  add_subdirectory(src/plugins/ffmpegdecoder)
  add_dependencies(musikcube ffmpegdecoder)
else()
  add_subdirectory(src/plugins/m4adecoder)
  add_subdirectory(src/plugins/oggdecoder)
  add_subdirectory(src/plugins/nomaddecoder)
  add_subdirectory(src/plugins/flacdecoder)
  add_dependencies(musikcube m4adecoder oggdecoder nomaddecoder flacdecoder)
endif()

add_dependencies(taglibreader taglib)

add_dependencies(musikcube musikcore taglibreader nullout server httpdatastream stockencoders)
add_dependencies(musikcubed musikcube)

if (CMAKE_SYSTEM_NAME MATCHES "Linux")
  add_subdirectory(src/plugins/alsaout)
  add_subdirectory(src/plugins/pulseout)
  add_dependencies(musikcube alsaout pulseout)
  if (${ENABLE_SNDIO} MATCHES "true")
    add_subdirectory(src/plugins/sndioout)
    add_dependencies(musikcube sndioout)
  endif()
elseif (CMAKE_SYSTEM_NAME MATCHES "FreeBSD")
  add_subdirectory(src/plugins/sndioout)
  add_dependencies(musikcube sndioout)
  if (${ENABLE_PULSEAUDIO} MATCHES "true")
    add_subdirectory(src/plugins/pulseout) # disabled by default
    add_dependencies(musikcube pulseout)
  endif()
elseif (CMAKE_SYSTEM_NAME MATCHES "Darwin")
  add_subdirectory(src/plugins/coreaudioout)
  add_dependencies(musikcube coreaudioout)
endif()

if (CMAKE_SYSTEM_NAME MATCHES "Darwin")
  file(GLOB plugins "bin/plugins/*.dylib")
  install(FILES ${plugins} DESTINATION share/musikcube/plugins)
else ()
  file(GLOB plugins "bin/plugins/*.so")
  install(FILES ${plugins} DESTINATION share/musikcube/plugins)
endif ()

file(GLOB sdk_headers "src/core/sdk/*.h")
install(FILES ${sdk_headers} DESTINATION include/musikcube/core/sdk)

file(GLOB themes "src/musikcube/data/themes/*.json")
file(COPY ${themes} DESTINATION bin/themes)
install(FILES ${themes} DESTINATION share/musikcube/themes)

file(GLOB locales "src/musikcube/data/locales/*.json")
file(COPY ${locales} DESTINATION bin/locales)
install(FILES ${locales} DESTINATION share/musikcube/locales)

# libmusikcore sshared library
if (CMAKE_SYSTEM_NAME MATCHES "Darwin")
  install(FILES "bin/libmusikcore.dylib" DESTINATION share/musikcube)
else ()
  install(FILES "bin/libmusikcore.so" DESTINATION share/musikcube)
endif ()

# executable and shell script for musikcube
install(
  FILES bin/musikcube DESTINATION share/musikcube
  PERMISSIONS
    OWNER_EXECUTE OWNER_READ OWNER_WRITE
    GROUP_EXECUTE GROUP_READ GROUP_WRITE
    WORLD_EXECUTE WORLD_READ)

install(
  FILES src/musikcube/musikcube
  DESTINATION bin/
  PERMISSIONS
    OWNER_EXECUTE OWNER_READ OWNER_WRITE
    GROUP_EXECUTE GROUP_READ GROUP_WRITE
    WORLD_EXECUTE WORLD_READ)

# executable and shell script for daemon
install(
  FILES bin/musikcubed DESTINATION share/musikcube
  PERMISSIONS
    OWNER_EXECUTE OWNER_READ OWNER_WRITE
    GROUP_EXECUTE GROUP_READ GROUP_WRITE
    WORLD_EXECUTE WORLD_READ)

install(
  FILES src/musikcubed/musikcubed
  DESTINATION bin/
  PERMISSIONS
    OWNER_EXECUTE OWNER_READ OWNER_WRITE
    GROUP_EXECUTE GROUP_READ GROUP_WRITE
    WORLD_EXECUTE WORLD_READ)

# deb generation
if (GENERATE_DEB MATCHES "1")
  if (CMAKE_SYSTEM_NAME MATCHES "Linux")
    set(DEB_BOOST_VERSION "1.55.0")
    set(DEB_MICROHTTPD_VERSION "10")
    set(DEB_LIBCURL_VERSION "3")
    if (DEB_DISTRO MATCHES "stretch")
      set(DEB_BOOST_VERSION "1.62.0")
      set(DEB_MICROHTTPD_VERSION "12")
    elseif (DEB_DISTRO MATCHES "zesty")
      set(DEB_BOOST_VERSION "1.62.0")
      set(DEB_MICROHTTPD_VERSION "10")
    elseif (DEB_DISTRO MATCHES "artful")
      set(DEB_BOOST_VERSION "1.63.0")
      set(DEB_MICROHTTPD_VERSION "12")
    elseif (DEB_DISTRO MATCHES "bionic")
      set(DEB_BOOST_VERSION "1.65.1")
      set(DEB_MICROHTTPD_VERSION "12")
      set(DEB_LIBCURL_VERSION "4")
    elseif (DEB_DISTRO MATCHES "xenial")
      set(DEB_BOOST_VERSION "1.58.0")
      set(DEB_MICROHTTPD_VERSION "10")
    endif()

    set(DEPENDENCIES "libboost-thread${DEB_BOOST_VERSION}, libboost-system${DEB_BOOST_VERSION}, libboost-filesystem${DEB_BOOST_VERSION}, libmicrohttpd${DEB_MICROHTTPD_VERSION}, libcurl${DEB_LIBCURL_VERSION}, libogg0, libvorbis0a, libvorbisfile3, libncursesw5, libasound2, libpulse0, pulseaudio, libmp3lame0, libev4,")
 
    if (${FFMPEG_DECODER} MATCHES "true")
      set(DEPENDENCIES "${DEPENDENCIES} libavcodec-extra, libavutil55, libavformat57, libswresample2")
    else()
      set(DEPENDENCIES "${DEPENDENCIES} libflac8, libfaad2")
    endif()
 
    set(CPACK_GENERATOR "DEB")
    set(CPACK_PACKAGE_DESCRIPTION "musikcube, a terminal-based music player and library")
    set(CPACK_PACKAGE_VERSION_MAJOR "${musikcube_VERSION_MAJOR}")
    set(CPACK_PACKAGE_VERSION_MINOR "${musikcube_VERSION_MINOR}")
    set(CPACK_PACKAGE_VERSION_PATCH "${musikcube_VERSION_PATCH}")
    set(CPACK_DEBIAN_PACKAGE_DEPENDS ${DEPENDENCIES})
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "casey langen")
    set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE ${DEB_ARCHITECTURE})
    set(CPACK_PACKAGE_FILE_NAME "musikcube_${musikcube_VERSION}_${DEB_DISTRO}_${DEB_ARCHITECTURE}")
    include(CPack)
  endif()
endif()

# run `cmake .` again to pick up build plugin build artifacts that we need
# to file glob in. these won't be picked up on the initial build because
# they don't yet exist!
add_custom_target(postbuild ALL DEPENDS musikcube musikcubed)
add_custom_command(TARGET postbuild POST_BUILD COMMAND cmake .)
