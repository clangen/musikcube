cmake_minimum_required(VERSION 3.16)

# compile a separate C project using only relocated installed headers. keep the
# prefix/build on failure so diagnostics never depend on deleted artifacts.
string(RANDOM LENGTH 12 suffix)
set(root "${BUILD_DIR}/tests/bin/provider-install-${suffix}")
set(original "${root}/original")
set(relocated "${root}/relocated SDK")

function(run)
  execute_process(COMMAND ${ARGV} RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE error)
  if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Installed provider check failed at ${root}: ${ARGV}\n${output}\n${error}")
  endif()
endfunction()

set(config_arguments)
if(TEST_CONFIG)
  set(config_arguments --config "${TEST_CONFIG}")
endif()
run("${CMAKE_COMMAND}" --install "${BUILD_DIR}" ${config_arguments}
  --prefix "${original}" --component musikcoreSDK)
file(RENAME "${original}" "${relocated}")
set(external "${root}/external provider")
set(generator)
if(WIN32)
  set(generator -G "${TEST_GENERATOR}" -A "${TEST_PLATFORM}")
endif()
run("${CMAKE_COMMAND}" -S "${SOURCE}/tests/support/plugins" -B "${external}"
  ${generator} "-DCMAKE_PREFIX_PATH=${relocated}" "-DCMAKE_BUILD_TYPE=${TEST_CONFIG}")
run("${CMAKE_COMMAND}" --build "${external}" ${config_arguments})
file(READ "${external}/artifact-${TEST_CONFIG}.txt" fixture)

if(UNIX)
  # no C++ or private core symbols may leak out of the separately built plugin.
  file(WRITE "${root}/exports.txt" "mc_plugin_get_api\n")
  run("${CMAKE_COMMAND}" "-DNM=${NM}" "-DAPPLE=${APPLE}" "-DLIBRARY=${fixture}"
    "-DALLOWLIST=${root}/exports.txt" -P "${SOURCE}/tests/scripts/check-exports.cmake")
  execute_process(COMMAND "${NM}" -u "${fixture}" RESULT_VARIABLE status OUTPUT_VARIABLE imports)
  if(NOT status EQUAL 0 OR imports MATCHES "[_ ]mc_[a-z_]+")
    message(FATAL_ERROR "Provider must use host tables rather than import musikcore: ${imports}")
  endif()
endif()
run("${HOST}" "${fixture}")
file(REMOVE_RECURSE "${root}")
