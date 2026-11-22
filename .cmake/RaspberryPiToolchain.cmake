# x-tools owns libc and the compiler runtime. the staged packages are linker inputs.
list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES MUSIKCUBE_XTOOLS_FILE MUSIKCUBE_DEPENDENCY_ROOT)
include("${MUSIKCUBE_XTOOLS_FILE}")

set(target_lib "${MUSIKCUBE_DEPENDENCY_ROOT}/usr/lib")
set(target_multiarch "arm-linux-gnueabihf")
set(CMAKE_LIBRARY_ARCHITECTURE "${target_multiarch}")
list(PREPEND CMAKE_FIND_ROOT_PATH "${MUSIKCUBE_DEPENDENCY_ROOT}")
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
set(ENV{PKG_CONFIG_PATH} "")
set(ENV{PKG_CONFIG_LIBDIR} "${target_lib}/${target_multiarch}/pkgconfig:${target_lib}/pkgconfig")
set(ENV{PKG_CONFIG_SYSROOT_DIR} "${MUSIKCUBE_DEPENDENCY_ROOT}")

set(MUSIKCUBE_DEPENDENCY_INCLUDE_DIRS
  "${MUSIKCUBE_DEPENDENCY_ROOT}/usr/include/${target_multiarch}"
  "${MUSIKCUBE_DEPENDENCY_ROOT}/usr/include")
list(APPEND VENDOR_INCLUDE_DIRECTORIES "${MUSIKCUBE_DEPENDENCY_ROOT}/usr/include")
list(REMOVE_DUPLICATES VENDOR_INCLUDE_DIRECTORIES)
set(MUSIKCUBE_FLAT_CURSES_HEADERS TRUE)
set(MUSIKCUBE_CXX_OPTIONS -fvisibility-inlines-hidden)
set(CMAKE_CXX_FLAGS_INIT "${CMAKE_CXX_FLAGS_INIT} -static-libstdc++")

set(LIBNCURSES "${target_lib}/libncursesw.a")
set(LIBPANEL "${target_lib}/libpanelw.a")
set(LIBTINFO "${target_lib}/libtinfo.a")
set(EVLIB "${target_lib}/libev.a")
set(MUSIKCUBE_REQUIRED_VENDOR_ARCHIVES "${LIBNCURSES}" "${LIBPANEL}" "${LIBTINFO}" "${EVLIB}")
set(LIBZ "${target_lib}/libz.so.1")
