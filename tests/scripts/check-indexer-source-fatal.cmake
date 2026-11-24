# require a real abort and a diagnostic identifying the failed indexer source.
execute_process(
  COMMAND "${NODE}" "${RUNNER}" "${ROOT}" "${TEST_BINARY}"
    --expect-abort --stage --plugin "${PLUGIN}" create-fail
  RESULT_VARIABLE result
  OUTPUT_VARIABLE output
  ERROR_VARIABLE diagnostic)
if(NOT result EQUAL 0 OR NOT diagnostic MATCHES "fatal contract violation.*could not start indexer source")
  message(FATAL_ERROR "expected a fatal indexer source diagnostic: ${result}\n${output}\n${diagnostic}")
endif()
