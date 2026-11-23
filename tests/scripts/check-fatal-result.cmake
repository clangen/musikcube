# fatal tests must observe an actual abort, not a status chosen by a terminate
# handler. FatalScope disables Windows crash reporting so CRT abort exits
# with code 3. Unix reports SIGABRT to CMake.
function(check_fatal_result result diagnostic)
  if(NOT "${result}" STREQUAL "Subprocess aborted" AND NOT (WIN32 AND "${result}" STREQUAL "3"))
    message(FATAL_ERROR "expected abort, got ${result}\n${diagnostic}")
  endif()
  if(NOT diagnostic MATCHES "fatal contract violation at [^\n]+:[0-9]+")
    message(FATAL_ERROR "missing fatal diagnostic/caller location\n${diagnostic}")
  endif()
  if(diagnostic MATCHES "fatal-test: (stack unwound|image unloaded|output destroyed|unexpected logger)")
    message(FATAL_ERROR "unsafe work occurred before fatal termination\n${diagnostic}")
  endif()
endfunction()
