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

run("${CMAKE_COMMAND}" --install "${BUILD_DIR}" --prefix "${installed}" --component MusikcoreSDK)
if(INSTALLED_TEST_PLUGIN)
  run("${CMAKE_COMMAND}" --install "${BUILD_DIR}" --prefix "${installed}" --component MusikcorePlugins)
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

if(INSTALLED_TEST_PLUGIN)
  # Discover only the installed headless plugin. Optional servers must stay off.
  set(plugins "${relocated}/share/musikcube/managed-test-plugins")
  file(MAKE_DIRECTORY "${plugins}")
  file(RENAME "${relocated}/share/musikcube/plugins/${INSTALLED_TEST_PLUGIN}" "${plugins}/${INSTALLED_TEST_PLUGIN}")
  file(MAKE_DIRECTORY "${root}/config")
  run("${CMAKE_COMMAND}" -E env "HOME=${root}/config" "XDG_CONFIG_HOME=${root}/config"
    "${root}/consumer/managed" "${plugins}")
endif()

file(REMOVE_RECURSE "${root}")
