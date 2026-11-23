# broken output contracts must abort before destroying the still-owned instance
# or unloading its image. fixture diagnostics expose premature cleanup.
include("${CMAKE_CURRENT_LIST_DIR}/check-fatal-result.cmake")
set(ENV{MUSIKCORE_TEST_EXPECT_ABORT} 1)
execute_process(COMMAND "${HELPER}" "--fatal-${MODE}" "${PLUGIN}"
  RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE diagnostic TIMEOUT 10)
check_fatal_result("${result}" "${diagnostic}")

# each child must reach the invariant it deliberately breaks. a later stop or
# destructor failure would otherwise mask an accepted stale or duplicate token.
if(MODE STREQUAL "stop")
  set(expected "output Stop failed")
elseif(MODE STREQUAL "duplicate" OR MODE STREQUAL "stale")
  set(expected "output returned an unknown or already completed buffer")
else()
  message(FATAL_ERROR "unknown output failure mode: ${MODE}")
endif()
if(NOT diagnostic MATCHES "${expected}")
  message(FATAL_ERROR "expected ${MODE} to abort with '${expected}'\n${diagnostic}")
endif()
