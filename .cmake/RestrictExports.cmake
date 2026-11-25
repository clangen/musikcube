#[[
Keep C++ implementation and runtime symbols private across Unix binaries.
Zig standalone binaries can carry separate static libc++ copies. Exporting
their symbols lets another binary bind to the wrong runtime state.
]]
function(musikcube_restrict_exports target)
  if(NOT UNIX)
    return()
  endif()

  # FreeBSD libc expects symbols supplied by the executable, including
  # environ and __progname, to remain visible to the dynamic linker.
  if(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
    get_target_property(kind ${target} TYPE)
    if(kind STREQUAL "EXECUTABLE")
      return()
    endif()
  endif()

  # Derive the linker rules and the binary audit from the same symbol list.
  set(exports ${ARGN})
  set(allowlist "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/${target}.exports.txt")
  string(REPLACE ";" "\n" lines "${exports}")
  file(GENERATE OUTPUT "${allowlist}" CONTENT "${lines}\n")
  set_property(TARGET ${target} PROPERTY MUSIKCUBE_EXPORT_ALLOWLIST "${allowlist}")

  set_property(GLOBAL APPEND PROPERTY MUSIKCUBE_RESTRICTED_TARGETS "${target}")

  if(APPLE AND NOT exports)
    # The executables have no entry points for plugins to resolve.
    set_property(TARGET ${target} APPEND_STRING PROPERTY LINK_FLAGS " -Wl,-no_exported_symbols")
    return()
  endif()

  set(linker_file "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/${target}.exports")
  if(APPLE)
    string(REPLACE ";" "\n_" symbols "_${exports}")
    file(GENERATE OUTPUT "${linker_file}" CONTENT "${symbols}\n")
    set(option "-Wl,-exported_symbols_list,${linker_file}")
  else()
    get_target_property(kind ${target} TYPE)
    if(kind STREQUAL "EXECUTABLE")
      # GNU linkers can expose imported data via copy relocations even with an empty export map.
      # Full PIC uses indirect references to that data.
      target_compile_options(${target} PRIVATE -fPIC)
    endif()

    set(global "")
    if(exports)
      string(REPLACE ";" ";\n    " symbols "${exports}")
      set(global "  global:\n    ${symbols};\n")
    endif()
    file(GENERATE OUTPUT "${linker_file}" CONTENT "{\n${global}  local: *;\n};\n")
    set(option "-Wl,--version-script,${linker_file}")
  endif()
  set_property(TARGET ${target} APPEND_STRING PROPERTY LINK_FLAGS " \"${option}\"")
  # Changing the list must relink the binary even when no sources changed.
  set_property(TARGET ${target} APPEND PROPERTY LINK_DEPENDS "${linker_file}")
endfunction()
