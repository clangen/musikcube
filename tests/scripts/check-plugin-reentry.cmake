# reentry must abort at its guard before stack cleanup or native image unload.
include("${CMAKE_CURRENT_LIST_DIR}/check-fatal-result.cmake")
set(ENV{MUSIKCORE_TEST_EXPECT_ABORT} 1)
execute_process(COMMAND "${HELPER}" "--fatal-${MODE}" "${PLUGIN}"
  RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE diagnostic TIMEOUT 10)
check_fatal_result("${result}" "${diagnostic}")
