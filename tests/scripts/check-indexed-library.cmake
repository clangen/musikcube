# Index generated media so these cases cover production tag readers and schema,
# rather than only the small hand-written databases in API tests.
if(DEFINED HTTP_DATASTREAM_PLUGIN)
  set(seed_duration --seconds 4)
endif()
execute_process(COMMAND "${NODE}" "${SEED_SCRIPT}" ${seed_duration} --indexer "${INDEXER}" "${BUILD_DIR}"
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
get_filename_component(config_directory "${directory}/../.." ABSOLUTE)

# Each selected case gets its own catalog and process so an earlier assertion
# cannot hide a later contract or leak preferences into another test.
if(DEFINED INSTALLED_SDK_SOURCE)
  execute_process(COMMAND "${CMAKE_COMMAND}" "-DBUILD_DIR=${BUILD_DIR}" "-DSOURCE=${INSTALLED_SDK_SOURCE}"
    "-DCATALOG=${directory}" "-DINSTALLED_TEST_PLUGIN=${INSTALLED_TEST_PLUGIN}" -P "${CMAKE_CURRENT_LIST_DIR}/check-installed-core.cmake"
    RESULT_VARIABLE status)
  if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Installed SDK test failed (${status}); fixture: ${directory}")
  endif()
elseif(DEFINED MANAGED_RUNTIME_TEST)
  if(DEFINED EQUALIZER_PLUGIN)
    set(managed_runtime_plugins --plugin "${EQUALIZER_PLUGIN}")
    set(managed_runtime_options --check-equalizer)
  endif()
  if(DEFINED SERVER_PLUGIN)
    list(APPEND managed_runtime_plugins --plugin "${SERVER_PLUGIN}")
    list(APPEND managed_runtime_options --check-server)
  endif()
  if(DEFINED HTTP_DATASTREAM_PLUGIN)
    list(APPEND managed_runtime_plugins --plugin "${HTTP_DATASTREAM_PLUGIN}")
    list(APPEND managed_runtime_options --check-remote-audio)
  endif()
  execute_process(COMMAND "${NODE}" "${ISOLATED_RUNNER}"
    "${BUILD_DIR}/tests/bin/isolated" "${MANAGED_RUNTIME_TEST}" --stage
    --locale "${CMAKE_CURRENT_LIST_DIR}/../../src/musikcube/data/locales/en_US.json"
    --headless-output "${NULL_OUTPUT}" ${managed_runtime_plugins}
    --config-root "${config_directory}" ${managed_runtime_options}
    RESULT_VARIABLE status)
  if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Managed runtime test failed (${status}); fixture: ${directory}")
  endif()
elseif(DEFINED PLUGIN_PLAYBACK_TEST)
  execute_process(COMMAND "${NODE}" "${ISOLATED_RUNNER}"
    "${BUILD_DIR}/tests/bin/isolated" "${PLUGIN_PLAYBACK_TEST}" --stage
    --headless-output "${NULL_OUTPUT}" "${config_directory}/media/01-morning.wav"
    RESULT_VARIABLE status)
  if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Plugin playback failed (${status}); fixture: ${directory}")
  endif()
elseif(DEFINED WRAPPER_CASE)
  if(WRAPPER_PRODUCTION)
    execute_process(COMMAND "${NODE}" "${ISOLATED_RUNNER}"
      "${BUILD_DIR}/tests/bin/isolated" "${PRODUCTION_TEST}" --stage
      --config-root "${config_directory}" "${directory}" --case "${WRAPPER_CASE}"
      RESULT_VARIABLE status)
  else()
    execute_process(COMMAND "${CPP_TEST}" "${directory}" --case "${WRAPPER_CASE}"
      RESULT_VARIABLE status)
  endif()
  if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Wrapper case ${WRAPPER_CASE} failed (${status}); fixture: ${directory}")
  endif()
elseif(BACKEND_PARITY)
  execute_process(COMMAND "${NODE}" "${ISOLATED_RUNNER}"
    "${BUILD_DIR}/tests/bin/isolated" "${REMOTE_TEST}" --stage
    --config-root "${config_directory}" --parity "${database}" RESULT_VARIABLE status)
  if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Local/loopback parity failed (${status}); fixture: ${directory}")
  endif()
else()
  # Both golden paths cross the real shared boundary against indexed media.
  foreach(example IN ITEMS C_EXAMPLE CPP_EXAMPLE)
    execute_process(COMMAND "${${example}}" "${directory}" RESULT_VARIABLE status)
    if(NOT status STREQUAL "0")
      message(FATAL_ERROR "${example} failed (${status}); fixture: ${directory}")
    endif()
  endforeach()
  execute_process(COMMAND "${QUERY_TEST}" --indexed "${directory}" RESULT_VARIABLE status)
  if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Indexed C query failed (${status}); fixture: ${directory}")
  endif()
endif()
