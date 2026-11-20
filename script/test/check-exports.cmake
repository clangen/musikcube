# Inspect the actual binary to catch C++ symbols leaked by linker visibility,
# even when public headers and the export allowlist still match.
if(APPLE)
  set(args -gUj)
else()
  set(args -D --defined-only --format=posix)
endif()
execute_process(COMMAND "${NM}" ${args} "${LIBRARY}"
  RESULT_VARIABLE status OUTPUT_VARIABLE listing ERROR_VARIABLE error)
if(NOT status STREQUAL "0")
  message(FATAL_ERROR "Cannot inspect exports from ${LIBRARY}: ${status}\n${error}")
endif()

# Normalize platform spelling before comparing sets: Mach-O adds a leading
# underscore, while ELF's POSIX listing includes columns after each symbol.
string(REGEX MATCHALL "[^\r\n]+" lines "${listing}")
foreach(line IN LISTS lines)
  if(APPLE)
    string(REGEX REPLACE "^_" "" symbol "${line}")
  else()
    string(REGEX MATCH "^[^ ]+" symbol "${line}")
  endif()
  list(APPEND actual "${symbol}")
endforeach()
file(STRINGS "${ALLOWLIST}" expected)
list(SORT actual)
list(SORT expected)
if(NOT "${actual}" STREQUAL "${expected}")
  message(FATAL_ERROR "ABI exports differ from allowlist\nExpected: ${expected}\nActual: ${actual}")
endif()
