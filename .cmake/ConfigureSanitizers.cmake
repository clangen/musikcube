# Configure before any targets: ABI-only libraries and early test fixtures must
# receive the same Debug instrumentation as production C and C++ code.
set(sanitizers_default "")
if(UNIX AND CMAKE_C_COMPILER_ID MATCHES "^(GNU|Clang|AppleClang)$" AND
    CMAKE_CXX_COMPILER_ID MATCHES "^(GNU|Clang|AppleClang)$")
  set(sanitizers_default "address,undefined")
endif()
set(MUSIKCUBE_SANITIZERS "${sanitizers_default}" CACHE STRING
  "Debug sanitizers: address, undefined, address,undefined, or none; Release is unaffected")
set_property(CACHE MUSIKCUBE_SANITIZERS PROPERTY STRINGS "address,undefined" address undefined none)

string(TOLOWER "${MUSIKCUBE_SANITIZERS}" sanitizers_selected)
string(REPLACE "," ";" MUSIKCUBE_SANITIZERS_LIST "${sanitizers_selected}")
if(sanitizers_selected STREQUAL "none" OR sanitizers_selected STREQUAL "")
  set(MUSIKCUBE_SANITIZERS_LIST)
endif()

if(MUSIKCUBE_SANITIZERS_LIST)
  if(NOT UNIX OR NOT CMAKE_C_COMPILER_ID MATCHES "^(GNU|Clang|AppleClang)$" OR
      NOT CMAKE_CXX_COMPILER_ID MATCHES "^(GNU|Clang|AppleClang)$")
    message(FATAL_ERROR "Debug sanitizers currently require a Unix Clang or GCC C/C++ toolchain")
  endif()
  foreach(sanitizer IN LISTS MUSIKCUBE_SANITIZERS_LIST)
    if(NOT sanitizer STREQUAL "address" AND NOT sanitizer STREQUAL "undefined")
      message(FATAL_ERROR "Unsupported sanitizer '${sanitizer}'; choose address, undefined, address,undefined, or none")
    endif()
  endforeach()
  list(REMOVE_DUPLICATES MUSIKCUBE_SANITIZERS_LIST)
  string(REPLACE ";" "," sanitizers_flags "${MUSIKCUBE_SANITIZERS_LIST}")

  add_compile_options("$<$<CONFIG:Debug>:-fsanitize=${sanitizers_flags}>"
    "$<$<CONFIG:Debug>:-fno-omit-frame-pointer>"
    "$<$<CONFIG:Debug>:-fno-sanitize-recover=all>")
  # Driver link flags cover C-only hosts as well as shared/module libraries.
  # Keep these config-specific so Release never inherits Debug instrumentation.
  foreach(kind IN ITEMS EXE SHARED MODULE)
    string(APPEND CMAKE_${kind}_LINKER_FLAGS_DEBUG " -fsanitize=${sanitizers_flags}")
  endforeach()
endif()

# AppleClang 21 leaves image-global poison behind on dlclose. Apply the narrow
# cleanup to every instrumented shared/module target, including test fixtures.
function(musikcube_finalize_sanitizers directory)
  if((NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE STREQUAL "Debug") OR
      NOT "address" IN_LIST MUSIKCUBE_SANITIZERS_LIST OR NOT APPLE OR
      NOT CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang" OR
      CMAKE_CXX_COMPILER_VERSION VERSION_LESS 21 OR
      NOT CMAKE_CXX_COMPILER_VERSION VERSION_LESS 22)
    return()
  endif()
  if(NOT TARGET musikcube_asan_unload)
    add_library(musikcube_asan_unload OBJECT EXCLUDE_FROM_ALL
      "${PROJECT_SOURCE_DIR}/.cmake/AppleClangAsanUnload.cpp")
    set_target_properties(musikcube_asan_unload PROPERTIES POSITION_INDEPENDENT_CODE ON
      DISABLE_PRECOMPILE_HEADERS ON)
    target_compile_options(musikcube_asan_unload PRIVATE -fno-sanitize=address)
  endif()
  get_property(targets DIRECTORY "${directory}" PROPERTY BUILDSYSTEM_TARGETS)
  foreach(target IN LISTS targets)
    get_target_property(kind "${target}" TYPE)
    if(kind STREQUAL "SHARED_LIBRARY" OR kind STREQUAL "MODULE_LIBRARY")
      target_sources(${target} PRIVATE "$<$<CONFIG:Debug>:$<TARGET_OBJECTS:musikcube_asan_unload>>")
      set_target_properties(${target} PROPERTIES LINKER_LANGUAGE CXX)
    endif()
  endforeach()
  get_property(children DIRECTORY "${directory}" PROPERTY SUBDIRECTORIES)
  foreach(child IN LISTS children)
    musikcube_finalize_sanitizers("${child}")
  endforeach()
endfunction()
