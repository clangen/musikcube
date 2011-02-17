#project(taglib)

#cmake_minimum_required(VERSION 2.6.0 FATAL_ERROR)

OPTION(ENABLE_STATIC "Make static version of libtag"  OFF)

OPTION(BUILD_TESTS "Build the test suite"  OFF)
OPTION(BUILD_EXAMPLES "Build the examples"  OFF)

OPTION(NO_ITUNES_HACKS "Disable workarounds for iTunes bugs"  OFF)
OPTION(WITH_ASF "Enable ASF tag reading/writing code"  OFF)
OPTION(WITH_MP4 "Enable MP4 tag reading/writing code"  OFF)

add_definitions(-DHAVE_CONFIG_H)

#add some KDE specific stuff
set(LIB_SUFFIX "" CACHE STRING "Define suffix of directory name (32/64)" )
set(EXEC_INSTALL_PREFIX  ${CMAKE_INSTALL_PREFIX}       CACHE PATH  "Base directory for executables and libraries" FORCE)
#
## the following are directories where stuff will be installed to
set(BIN_INSTALL_DIR          "${EXEC_INSTALL_PREFIX}/bin"    CACHE PATH "The subdirectory to the binaries prefix (default prefix/bin)" FORCE)
set(LIB_INSTALL_DIR          "${EXEC_INSTALL_PREFIX}/lib${LIB_SUFFIX}"     CACHE PATH "The subdirectory relative to the install prefix where libraries will be installed (default is /lib${LIB_SUFFIX})" FORCE)
set(INCLUDE_INSTALL_DIR      "${CMAKE_INSTALL_PREFIX}/include"             CACHE PATH "The subdirectory to the header prefix" FORCE)

if (CMAKE_COMPILER_IS_GNUCXX)
   if (CMAKE_SYSTEM_NAME MATCHES Linux)
     set ( CMAKE_C_FLAGS     "${CMAKE_C_FLAGS} -Wno-long-long -ansi -Wundef -Wcast-align -Werror-implicit-function-declaration -Wchar-subscripts -Wall -W -Wpointer-arith -Wwrite-strings -Wformat-security -Wmissing-format-attribute -fno-common")
     set ( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wnon-virtual-dtor -Wno-long-long -ansi -Wundef -Wcast-align -Wchar-subscripts -Wall -W -Wpointer-arith -Wwrite-strings -Wformat-security -fno-check-new -fno-common")
   endif (CMAKE_SYSTEM_NAME MATCHES Linux)
endif (CMAKE_COMPILER_IS_GNUCXX)
if(MSVC)
  if (MSVC_VERSION GREATER 1399)
    # If using Visual C++ 2005 (MSVC80) and greater (MSVC_VERSION=1400)
    add_definitions(/D_CRT_SECURE_NO_DEPRECATE /D_CRT_NONSTDC_NO_DEPRECATE)
  endif (MSVC_VERSION GREATER 1399)
endif(MSVC)
if (WIN32)
  set(CMAKE_DEBUG_POSTFIX "d")
endif (WIN32)

SET(TAGLIB_LIB_MAJOR_VERSION "1")
SET(TAGLIB_LIB_MINOR_VERSION "6")
SET(TAGLIB_LIB_PATCH_VERSION "3")

SET(TAGLIB_LIB_VERSION_STRING "${TAGLIB_LIB_MAJOR_VERSION}.${TAGLIB_LIB_MINOR_VERSION}.${TAGLIB_LIB_PATCH_VERSION}")


include(ConfigureChecks.cmake)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/taglib-config.cmake ${CMAKE_CURRENT_BINARY_DIR}/taglib-config )

if(NOT WIN32)
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/taglib.pc.cmake ${CMAKE_CURRENT_BINARY_DIR}/taglib.pc )
endif(NOT WIN32)
include_directories(${CMAKE_CURRENT_BINARY_DIR})
configure_file(config-taglib.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h )

if(WITH_ASF)
    set(TAGLIB_WITH_ASF TRUE)
endif(WITH_ASF)
if(WITH_MP4)
    set(TAGLIB_WITH_MP4 TRUE)
endif(WITH_MP4)
configure_file(taglib/taglib_config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/taglib_config.h )
install( FILES  ${CMAKE_CURRENT_BINARY_DIR}/taglib_config.h DESTINATION ${INCLUDE_INSTALL_DIR}/taglib)

ADD_SUBDIRECTORY( taglib ) 

#ADD_SUBDIRECTORY(tests)
#ADD_SUBDIRECTORY(examples)

#ADD_SUBDIRECTORY(bindings)
if(NOT WIN32)
  install( FILES  ${CMAKE_CURRENT_BINARY_DIR}/taglib.pc DESTINATION ${LIB_INSTALL_DIR}/pkgconfig )
endif(NOT WIN32)

INSTALL( PROGRAMS  ${CMAKE_CURRENT_BINARY_DIR}/taglib-config DESTINATION ${BIN_INSTALL_DIR})
