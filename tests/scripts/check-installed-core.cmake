cmake_minimum_required(VERSION 3.16)

# Relocate the prefix before compiling to catch absolute package metadata.
string(RANDOM LENGTH 12 suffix)
set(root "${BUILD_DIR}/tests/bin/sdk-install-${suffix}")
set(installed "${root}/original")
set(relocated "${root}/relocated")
file(MAKE_DIRECTORY "${root}")

function(run)
  execute_process(COMMAND ${ARGV} RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE error)
  if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Installed SDK check failed at ${root}: ${ARGV}\n${output}\n${error}")
  endif()
  set(last_output "${output}" PARENT_SCOPE)
endfunction()

run("${CMAKE_COMMAND}" --install "${BUILD_DIR}" --prefix "${installed}" --component musikcoreSDK)
if(INSTALLED_TEST_PLUGIN)
  run("${CMAKE_COMMAND}" --install "${BUILD_DIR}" --prefix "${installed}" --component musikcorePlugins)
endif()
file(RENAME "${installed}" "${relocated}")
run("${CMAKE_COMMAND}" -S "${SOURCE}/tests/integration/binary/consumer" -B "${root}/consumer"
  "-DCMAKE_PREFIX_PATH=${relocated}" "-DEXAMPLES=${SOURCE}/examples" "-DSUPPORT=${SOURCE}/tests/support")
run("${CMAKE_COMMAND}" --build "${root}/consumer")

foreach(language IN ITEMS c cpp)
  run("${root}/consumer/query_${language}" "${CATALOG}")
  if(NOT last_output MATCHES "Morning Light")
    message(FATAL_ERROR "Installed ${language} consumer did not read the fixture catalog at ${root}\n${last_output}")
  endif()
endforeach()

file(MAKE_DIRECTORY "${root}/config")
run("${CMAKE_COMMAND}" -E env "HOME=${root}/config" "XDG_CONFIG_HOME=${root}/config"
  "${NODE}" "${CMAKE_CURRENT_LIST_DIR}/run-installed-session.js"
  "${REMOTE_TEST}" "${root}/consumer/session" "${CATALOG}")

# use an empty isolated directory even in builds without an output provider.
set(plugins "${relocated}/share/musikcube/managed-test-plugins")
file(MAKE_DIRECTORY "${plugins}")
if(INSTALLED_TEST_PLUGIN)
  # Discover only the installed headless plugin. Optional servers must stay off.
  file(RENAME "${relocated}/share/musikcube/plugins/${INSTALLED_TEST_PLUGIN}" "${plugins}/${INSTALLED_TEST_PLUGIN}")
  run("${CMAKE_COMMAND}" -E env "HOME=${root}/config" "XDG_CONFIG_HOME=${root}/config"
    "${root}/consumer/managed" "${plugins}")
endif()

# share the indexed fixture, but isolate preferences and load only null output.
get_filename_component(config_directory "${CATALOG}/../.." ABSOLUTE)
foreach(language IN ITEMS c cpp)
  run("${CMAKE_COMMAND}" -E env "HOME=${config_directory}" "XDG_CONFIG_HOME=${config_directory}"
    "${root}/consumer/engine_${language}" "${plugins}")
  if(NOT last_output MATCHES "Morning Light")
    message(FATAL_ERROR "Installed engine ${language} did not read the configured catalog at ${root}\n${last_output}")
  endif()
endforeach()

# play generated media using only the installed decoder and headless output.
file(GLOB installed_decoders "${relocated}/share/musikcube/plugins/*ffmpegdecoder*")
file(COPY ${installed_decoders} DESTINATION "${plugins}")
foreach(language IN ITEMS c cpp)
  run("${CMAKE_COMMAND}" -E env "XDG_CONFIG_HOME=${root}/player-${language}"
    "${root}/consumer/player_${language}" "${config_directory}/media/01-morning.wav"
    "${plugins}")
endforeach()

file(REMOVE_RECURSE "${root}")
