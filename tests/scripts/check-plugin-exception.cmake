# run fatal cleanup in a child so it cannot terminate the test runner. the
# helper's terminate handler verifies the DLL is still mapped before exiting.
execute_process(COMMAND "${HELPER}" "--exception-${STAGE}" "${PLUGIN}"
  RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 10)
set(expected 0)
if(STAGE STREQUAL "shutdown")
  set(expected 77)
endif()
if(NOT "${result}" STREQUAL "${expected}")
  message(FATAL_ERROR "${STAGE} exception: expected exit ${expected}, got ${result}\n${output}\n${diagnostics}")
endif()
