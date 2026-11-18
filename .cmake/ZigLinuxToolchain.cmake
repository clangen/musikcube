set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES MUSIKCUBE_DEPENDENCY_ROOT MUSIKCUBE_ZIG_WRAPPERS)
set(CMAKE_C_COMPILER "${MUSIKCUBE_ZIG_WRAPPERS}/cc")
set(CMAKE_CXX_COMPILER "${MUSIKCUBE_ZIG_WRAPPERS}/c++")
set(CMAKE_AR "${MUSIKCUBE_ZIG_WRAPPERS}/ar")
set(CMAKE_RANLIB "${MUSIKCUBE_ZIG_WRAPPERS}/ranlib")

# Zig supplies glibc. The dependency sysroot is for CMake and pkg-config discovery.
set(CMAKE_FIND_ROOT_PATH "${MUSIKCUBE_DEPENDENCY_ROOT}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

string(JOIN ":" zig_pkg_config_libdir
  "${MUSIKCUBE_DEPENDENCY_ROOT}/usr/lib/x86_64-linux-gnu/pkgconfig"
  "${MUSIKCUBE_DEPENDENCY_ROOT}/usr/lib/pkgconfig"
  "${MUSIKCUBE_DEPENDENCY_ROOT}/usr/share/pkgconfig")
set(ENV{PKG_CONFIG_LIBDIR} "${zig_pkg_config_libdir}")
set(ENV{PKG_CONFIG_SYSROOT_DIR} "${MUSIKCUBE_DEPENDENCY_ROOT}")
set(ENV{PKG_CONFIG_PATH} "")

set(CMAKE_LIBRARY_ARCHITECTURE x86_64-linux-gnu)
list(APPEND CMAKE_LIBRARY_PATH
  "${MUSIKCUBE_DEPENDENCY_ROOT}/usr/lib/x86_64-linux-gnu"
  "${MUSIKCUBE_DEPENDENCY_ROOT}/lib/x86_64-linux-gnu"
  "${MUSIKCUBE_DEPENDENCY_ROOT}/usr/lib"
  "${MUSIKCUBE_DEPENDENCY_ROOT}/lib")

set(MUSIKCUBE_DEPENDENCY_INCLUDE_DIRS
  "${MUSIKCUBE_DEPENDENCY_ROOT}/usr/include/x86_64-linux-gnu"
  "${MUSIKCUBE_DEPENDENCY_ROOT}/usr/include")

# Use the glibc target stubs supplied by Zig.
set(LIBDL dl)
set(PTHREAD pthread)
find_library(target_zlib NAMES libz.so.1 PATHS ${CMAKE_LIBRARY_PATH}
  NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH NO_CACHE REQUIRED)
set(LIBZ "${target_zlib}")

get_filename_component(vendor_usr "${CMAKE_CURRENT_LIST_DIR}/../vendor/sysroot/usr" ABSOLUTE)
set(MUSIKCUBE_FLAT_CURSES_HEADERS TRUE)
list(APPEND VENDOR_INCLUDE_DIRECTORIES "${vendor_usr}/include")
list(REMOVE_DUPLICATES VENDOR_INCLUDE_DIRECTORIES)
set(LIBNCURSES "${vendor_usr}/lib/libncursesw.a")
set(LIBPANEL "${vendor_usr}/lib/libpanelw.a")
set(LIBTINFO "${vendor_usr}/lib/libtinfo.a")
set(EVLIB "${vendor_usr}/lib/libev.a")
set(MUSIKCUBE_REQUIRED_VENDOR_ARCHIVES "${LIBNCURSES}" "${LIBPANEL}" "${LIBTINFO}" "${EVLIB}")

# Prevent inline functions from crossing binaries with separate static libc++ state.
set(MUSIKCUBE_CXX_OPTIONS -fvisibility-inlines-hidden)
