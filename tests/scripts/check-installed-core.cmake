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

file(REMOVE_RECURSE "${root}")
