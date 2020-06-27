cmake_minimum_required(VERSION 3.0.0 FATAL_ERROR)

project(taglib)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules")

if(DEFINED ENABLE_STATIC)
  message(FATAL_ERROR "This option is no longer available, use BUILD_SHARED_LIBS instead")
endif()

option(BUILD_SHARED_LIBS "Build shared libraries" OFF)
if(APPLE)
  option(BUILD_FRAMEWORK "Build an OS X framework" OFF)
  if(BUILD_FRAMEWORK)
    set(BUILD_SHARED_LIBS ON)
    #set(CMAKE_MACOSX_RPATH 1)
    set(FRAMEWORK_INSTALL_DIR "/Library/Frameworks" CACHE STRING "Directory to install frameworks to.")
  endif()
endif()
if(NOT BUILD_SHARED_LIBS)
  add_definitions(-DTAGLIB_STATIC)
endif()
option(ENABLE_STATIC_RUNTIME "Visual Studio, link with runtime statically" OFF)

option(ENABLE_CCACHE "Use ccache when building libtag" OFF)
if(ENABLE_CCACHE)
  find_program(CCACHE_FOUND ccache)
  if(CCACHE_FOUND)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK ccache)
  endif()
endif()

option(VISIBILITY_HIDDEN "Build with -fvisibility=hidden" OFF)
option(BUILD_TESTS "Build the test suite" OFF)
option(BUILD_EXAMPLES "Build the examples" OFF)
option(BUILD_BINDINGS "Build the bindings" ON)

option(NO_ITUNES_HACKS "Disable workarounds for iTunes bugs" OFF)

option(PLATFORM_WINRT "Enable WinRT support" OFF)
if(PLATFORM_WINRT)
  add_definitions(-DPLATFORM_WINRT)
endif()

add_definitions(-DHAVE_CONFIG_H)
set(TESTS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/tests/")

## the following are directories where stuff will be installed to
set(LIB_SUFFIX "" CACHE STRING "Define suffix of directory name (32/64)")
set(EXEC_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}" CACHE PATH "Base directory for executables and libraries")
set(BIN_INSTALL_DIR "${EXEC_INSTALL_PREFIX}/bin" CACHE PATH "The subdirectory to the binaries prefix (default prefix/bin)")
set(LIB_INSTALL_DIR "${EXEC_INSTALL_PREFIX}/lib${LIB_SUFFIX}" CACHE PATH "The subdirectory relative to the install prefix where libraries will be installed (default is /lib${LIB_SUFFIX})")
set(INCLUDE_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/include" CACHE PATH "The subdirectory to the header prefix")

if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")
endif()

if(MSVC AND ENABLE_STATIC_RUNTIME)
  foreach(flag_var CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE CMAKE_CXX_FLAGS_MINSIZEREL CMAKE_CXX_FLAGS_RELWITHDEBINFO)
    string(REGEX REPLACE "/MD" "/MT" ${flag_var} "${${flag_var}}")
  endforeach(flag_var)
endif()

# Read version information from file taglib/toolkit/taglib.h into variables
# TAGLIB_LIB_MAJOR_VERSION, TAGLIB_LIB_MINOR_VERSION, TAGLIB_LIB_PATCH_VERSION.
foreach(version_part MAJOR MINOR PATCH)
  set(version_var_name "TAGLIB_${version_part}_VERSION")
  file(STRINGS taglib/toolkit/taglib.h version_line
       REGEX "^#define +${version_var_name}")
  if(NOT version_line)
    message(FATAL_ERROR "${version_var_name} not found in taglib.h")
  endif()
  string(REGEX MATCH "${version_var_name} +([^ ]+)" result ${version_line})
  set(TAGLIB_LIB_${version_part}_VERSION ${CMAKE_MATCH_1})
endforeach(version_part)

# Only used to force cmake rerun when taglib.h changes.
configure_file(taglib/toolkit/taglib.h ${CMAKE_CURRENT_BINARY_DIR}/taglib.h.stamp)

if("${TAGLIB_LIB_PATCH_VERSION}" EQUAL "0")
  set(TAGLIB_LIB_VERSION_STRING "${TAGLIB_LIB_MAJOR_VERSION}.${TAGLIB_LIB_MINOR_VERSION}")
else()
  set(TAGLIB_LIB_VERSION_STRING "${TAGLIB_LIB_MAJOR_VERSION}.${TAGLIB_LIB_MINOR_VERSION}.${TAGLIB_LIB_PATCH_VERSION}")
endif()

# 1. If the library source code has changed at all since the last update, then increment revision.
# 2. If any interfaces have been added, removed, or changed since the last update, increment current, and set revision to 0.
# 3. If any interfaces have been added since the last public release, then increment age.
# 4. If any interfaces have been removed since the last public release, then set age to 0.
set(TAGLIB_SOVERSION_CURRENT  18)
set(TAGLIB_SOVERSION_REVISION 0)
set(TAGLIB_SOVERSION_AGE      17)

math(EXPR TAGLIB_SOVERSION_MAJOR "${TAGLIB_SOVERSION_CURRENT} - ${TAGLIB_SOVERSION_AGE}")
math(EXPR TAGLIB_SOVERSION_MINOR "${TAGLIB_SOVERSION_AGE}")
math(EXPR TAGLIB_SOVERSION_PATCH "${TAGLIB_SOVERSION_REVISION}")

include(ConfigureChecks.cmake)

if(${ZLIB_FOUND})
  set(ZLIB_LIBRARIES_FLAGS -lz)
endif()

if(NOT WIN32)
  configure_file("${CMAKE_CURRENT_SOURCE_DIR}/taglib-config.cmake" "${CMAKE_CURRENT_BINARY_DIR}/taglib-config" @ONLY)
  install(PROGRAMS "${CMAKE_CURRENT_BINARY_DIR}/taglib-config" DESTINATION "${BIN_INSTALL_DIR}")
endif()

if(WIN32)
  configure_file("${CMAKE_CURRENT_SOURCE_DIR}/taglib-config.cmd.cmake" "${CMAKE_CURRENT_BINARY_DIR}/taglib-config.cmd")
  install(PROGRAMS "${CMAKE_CURRENT_BINARY_DIR}/taglib-config.cmd" DESTINATION "${BIN_INSTALL_DIR}")
endif()

if(NOT BUILD_FRAMEWORK)
  configure_file("${CMAKE_CURRENT_SOURCE_DIR}/taglib.pc.cmake" "${CMAKE_CURRENT_BINARY_DIR}/taglib.pc" @ONLY)
  install(FILES "${CMAKE_CURRENT_BINARY_DIR}/taglib.pc" DESTINATION "${LIB_INSTALL_DIR}/pkgconfig")
endif()

if(NOT HAVE_ZLIB AND ZLIB_SOURCE)
  set(HAVE_ZLIB 1)
  set(HAVE_ZLIB_SOURCE 1)
endif()

include_directories(${CMAKE_CURRENT_BINARY_DIR})
configure_file(config.h.cmake "${CMAKE_CURRENT_BINARY_DIR}/config.h")

if(WITH_ASF)
  set(TAGLIB_WITH_ASF TRUE)
endif()
if(WITH_MP4)
  set(TAGLIB_WITH_MP4 TRUE)
endif()

option(TRACE_IN_RELEASE "Output debug messages even in release mode" OFF)
if(TRACE_IN_RELEASE)
  set(TRACE_IN_RELEASE TRUE)
endif()

configure_file(taglib/taglib_config.h.cmake "${CMAKE_CURRENT_BINARY_DIR}/taglib_config.h")

add_subdirectory(taglib)

if(BUILD_BINDINGS)
  add_subdirectory(bindings)
endif()

if(BUILD_TESTS AND NOT BUILD_SHARED_LIBS)
  enable_testing()
  add_subdirectory(tests)
endif()

if(BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.cmake" "${CMAKE_CURRENT_BINARY_DIR}/Doxyfile")
file(COPY doc/taglib.png DESTINATION doc)
add_custom_target(docs doxygen)

# uninstall target
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/cmake_uninstall.cmake.in" "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake" IMMEDIATE @ONLY)

if(NOT TARGET uninstall)
  add_custom_target(uninstall COMMAND "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake")
endif()
