# only the first metadata-detachment hook may run. fixture markers are written
# by ordinary calls, not by code executing after a fatal ownership violation.
execute_process(COMMAND "${NODE}" "${SOURCE}/tests/scripts/run-isolated-test.js" "${ROOT}" "${HELPER}"
  --expect-abort --stage --plugin "${FIRST}" --plugin "${SECOND}" failure
  RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE diagnostic TIMEOUT 25)
string(REGEX MATCHALL "fatal-test: cleanup-bit=[0-9]+" markers "${diagnostic}")
list(LENGTH markers count)
if(NOT "${result}" STREQUAL "0" OR NOT diagnostic MATCHES "fatal contract violation at [^\n]+:[0-9]+"
    OR NOT count EQUAL 1 OR NOT "${markers}" STREQUAL "fatal-test: cleanup-bit=1")
  message(FATAL_ERROR "expected abort at first host detachment, got ${result}\n${output}\n${diagnostic}")
endif()
