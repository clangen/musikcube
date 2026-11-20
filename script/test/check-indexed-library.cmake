# Index generated media so these cases cover production tag readers and schema,
# rather than only the small hand-written databases in API tests.
execute_process(COMMAND "${NODE}" "${SEED_SCRIPT}" --indexer "${INDEXER}" "${BUILD_DIR}"
  RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE error
  OUTPUT_STRIP_TRAILING_WHITESPACE)
if(NOT status STREQUAL "0")
  message(FATAL_ERROR "Could not seed the test library (${status})\n${output}\n${error}")
endif()

string(REGEX MATCH "[^\r\n]+$" database "${output}")
if(NOT EXISTS "${database}")
  message(FATAL_ERROR "Fixture database missing: ${database}")
endif()
get_filename_component(directory "${database}" DIRECTORY)
# Both documented golden paths must work using the actual shared C boundary.
foreach(example IN ITEMS C_EXAMPLE CPP_EXAMPLE)
  execute_process(COMMAND "${${example}}" "${directory}" RESULT_VARIABLE status)
  if(NOT status STREQUAL "0")
    message(FATAL_ERROR "${example} failed (${status})")
  endif()
endforeach()
# Reuse the catalog to compare C and C++ readers of real indexed metadata.
execute_process(COMMAND "${QUERY_TEST}" --indexed "${directory}" RESULT_VARIABLE status)
if(NOT status STREQUAL "0")
  message(FATAL_ERROR "Indexed library query failed (${status})")
endif()
execute_process(COMMAND "${CPP_TEST}" "${directory}" RESULT_VARIABLE status)
if(NOT status STREQUAL "0")
  message(FATAL_ERROR "C++ wrapper query failed (${status})")
endif()
# Run production services in a separate process to avoid loading both core
# implementations together while comparing their public behavior.
get_filename_component(config_directory "${directory}/../.." ABSOLUTE)
execute_process(COMMAND "${NODE}" "${ISOLATED_RUNNER}"
  "${BUILD_DIR}/tests/bin/isolated" "${PRODUCTION_TEST}" --stage
  --config-root "${config_directory}" "${directory}" RESULT_VARIABLE status)
if(NOT status STREQUAL "0")
  message(FATAL_ERROR "C++ wrapper query through production musikcore failed (${status})")
endif()
execute_process(COMMAND "${NODE}" "${ISOLATED_RUNNER}"
  "${BUILD_DIR}/tests/bin/isolated" "${REMOTE_TEST}" --stage
  --config-root "${config_directory}" --parity RESULT_VARIABLE status)
if(NOT status STREQUAL "0")
  message(FATAL_ERROR "Local/loopback core API parity failed (${status})")
endif()
