#cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr .
#cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=/usr .
#cmake -DGENERATE_DEB=1 -DDEB_ARCHITECTURE=i386|amd64|armhf -DDEB_PLATFORM=ubuntu -DDEB_DISTRO=eoan -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=Release .
#cmake -DCMAKE_BUILD_TYPE=Release - -DBUILD_STANDALONE=true .

cmake_minimum_required(VERSION 3.0)

project(musikcube)
set (musikcube_VERSION_MAJOR 0)
set (musikcube_VERSION_MINOR 96)
set (musikcube_VERSION_PATCH 11)
set (musikcube_VERSION "${musikcube_VERSION_MAJOR}.${musikcube_VERSION_MINOR}.${musikcube_VERSION_PATCH}")
set (LIBRARY_OUTPUT_PATH ${musikcube_SOURCE_DIR}/bin/plugins)
set (EXECUTABLE_OUTPUT_PATH ${musikcube_SOURCE_DIR}/bin)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/.cmake)
include(CMakeToolsHelpers OPTIONAL)
include(CheckAtomic)
include(ConfigureRpath)
include(ConfigureStandalone)
include(ConfigureBsdPaths)
include(AddDarwinSystemLibs)
include(AddLinuxSystemLibs)
include(ConfigureCurses)
include(ConfigureTaglib)
include(ConfigureCompilerFlags)

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
  find_library(LIBCURL NAMES curl PATHS ${VENDOR_LINK_DIRECTORIES} NO_DEFAULT_PATH)
  find_library(LIBSSL NAMES ssl PATHS ${VENDOR_LINK_DIRECTORIES} NO_DEFAULT_PATH)
  find_library(LIBCRYPTO NAMES crypto PATHS ${VENDOR_LINK_DIRECTORIES} NO_DEFAULT_PATH)
else()
  find_library(LIBCURL NAMES curl)
  find_library(LIBSSL NAMES ssl)
  find_library(LIBCRYPTO NAMES crypto)
endif()

find_library(LIBSSL NAMES ssl PATHS ${VENDOR_LINK_DIRECTORIES})
find_library(LIBCRYPTO NAMES crypto PATHS ${VENDOR_LINK_DIRECTORIES})
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
add_subdirectory(src/plugins/taglib_plugin)
add_subdirectory(src/plugins/nullout)
add_subdirectory(src/plugins/server)
add_subdirectory(src/plugins/httpdatastream)
add_subdirectory(src/plugins/stockencoders)
add_subdirectory(src/plugins/supereqdsp)
add_subdirectory(src/plugins/gmedecoder)
add_subdirectory(src/plugins/ffmpegdecoder)

add_dependencies(musikcube musikcore)
add_dependencies(musikcubed musikcore)

include(AddOsSpecificPlugins)
include(InstallFiles)
include(GenerateDeb)
include(PostBuild)
