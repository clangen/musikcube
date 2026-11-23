# entry/initialization failures remain recoverable; a throwing shutdown must
# abort before user logging, stack cleanup, or native image unload.
include("${CMAKE_CURRENT_LIST_DIR}/check-fatal-result.cmake")
set(ENV{MUSIKCORE_TEST_EXPECT_ABORT} 1)
execute_process(COMMAND "${HELPER}" "--exception-${STAGE}" "${PLUGIN}"
  RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE diagnostic TIMEOUT 10)
if(STAGE STREQUAL "shutdown")
  check_fatal_result("${result}" "${diagnostic}")
elseif(NOT "${result}" STREQUAL "0")
  message(FATAL_ERROR "${STAGE} exception: expected recovery, got ${result}\n${output}\n${diagnostic}")
endif()
