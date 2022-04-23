#cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr .
#cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=/usr .
#cmake -DGENERATE_DEB=true -DDEB_ARCHITECTURE=i386|amd64|armhf -DDEB_PLATFORM=ubuntu -DDEB_DISTRO=eoan -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=Release .
#cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_STANDALONE=true .

cmake_minimum_required(VERSION 3.0)

project(musikcube)
set (musikcube_VERSION_MAJOR 0)
set (musikcube_VERSION_MINOR 97)
set (musikcube_VERSION_PATCH 1)
set (musikcube_VERSION "${musikcube_VERSION_MAJOR}.${musikcube_VERSION_MINOR}.${musikcube_VERSION_PATCH}")
set (LIBRARY_OUTPUT_PATH ${musikcube_SOURCE_DIR}/bin/plugins)
set (EXECUTABLE_OUTPUT_PATH ${musikcube_SOURCE_DIR}/bin)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/.cmake)
include(Colors)
include(CMakeToolsHelpers OPTIONAL)
include(CheckAtomic)
include(AddPlugin)
include(ConfigureRpath)
include(ConfigureStandalone)
include(ConfigureBsdPaths)
include(AddDarwinSystemLibs)
include(AddLinuxSystemLibs)
include(ConfigureCurses)
include(ConfigureTaglib)
include(ConfigureCompilerFlags)
include(FindVendorLibrary)

find_program(CCACHE_FOUND ccache)
if (CCACHE_FOUND)
  message(STATUS "${BoldGreen}[ccache] ccache enabled!${ColorReset}")
  set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)
  set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK ccache)
endif(CCACHE_FOUND)

if (CROSS_COMPILE_SYSROOT)
  message(STATUS "[cross-compile] enabled, rooted at: ${CROSS_COMPILE_SYSROOT}")
  set(CMAKE_FIND_ROOT_PATH ${CROSS_COMPILE_SYSROOT} ${musikcube_SOURCE_DIR}/vendor)
  set(ENV{PKG_CONFIG_PATH} ${CROSS_COMPILE_PKG_CONFIG_PATH})
else()
  message(STATUS "[cross-compile] not enabled!")
endif()

message(STATUS "[vendor-link-directories] ${VENDOR_LINK_DIRECTORIES}")
message(STATUS "[vendor-include-directories] ${VENDOR_INCLUDE_DIRECTORIES}")
message(STATUS "[os-system-libs] ${DEFAULT_OS_SYSTEM_LIBS}")

include_directories(
  "${musikcube_SOURCE_DIR}/src"
  "${musikcube_SOURCE_DIR}/src/musikcore"
  "${musikcube_SOURCE_DIR}/src/musikcube"
  "${musikcube_SOURCE_DIR}/src/musikcube/cursespp"
  "${musikcube_SOURCE_DIR}/src/3rdparty/include")

link_directories("${musikcube_SOURCE_DIR}/bin/plugins")

find_package(Boost 1.55.0 REQUIRED system filesystem thread)
include_directories(${Boost_INCLUDE_DIRS})
message(STATUS "[boost] libs: " ${Boost_LIBRARIES})
message(STATUS "[boost] includes: " ${Boost_INCLUDE_DIRS})

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

find_library(LIBZ NAMES z)

set(musikcube_LINK_LIBS ${DEFAULT_OS_SYSTEM_LIBS} ${Boost_LIBRARIES} ${LIBCURL} ${LIBSSL} ${LIBCRYPTO} ${LIBZ})

if(NOT HAVE_CXX_ATOMICS_WITHOUT_LIB)
  if(NOT HAVE_CXX_ATOMICS64_WITHOUT_LIB)
    message(STATUS "[build] libatomic required, adding to library list")
    set (musikcube_LINK_LIBS ${musikcube_LINK_LIBS} atomic)
  endif()
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
add_plugin("src/plugins/nullout" "nullout")
add_plugin("src/plugins/alsaout" "alsaout")
add_plugin("src/plugins/pulseout" "pulseout")
add_plugin("src/plugins/pipewireout" "pipewireout")
add_plugin("src/plugins/sndioout" "sndioout")
add_plugin("src/plugins/coreaudioout" "coreaudioout")
# remotes
add_plugin("src/plugins/server" "server")
add_plugin("src/plugins/mpris" "mpris")
add_plugin("src/plugins/macosmediakeys" "macosmediakeys")
# streams
add_plugin("src/plugins/httpdatastream" "httpdatastream")
# decoders
add_plugin("src/plugins/ffmpegdecoder" "ffmpegdecoder")
add_plugin("src/plugins/libopenmptdecoder" "openmptdecoder")
add_plugin("src/plugins/gmedecoder" "gmedecoder")
# encoders
add_plugin("src/plugins/stockencoders" "stockencoders")
# dsps
add_plugin("src/plugins/supereqdsp" "supereqdsp")

include(InstallFiles)
include(GeneratePackage)
include(PostBuild)
