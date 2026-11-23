include("${CMAKE_CURRENT_LIST_DIR}/check-fatal-result.cmake")

# successful assertions must neither terminate nor emit diagnostic output.
execute_process(COMMAND "${HELPER}" RESULT_VARIABLE result
  OUTPUT_VARIABLE output ERROR_VARIABLE diagnostic TIMEOUT 5)
if(NOT "${result}" STREQUAL "0" OR NOT "${output}${diagnostic}" STREQUAL "")
  message(FATAL_ERROR "successful assertion had side effects: ${result}\n${output}\n${diagnostic}")
endif()

# test the ordinary failure and a missing diagnostic independently, including
# caller location rather than the line inside the helper implementation.
foreach(mode IN ITEMS failure null)
  execute_process(COMMAND "${HELPER}" "${mode}" RESULT_VARIABLE result
    OUTPUT_VARIABLE output ERROR_VARIABLE diagnostic TIMEOUT 5)
  check_fatal_result("${result}" "${diagnostic}")
  string(REGEX MATCH "expected-line=([0-9]+)" expected "${diagnostic}")
  if(NOT expected OR NOT diagnostic MATCHES "contract.cpp:${CMAKE_MATCH_1} [(]"
      OR NOT diagnostic MATCHES "failContract")
    message(FATAL_ERROR "assertion did not identify its caller\n${diagnostic}")
  endif()
  if(mode STREQUAL "null" AND NOT diagnostic MATCHES "unspecified invariant")
    message(FATAL_ERROR "missing diagnostic was not handled safely\n${diagnostic}")
  endif()
endforeach()
