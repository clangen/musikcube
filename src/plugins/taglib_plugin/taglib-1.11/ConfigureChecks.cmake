include(CheckLibraryExists)
include(CheckTypeSize)
include(CheckCXXSourceCompiles)

# Check if the size of numeric types are suitable.

check_type_size("short" SIZEOF_SHORT)
if(NOT ${SIZEOF_SHORT} EQUAL 2)
  message(FATAL_ERROR "TagLib requires that short is 16-bit wide.")
endif()

check_type_size("int" SIZEOF_INT)
if(NOT ${SIZEOF_INT} EQUAL 4)
  message(FATAL_ERROR "TagLib requires that int is 32-bit wide.")
endif()

check_type_size("long long" SIZEOF_LONGLONG)
if(NOT ${SIZEOF_LONGLONG} EQUAL 8)
  message(FATAL_ERROR "TagLib requires that long long is 64-bit wide.")
endif()

check_type_size("wchar_t" SIZEOF_WCHAR_T)
if(${SIZEOF_WCHAR_T} LESS 2)
  message(FATAL_ERROR "TagLib requires that wchar_t is sufficient to store a UTF-16 char.")
endif()

check_type_size("float" SIZEOF_FLOAT)
if(NOT ${SIZEOF_FLOAT} EQUAL 4)
  message(FATAL_ERROR "TagLib requires that float is 32-bit wide.")
endif()

check_type_size("double" SIZEOF_DOUBLE)
if(NOT ${SIZEOF_DOUBLE} EQUAL 8)
  message(FATAL_ERROR "TagLib requires that double is 64-bit wide.")
endif()

# Enable check_cxx_source_compiles() to work with Boost "header-only" libraries.

find_package(Boost)
if(Boost_FOUND)
  set(CMAKE_REQUIRED_INCLUDES "${CMAKE_REQUIRED_INCLUDES};${Boost_INCLUDE_DIRS}")
endif()

# Determine which kind of atomic operations your compiler supports.

check_cxx_source_compiles("
  #include <atomic>
  int main() {
    std::atomic<unsigned int> x;
    x.fetch_add(1);
    x.fetch_sub(1);
    return 0;
  }
" HAVE_STD_ATOMIC)

if(NOT HAVE_STD_ATOMIC)
  find_package(Boost COMPONENTS atomic)
  if(Boost_ATOMIC_FOUND)
    set(HAVE_BOOST_ATOMIC 1)
  else()
    set(HAVE_BOOST_ATOMIC 0)
  endif()

  if(NOT HAVE_BOOST_ATOMIC)
    check_cxx_source_compiles("
      int main() {
        volatile int x;
        __sync_add_and_fetch(&x, 1);
        int y = __sync_sub_and_fetch(&x, 1);
        return 0;
      }
    " HAVE_GCC_ATOMIC)

    if(NOT HAVE_GCC_ATOMIC)
      check_cxx_source_compiles("
        #include <libkern/OSAtomic.h>
        int main() {
          volatile int32_t x;
          OSAtomicIncrement32Barrier(&x);
          int32_t y = OSAtomicDecrement32Barrier(&x);
          return 0;
        }
      " HAVE_MAC_ATOMIC)

      if(NOT HAVE_MAC_ATOMIC)
        check_cxx_source_compiles("
          #include <windows.h>
          int main() {
            volatile LONG x;
            InterlockedIncrement(&x);
            LONG y = InterlockedDecrement(&x);
            return 0;
          }
        " HAVE_WIN_ATOMIC)

        if(NOT HAVE_WIN_ATOMIC)
          check_cxx_source_compiles("
            #include <ia64intrin.h>
            int main() {
              volatile int x;
              __sync_add_and_fetch(&x, 1);
              int y = __sync_sub_and_fetch(&x, 1);
              return 0;
            }
          " HAVE_IA64_ATOMIC)
        endif()
      endif()
    endif()
  endif()
endif()

# Determine which kind of byte swap functions your compiler supports.

check_cxx_source_compiles("
  #include <boost/endian/conversion.hpp>
  int main() {
    boost::endian::endian_reverse(static_cast<uint16_t>(1));
    boost::endian::endian_reverse(static_cast<uint32_t>(1));
    boost::endian::endian_reverse(static_cast<uint64_t>(1));
    return 0;
  }
" HAVE_BOOST_BYTESWAP)

if(NOT HAVE_BOOST_BYTESWAP)
  check_cxx_source_compiles("
    int main() {
      __builtin_bswap16(0);
      __builtin_bswap32(0);
      __builtin_bswap64(0);
      return 0;
    }
  " HAVE_GCC_BYTESWAP)

  if(NOT HAVE_GCC_BYTESWAP)
    check_cxx_source_compiles("
      #include <byteswap.h>
      int main() {
        __bswap_16(0);
        __bswap_32(0);
        __bswap_64(0);
        return 0;
      }
    " HAVE_GLIBC_BYTESWAP)

    if(NOT HAVE_GLIBC_BYTESWAP)
      check_cxx_source_compiles("
        #include <stdlib.h>
        int main() {
          _byteswap_ushort(0);
          _byteswap_ulong(0);
          _byteswap_uint64(0);
          return 0;
        }
      " HAVE_MSC_BYTESWAP)

      if(NOT HAVE_MSC_BYTESWAP)
        check_cxx_source_compiles("
          #include <libkern/OSByteOrder.h>
          int main() {
            OSSwapInt16(0);
            OSSwapInt32(0);
            OSSwapInt64(0);
            return 0;
          }
        " HAVE_MAC_BYTESWAP)

        if(NOT HAVE_MAC_BYTESWAP)
          check_cxx_source_compiles("
            #include <sys/endian.h>
            int main() {
              swap16(0);
              swap32(0);
              swap64(0);
              return 0;
            }
          " HAVE_OPENBSD_BYTESWAP)
        endif()
      endif()
    endif()
  endif()
endif()

# Determine whether your compiler supports some safer version of vsprintf.

check_cxx_source_compiles("
  #include <cstdio>
  #include <cstdarg>
  int main() {
    char buf[20];
    va_list args;
    vsnprintf(buf, 20, \"%d\", args);
    return 0;
  }
" HAVE_VSNPRINTF)

if(NOT HAVE_VSNPRINTF)
  check_cxx_source_compiles("
    #include <cstdio>
    #include <cstdarg>
    int main() {
      char buf[20];
      va_list args;
      vsprintf_s(buf, \"%d\", args);
      return 0;
    }
  " HAVE_VSPRINTF_S)
endif()

# Determine whether your compiler supports ISO _strdup.

check_cxx_source_compiles("
  #include <cstring>
  int main() {
    _strdup(0);
    return 0;
  }
" HAVE_ISO_STRDUP)

# Determine whether zlib is installed.

if(NOT ZLIB_SOURCE)
  find_package(ZLIB)
  if(ZLIB_FOUND)
    set(HAVE_ZLIB 1)
  else()
    set(HAVE_ZLIB 0)
  endif()

  if(NOT HAVE_ZLIB)
    find_package(Boost COMPONENTS iostreams zlib)
    if(Boost_IOSTREAMS_FOUND AND Boost_ZLIB_FOUND)
      set(HAVE_BOOST_ZLIB 1)
    else()
      set(HAVE_BOOST_ZLIB 0)
    endif()
  endif()
endif()

# Determine whether CppUnit is installed.

if(BUILD_TESTS AND NOT BUILD_SHARED_LIBS)
  find_package(CppUnit)
  if(NOT CppUnit_FOUND)
    message(STATUS "CppUnit not found, disabling tests.")
    set(BUILD_TESTS OFF)
  endif()
endif()

