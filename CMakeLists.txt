#cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr .
#cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=/usr .
#cmake -DGENERATE_DEB=true -DPACKAGE_ARCHITECTURE=i386|amd64|armhf -DDEB_PLATFORM=ubuntu -DDEB_DISTRO=eoan -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=Release .
#cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_STANDALONE=true .

cmake_minimum_required(VERSION 3.5)

project(musikcube)
set (musikcube_VERSION_MAJOR 3)
set (musikcube_VERSION_MINOR 0)
set (musikcube_VERSION_PATCH 4)
set (musikcube_VERSION "${musikcube_VERSION_MAJOR}.${musikcube_VERSION_MINOR}.${musikcube_VERSION_PATCH}")
set (LIBRARY_OUTPUT_PATH ${musikcube_SOURCE_DIR}/bin/plugins)
set (EXECUTABLE_OUTPUT_PATH ${musikcube_SOURCE_DIR}/bin)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/.cmake)
include(Colors)
include(CMakeToolsHelpers OPTIONAL)
include(CheckAtomic)
include(AddPlugin)
if (${BUILD_STANDALONE} MATCHES "true")
  include(ConfigureStandalone)
endif()
include(ConfigureRpath)
include(ConfigureBsdPaths)
include(AddDarwinSystemLibs)
include(AddLinuxSystemLibs)
include(ConfigureCurses)
include(ConfigureCompilerFlags)
include(DependencyDetection)

if (NOT DEFINED ENV{HOMEBREW_PREFIX} AND NOT ${BUILD_STANDALONE} MATCHES "true")
  find_program(CCACHE_FOUND ccache)
  if (CCACHE_FOUND)
    message(STATUS "${BoldGreen}[ccache] ccache enabled!${ColorReset}")
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK ccache)
  endif(CCACHE_FOUND)
else()
  message(STATUS "${BoldYellow}[ccache] disabled; homebrew or standalone build detected.${ColorReset}")
endif()

if (CROSS_COMPILE_SYSROOT)
  message(STATUS "[cross-compile] enabled, rooted at: ${CROSS_COMPILE_SYSROOT}")
  set(CMAKE_FIND_ROOT_PATH ${CROSS_COMPILE_SYSROOT} ${musikcube_SOURCE_DIR}/vendor)
  set(ENV{PKG_CONFIG_PATH} ${CROSS_COMPILE_PKG_CONFIG_PATH})
else()
  message(STATUS "[cross-compile] not enabled!")
endif()

list(
  APPEND
  PROJECT_INCLUDE_DIRECTORIES
  "${musikcube_SOURCE_DIR}/src"
  "${musikcube_SOURCE_DIR}/src/musikcore"
  "${musikcube_SOURCE_DIR}/src/musikcube"
  "${musikcube_SOURCE_DIR}/src/musikcube/cursespp"
  "${musikcube_SOURCE_DIR}/src/3rdparty/include"
  "${musikcube_SOURCE_DIR}/src/3rdparty/asio/asio/include")

include_directories(${PROJECT_INCLUDE_DIRECTORIES})

link_directories("${musikcube_SOURCE_DIR}/bin/plugins")

message(STATUS "[vendor-link-directories] ${VENDOR_LINK_DIRECTORIES}")
message(STATUS "[vendor-include-directories] ${VENDOR_INCLUDE_DIRECTORIES}")
message(STATUS "[os-system-libs] ${DEFAULT_OS_SYSTEM_LIBS}")
message(STATUS "[project-include-directories] ${PROJECT_INCLUDE_DIRECTORIES}")

# these are used to (1) disable the standalone ASIO from trying to use
# boost, and (2) instruct websocketpp to use standalone (not boost) ASIO
add_definitions(
  -DBOOST_DATE_TIME_NO_LIB
  -DBOOST_REGEX_NO_LIB
  -D_WEBSOCKETPP_CPP11_TYPE_TRAITS_
  -D_WEBSOCKETPP_CPP11_RANDOM_DEVICE_
  -DASIO_STANDALONE)

# disable update check for environments that we know will provided hosted,
# prebuilt binaries.
if (DEFINED ENV{HOMEBREW_PREFIX})
  message(STATUS "[build] detected homebrew, disabling update check functionality.")
  add_definitions(-DDISABLE_UPDATE_CHECK)
endif()

if (${BUILD_STANDALONE} MATCHES "true")
  find_vendor_library(LIBCURL curl)
  find_vendor_library(LIBSSL ssl)
  find_vendor_library(LIBCRYPTO crypto)
else()
  # need VENDOR_LINK_DIRECTORIES here for Darwin.
  find_library(LIBCURL NAMES curl PATHS ${VENDOR_LINK_DIRECTORIES})
  find_library(LIBSSL NAMES ssl PATHS ${VENDOR_LINK_DIRECTORIES})
  find_library(LIBCRYPTO NAMES crypto PATHS ${VENDOR_LINK_DIRECTORIES})
endif()

find_library_and_header(LIBZ z zlib.h)
find_library_and_header(PTHREAD pthread pthread.h)

set(musikcube_LINK_LIBS ${DEFAULT_OS_SYSTEM_LIBS} ${LIBCURL} ${LIBSSL} ${LIBCRYPTO} ${LIBZ} ${PTHREAD})

if(NOT HAVE_CXX_ATOMICS_WITHOUT_LIB)
  if(NOT HAVE_CXX_ATOMICS64_WITHOUT_LIB)
    message(STATUS "[build] libatomic required, adding to library list")
    set (musikcube_LINK_LIBS ${musikcube_LINK_LIBS} atomic)
  endif()
endif()

if (
  (CMAKE_COMPILER_IS_GNUCC AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.0) OR
  (CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 8.0)
)
  message(STATUS "[build] detected old gcc, manually adding -lstdc++fs")
  set (musikcube_LINK_LIBS ${musikcube_LINK_LIBS} stdc++fs)
endif()

message(STATUS "[build] musikcube_LINK_LIBS: " ${musikcube_LINK_LIBS})

add_subdirectory(src/musikcore)
add_subdirectory(src/core_c_demo)
add_subdirectory(src/musikcube)
add_subdirectory(src/musikcubed)

add_dependencies(musikcube musikcore)
add_dependencies(musikcubed musikcore)

# tag readers
add_plugin("src/plugins/taglib_plugin" "taglibreader")
# outputs
add_plugin("src/plugins/alsaout" "alsaout")
add_plugin("src/plugins/coreaudioout" "coreaudioout")
add_plugin("src/plugins/nullout" "nullout")
add_plugin("src/plugins/pipewireout" "pipewireout")
add_plugin("src/plugins/portaudioout" "portaudioout")
add_plugin("src/plugins/pulseout" "pulseout")
add_plugin("src/plugins/sndioout" "sndioout")
# remotes
add_plugin("src/plugins/macosmediakeys" "macosmediakeys")
add_plugin("src/plugins/mpris" "mpris")
add_plugin("src/plugins/server" "server")
# streams
add_plugin("src/plugins/httpdatastream" "httpdatastream")
# decoders
add_plugin("src/plugins/ffmpegdecoder" "ffmpegdecoder")
add_plugin("src/plugins/gmedecoder" "gmedecoder")
add_plugin("src/plugins/libopenmptdecoder" "openmptdecoder")
# encoders
add_plugin("src/plugins/stockencoders" "stockencoders")
# dsps
add_plugin("src/plugins/supereqdsp" "supereqdsp")

add_custom_target(postbuild ALL DEPENDS musikcube musikcubed)
add_custom_command(
  TARGET postbuild
  POST_BUILD
  COMMAND
  "${CMAKE_CURRENT_SOURCE_DIR}/script/post-build.sh"
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_SYSTEM_NAME}
  ${CMAKE_BUILD_TYPE}
  ${BUILD_STANDALONE})

include(InstallFiles)
include(GeneratePackage)
