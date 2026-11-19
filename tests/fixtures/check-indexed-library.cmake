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
execute_process(COMMAND "${QUERY_TEST}" --indexed "${directory}" RESULT_VARIABLE status)
if(NOT status STREQUAL "0")
  message(FATAL_ERROR "Indexed library query failed (${status})")
endif()
execute_process(COMMAND "${CPP_TEST}" "${directory}" RESULT_VARIABLE status)
if(NOT status STREQUAL "0")
  message(FATAL_ERROR "C++ wrapper query failed (${status})")
endif()
get_filename_component(config_directory "${directory}/../.." ABSOLUTE)
execute_process(COMMAND "${CMAKE_COMMAND}" -E env
  "XDG_CONFIG_HOME=${config_directory}" "APPDATA=${config_directory}"
  "${PRODUCTION_TEST}" "${directory}" RESULT_VARIABLE status)
if(NOT status STREQUAL "0")
  message(FATAL_ERROR "C++ wrapper query through production musikcore failed (${status})")
endif()
