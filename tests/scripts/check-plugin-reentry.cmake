# the child's terminate handler verifies its active DLL is still mapped. a
# returned error (79) or an already-unloaded image (78) violates this contract.
execute_process(COMMAND "${HELPER}" "--fatal-${MODE}" "${PLUGIN}"
  RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE diagnostic TIMEOUT 10)
if(NOT "${result}" STREQUAL "77")
  message(FATAL_ERROR "${MODE}: expected fatal-before-unload (77), got ${result}\n${output}\n${diagnostic}")
endif()
if(NOT diagnostic MATCHES "fatal contract violation")
  message(FATAL_ERROR "${MODE}: missing fatal contract diagnostic\n${diagnostic}")
endif()
