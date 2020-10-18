#cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr .
#cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=/usr .
#cmake -DGENERATE_DEB=1 -DDEB_ARCHITECTURE=i386|amd64|armhf -DDEB_PLATFORM=ubuntu -DDEB_DISTRO=eoan -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=Release .
#cmake -DCMAKE_BUILD_TYPE=Release -DLINK_STATICALLY=true .

cmake_minimum_required(VERSION 3.0)

project(musikcube)
set (musikcube_VERSION_MAJOR 0)
set (musikcube_VERSION_MINOR 94)
set (musikcube_VERSION_PATCH 0)
set (musikcube_VERSION "${musikcube_VERSION_MAJOR}.${musikcube_VERSION_MINOR}.${musikcube_VERSION_PATCH}")

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/.cmake)
include(CMakeToolsHelpers OPTIONAL)
include(CheckAtomic)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -Wno-unused-result -Wno-deprecated-declarations")

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -frtti -fexceptions")

# enable for additional memory checking with fsanitize
# set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g3 -fsanitize=address,undefined")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O2 -frtti -fexceptions")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O2")

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

link_directories ("${musikcube_SOURCE_DIR}/bin/plugins")

set(COMMON_LINK_LIBS ${BOOST_LINK_LIBS} curl pthread ssl crypto)

if (CMAKE_SYSTEM_NAME MATCHES "FreeBSD" OR CMAKE_SYSTEM_NAME MATCHES "OpenBSD")
  set (musikcube_LINK_LIBS ${COMMON_LINK_LIBS})
else()
  set (musikcube_LINK_LIBS ${COMMON_LINK_LIBS} dl)
endif()

if(NOT HAVE_CXX_ATOMICS_WITHOUT_LIB)
  if(NOT HAVE_CXX_ATOMICS64_WITHOUT_LIB)
    message(STATUS "[build] libatomic required, adding to library list")
    set (musikcube_LINK_LIBS ${musikcube_LINK_LIBS} atomic)
  endif()
endif()

message(STATUS "[build] link libraries are: ${musikcube_LINK_LIBS}")

include_directories (
  "${musikcube_SOURCE_DIR}/src"
  "${musikcube_SOURCE_DIR}/src/musikcore"
  "${musikcube_SOURCE_DIR}/src/musikcube"
  "${musikcube_SOURCE_DIR}/src/musikcube/cursespp"
  "${musikcube_SOURCE_DIR}/src/3rdparty/include")

# ensure the binaries can find libmusikcore.so, which lives in the
# same directory.
if (NOT CMAKE_SYSTEM_NAME MATCHES "Darwin")
  set(CMAKE_INSTALL_RPATH "$ORIGIN")
  set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
endif()

# "/usr/local" doesn't seem to be included by default on macOS 10.12+
# "/opt/local" is the default installation location for MacPorts
if (CMAKE_SYSTEM_NAME MATCHES "Darwin" OR CMAKE_SYSTEM_NAME MATCHES "FreeBSD" OR CMAKE_SYSTEM_NAME MATCHES "OpenBSD")
  link_directories ("/usr/local/lib")
  link_directories ("/usr/local/opt/openssl/lib")
  include_directories("/usr/local/include")
  include_directories("/usr/local/opt/openssl/include")
endif ()

if (EXISTS "/etc/arch-release" OR EXISTS "/etc/manjaro-release" OR NO_NCURSESW)
  add_definitions (-DNO_NCURSESW)
elseif(CMAKE_SYSTEM_NAME MATCHES "FreeBSD" OR CMAKE_SYSTEM_NAME MATCHES "OpenBSD" )
  add_definitions (-DNO_NCURSESW)
endif()

# we use a bundled version of taglib because the latest release is from 2016,
# and the upstream git repo is hundreds of commits ahead and has a number of
# important bugfixes.
set(USE_BUNDLED_TAGLIB "true")

add_subdirectory(src/musikcore)
add_subdirectory(src/core_c_demo)
add_subdirectory(src/musikcube)
add_subdirectory(src/musikcubed)
add_subdirectory(src/plugins/taglib_plugin)
add_subdirectory(src/plugins/nullout)
add_subdirectory(src/plugins/server)
add_subdirectory(src/plugins/httpdatastream)
add_subdirectory(src/plugins/stockencoders)
add_subdirectory(src/plugins/supereqdsp)
add_subdirectory(src/plugins/gmedecoder)

find_library(LIBOPENMPT NAMES openmpt)
if ("${LIBOPENMPT}" STREQUAL "LIBOPENMPT-NOTFOUND")
  message(STATUS "[libopenmpt] *not* found! plugin will not be built.")
else()
  message(STATUS "[libopenmpt] enabled = true; found library at ${LIBOPENMPT}")
  add_subdirectory(src/plugins/libopenmptdecoder)
endif()

if (${FFMPEG_ENABLED} MATCHES "false")
  message(STATUS "[ffmpeg] enabled = false")
  add_subdirectory(src/plugins/m4adecoder)
  add_subdirectory(src/plugins/oggdecoder)
  add_subdirectory(src/plugins/nomaddecoder)
  add_subdirectory(src/plugins/flacdecoder)
  add_dependencies(musikcube m4adecoder oggdecoder nomaddecoder flacdecoder)
else()
  message(STATUS "[ffmpeg] enabled = true")
  add_subdirectory(src/plugins/ffmpegdecoder)
  add_dependencies(musikcube ffmpegdecoder)
endif()

# systemd / MPRIS detection

if (NOT ENABLE_MPRIS MATCHES "false")
  find_library(LIB_SYSTEMD NAMES systemd)
  if (NOT LIB_SYSTEMD MATCHES "LIB_SYSTEMD-NOTFOUND")
    message(STATUS "[mpris] systemd found at " ${LIB_SYSTEMD})
    message(STATUS "[mpris] setting ENABLE_MPRIS=true")
    set(ENABLE_MPRIS "true")
  else()
    message(STATUS "[mpris] systemd *not* found. MPRIS plugin not enabled")
    set(ENABLE_MPRIS "false")
  endif()
endif()

#end systemd / MPRIS detection

add_dependencies(musikcube musikcore taglibreader nullout server httpdatastream stockencoders)
add_dependencies(musikcubed musikcube)

if (CMAKE_SYSTEM_NAME MATCHES "Linux")
  add_subdirectory(src/plugins/alsaout)
  add_subdirectory(src/plugins/pulseout)
  if (${ENABLE_MPRIS} MATCHES "true")
    add_subdirectory(src/plugins/mpris)
    add_dependencies(musikcube mpris)
  endif()
  if (${ENABLE_SNDIO} MATCHES "true")
    add_subdirectory(src/plugins/sndioout)
    add_dependencies(musikcube sndioout)
  endif()
elseif (CMAKE_SYSTEM_NAME MATCHES "OpenBSD")
  add_subdirectory(src/plugins/sndioout)
  add_dependencies(musikcube sndioout)
  if (${ENABLE_PULSEAUDIO} MATCHES "true")
    add_subdirectory(src/plugins/pulseout) # disabled by default
    add_dependencies(musikcube pulseout)
  endif()
elseif (CMAKE_SYSTEM_NAME MATCHES "FreeBSD")
  add_subdirectory(src/plugins/sndioout)
  add_dependencies(musikcube sndioout)
  if (${ENABLE_PULSEAUDIO} MATCHES "true")
    add_subdirectory(src/plugins/pulseout) # disabled by default
    add_dependencies(musikcube pulseout)
  endif()
  if (${ENABLE_ALSA} MATCHES "true")
    add_subdirectory(src/plugins/alsaout) # disabled by default
    add_dependencies(musikcube alsaout)
  endif()
elseif (CMAKE_SYSTEM_NAME MATCHES "Darwin")
  add_subdirectory(src/plugins/coreaudioout)
  add_dependencies(musikcube coreaudioout)
  add_subdirectory(src/plugins/macosmediakeys)
  add_dependencies(musikcube macosmediakeys)
endif()

if (CMAKE_SYSTEM_NAME MATCHES "Darwin")
  file(GLOB plugins "bin/plugins/*.dylib")
  install(FILES ${plugins} DESTINATION share/musikcube/plugins)
else ()
  file(GLOB plugins "bin/plugins/*.so")
  install(FILES ${plugins} DESTINATION share/musikcube/plugins)
endif ()

file(GLOB sdk_headers "src/musikcore/sdk/*.h")
install(FILES ${sdk_headers} DESTINATION include/musikcube/musikcore/sdk)

file(GLOB themes "src/musikcube/data/themes/*.json")
file(COPY ${themes} DESTINATION bin/themes)
install(FILES ${themes} DESTINATION share/musikcube/themes)

file(GLOB locales "src/musikcube/data/locales/*.json")
file(COPY ${locales} DESTINATION bin/locales)
install(FILES ${locales} DESTINATION share/musikcube/locales)

if (CMAKE_SYSTEM_NAME MATCHES "Linux")
  if (${ENABLE_SNAP} MATCHES "true")
    message(STATUS "[snap] enabled = true")
    file(GLOB linux_share_applications "src/musikcube/data/linux/share/applications/musikcube.snap.desktop")
  else()
    message(STATUS "[snap] enabled = false")
    file(GLOB linux_share_applications "src/musikcube/data/linux/share/applications/musikcube.desktop")
  endif()
  install(FILES ${linux_share_applications} DESTINATION share/applications/)
  file(GLOB linux_share_icons_48 "src/musikcube/data/linux/share/icons/hicolor/48x48/apps/*")
  install(FILES ${linux_share_icons_48} DESTINATION share/icons/hicolor/48x48/apps/)
  file(GLOB linux_share_icons_64 "src/musikcube/data/linux/share/icons/hicolor/64x64/apps/*")
  install(FILES ${linux_share_icons_64} DESTINATION share/icons/hicolor/64x64/apps/)
  file(GLOB linux_share_icons_128 "src/musikcube/data/linux/share/icons/hicolor/128x128/apps/*")
  install(FILES ${linux_share_icons_128} DESTINATION share/icons/hicolor/128x128/apps/)
endif()

# libmusikcore sshared library
if (CMAKE_SYSTEM_NAME MATCHES "Darwin")
  install(FILES "bin/libmusikcore.dylib" DESTINATION share/musikcube)
else()
  install(FILES "bin/libmusikcore.so" DESTINATION share/musikcube)
endif()

# executable and shell script for musikcube
install(
  FILES bin/musikcube
  DESTINATION share/musikcube
  PERMISSIONS
    OWNER_EXECUTE OWNER_READ OWNER_WRITE
    GROUP_EXECUTE GROUP_READ GROUP_WRITE
    WORLD_EXECUTE WORLD_READ)

install(
  FILES "${CMAKE_CURRENT_BINARY_DIR}/src/musikcube/musikcube"
  DESTINATION bin/
  PERMISSIONS
    OWNER_EXECUTE OWNER_READ OWNER_WRITE
    GROUP_EXECUTE GROUP_READ GROUP_WRITE
    WORLD_EXECUTE WORLD_READ)

# executable and shell script for daemon
install(
  FILES bin/musikcubed
  DESTINATION share/musikcube
  PERMISSIONS
    OWNER_EXECUTE OWNER_READ OWNER_WRITE
    GROUP_EXECUTE GROUP_READ GROUP_WRITE
    WORLD_EXECUTE WORLD_READ)

install(
  FILES "${CMAKE_CURRENT_BINARY_DIR}/src/musikcubed/musikcubed"
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
    set(DEB_AVUTIL_VERSION "55")
    set(DEB_AVFORMAT_VERSION "57")
    set(DEB_SWRESAMPLE_VERSION "2")
    set(DEB_NCURSES_VERSION "5")
    if (DEB_DISTRO MATCHES "artful")
      set(DEB_BOOST_VERSION "1.63.0")
      set(DEB_MICROHTTPD_VERSION "12")
    elseif (DEB_DISTRO MATCHES "bionic")
      set(DEB_BOOST_VERSION "1.65.1")
      set(DEB_MICROHTTPD_VERSION "12")
      set(DEB_LIBCURL_VERSION "4")
    elseif (DEB_DISTRO MATCHES "cosmic" OR DEB_DISTRO MATCHES "disco" OR DEB_DISTRO MATCHES "eoan" OR DEB_DISTRO MATCHES "buster")
      set(DEB_BOOST_VERSION "1.67.0")
      set(DEB_MICROHTTPD_VERSION "12")
      set(DEB_LIBCURL_VERSION "4")
      set(DEB_AVUTIL_VERSION "56")
      set(DEB_AVFORMAT_VERSION "58")
      set(DEB_SWRESAMPLE_VERSION "3")
    elseif (DEB_DISTRO MATCHES "focal")
      set(DEB_BOOST_VERSION "1.71.0")
      set(DEB_MICROHTTPD_VERSION "12")
      set(DEB_LIBCURL_VERSION "4")
      set(DEB_AVUTIL_VERSION "56")
      set(DEB_AVFORMAT_VERSION "58")
      set(DEB_SWRESAMPLE_VERSION "3")
      set(DEB_NCURSES_VERSION "6")
    endif()

    set(DEPENDENCIES "libboost-thread${DEB_BOOST_VERSION}, libboost-system${DEB_BOOST_VERSION}, libboost-chrono${DEB_BOOST_VERSION}, libboost-filesystem${DEB_BOOST_VERSION}, libboost-date-time${DEB_BOOST_VERSION}, libmicrohttpd${DEB_MICROHTTPD_VERSION}, libcurl${DEB_LIBCURL_VERSION}, libogg0, libvorbis0a, libvorbisfile3, libncursesw${DEB_NCURSES_VERSION}, libasound2, libpulse0, pulseaudio, libmp3lame0, libev4, libopenmpt0, libssl1.1")

    if (${FFMPEG_ENABLED} MATCHES "false")
      set(DEPENDENCIES "${DEPENDENCIES}, libflac8, libfaad2")
    else()
      set(DEPENDENCIES "${DEPENDENCIES}, libavcodec-extra, libavutil${DEB_AVUTIL_VERSION}, libavformat${DEB_AVFORMAT_VERSION}, libswresample${DEB_SWRESAMPLE_VERSION}")
    endif()

    if (NOT ${USE_BUNDLED_TAGLIB} MATCHES "true")
      set(DEPENDENCIES "${DEPENDENCIES}, libtag1v5")
    endif()

    set(CPACK_GENERATOR "DEB")
    set(CPACK_PACKAGE_DESCRIPTION "musikcube, a terminal-based music player and library")
    set(CPACK_PACKAGE_VERSION_MAJOR "${musikcube_VERSION_MAJOR}")
    set(CPACK_PACKAGE_VERSION_MINOR "${musikcube_VERSION_MINOR}")
    set(CPACK_PACKAGE_VERSION_PATCH "${musikcube_VERSION_PATCH}")
    set(CPACK_DEBIAN_PACKAGE_DEPENDS ${DEPENDENCIES})
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "casey langen")
    set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE ${DEB_ARCHITECTURE})
    set(CPACK_PACKAGE_FILE_NAME "musikcube_${musikcube_VERSION}_${DEB_PLATFORM}_${DEB_DISTRO}_${DEB_ARCHITECTURE}")
    include(CPack)
  endif()
endif()

# run `cmake .` again to pick up build plugin build artifacts that we need
# to file glob in. these won't be picked up on the initial build because
# they don't yet exist!
add_custom_target(postbuild ALL DEPENDS musikcube musikcubed)
add_custom_command(TARGET postbuild POST_BUILD COMMAND cmake .)

# strip binaries in release mode
if (CMAKE_BUILD_TYPE MATCHES Release)
  if ((NOT DEFINED DISABLE_STRIP) OR (NOT ${DISABLE_STRIP} MATCHES "true"))
    message(STATUS "[build] binary stripping enabled for ${CMAKE_CURRENT_SOURCE_DIR}")
    add_custom_command(TARGET postbuild POST_BUILD COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/script/strip-nix.sh" ${CMAKE_CURRENT_SOURCE_DIR})
  else()
    message(STATUS "[build] DISABLE_STRIP=true, *NOT* stripping binaries.")
  endif()
endif()
