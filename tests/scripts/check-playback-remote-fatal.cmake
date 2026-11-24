# the plugin publishes an uncertain instance before throwing across the C ABI.
execute_process(
  COMMAND "${NODE}" "${RUNNER}" "${ROOT}" "${TEST_BINARY}" --expect-abort --stage
    --plugin "${PLUGIN}" --plugin "${OUTPUT}" "${MODE}"
  RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE diagnostic)
if(NOT result EQUAL 0 OR NOT diagnostic MATCHES "playback remote (creation|start) threw" OR diagnostic MATCHES "unsafe-(destroy|stop)")
  message(FATAL_ERROR "expected fatal creation error before destruction: ${result}\n${output}\n${diagnostic}")
endif()
