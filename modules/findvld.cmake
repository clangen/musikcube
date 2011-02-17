# Module for locating the Visual Leak Detector.
# 
# Cutomizable variables:
#   VLD_ROOT_DIR
#     This variable points to the Visual Leak Detector root directory. By
#     default, the module looks for the installation directory by examining the
#     Program Files/Program Files (x86) folders.
#
# Read-Only variables:
#   VLD_FOUND
#     Indicates that the library has been found.
#
#   VLD_INCLUDE_DIR
#     Points to the Visual Leak Detector include directory.
#
#   VLD_LIBRARIES
#     Points to the Visual Leak Detector libraries that can be passed to
#     target_link_libararies.
#
#   VLD_LIBRARIES_DIR
#     Points to the Visual Leak Detector directory that contains the libraries.
#     The content of this variable can be passed to link_directories.
#
# Copyright (c) 2010 Sergiu Dotenco
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

INCLUDE (FindPackageHandleStandardArgs)

SET (_VLD_POSSIBLE_DIRS
  ${VLD_ROOT_DIR}
  "$ENV{PROGRAMFILES}/Visual Leak Detector"
  "$ENV{PROGRAMFILES(X86)}/Visual Leak Detector")

SET (_VLD_POSSIBLE_INCLUDE_SUFFIXES
  include)

SET (_VLD_POSSIBLE_LIB_SUFFIXES
  lib)

# Version 2.0 uses vld_x86 and vld_x64 instead of simply vld as library names
IF (CMAKE_SIZEOF_VOID_P EQUAL 4)
  LIST (APPEND _VLD_POSSIBLE_LIB_SUFFIXES lib/Win32)
ELSEIF (CMAKE_SIZEOF_VOID_P EQUAL 8)
  LIST (APPEND _VLD_POSSIBLE_LIB_SUFFIXES lib/Win64)
ENDIF (CMAKE_SIZEOF_VOID_P EQUAL 4)

FIND_PATH (VLD_ROOT_DIR
  NAMES include/vld.h
  PATHS ${_VLD_POSSIBLE_DIRS}
  DOC "VLD root directory")

FIND_PATH (VLD_INCLUDE_DIR 
  NAMES vld.h
  PATHS ${VLD_ROOT_DIR}
  PATH_SUFFIXES ${_VLD_POSSIBLE_INCLUDE_SUFFIXES}
  DOC "VLD include directory")

FIND_PATH (VLD_LIBRARIES_DIR
  NAMES vld.lib
  PATHS ${VLD_ROOT_DIR}
  PATH_SUFFIXES ${_VLD_POSSIBLE_LIB_SUFFIXES}
  DOC "VLD libraries directory")

IF (MSVC)
  FIND_LIBRARY (VLD_LIBRARIES_DEBUG
    NAMES vld
    PATHS ${VLD_ROOT_DIR}
    PATH_SUFFIXES ${_VLD_POSSIBLE_LIB_SUFFIXES}
    DOC "VLD libraries")

  SET (VLD_LIBRARIES debug ${VLD_LIBRARIES_DEBUG})
ENDIF (MSVC)

FIND_PACKAGE_HANDLE_STANDARD_ARGS (VLD DEFAULT_MSG
  VLD_INCLUDE_DIR VLD_LIBRARIES)

IF (NOT VLD_FOUND)
  IF (NOT PACKAGE_FIND_QUIETLY)
    IF (PACKAGE_FIND_REQUIRED)
      MESSAGE (FATAL_ERROR 
        "VLD required but some files were not found. "
        "Specify the VLD location using VLD_ROOT_DIR")
    ENDIF (PACKAGE_FIND_REQUIRED)
  ENDIF (NOT PACKAGE_FIND_QUIETLY)
ENDIF (NOT VLD_FOUND)

MARK_AS_ADVANCED (VLD_INCLUDE_DIR VLD_LIBRARIES VLD_LIBRARIES_DIR)
