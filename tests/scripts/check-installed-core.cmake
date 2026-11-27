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

set(config_argument)
if(TEST_CONFIG)
  set(config_argument --config "${TEST_CONFIG}")
endif()
# The ordinary full installation must not publish the retired C++ SDK.
set(full_install "${root}/full")
run("${CMAKE_COMMAND}" --install "${BUILD_DIR}" ${config_argument} --prefix "${full_install}")
if(EXISTS "${full_install}/include/musikcube/musikcore/sdk")
  message(FATAL_ERROR "Full installation contains the retired C++ SDK at ${root}")
endif()
run("${CMAKE_COMMAND}" --install "${BUILD_DIR}" ${config_argument} --prefix "${installed}" --component musikcoreSDK)
if(INSTALLED_TEST_PLUGIN)
  run("${CMAKE_COMMAND}" --install "${BUILD_DIR}" ${config_argument} --prefix "${installed}" --component musikcorePlugins)
endif()
file(RENAME "${installed}" "${relocated}")
set(consumer_bin "${root}/consumer")
set(runtime "${relocated}/share/musikcube")
set(exe)
set(consumer_generator)
if(WIN32)
  set(consumer_bin "${consumer_bin}/${TEST_CONFIG}")
  set(runtime "${relocated}/bin")
  set(exe ".exe")
  set(consumer_generator -G "Visual Studio 18 2026" -A x64 -T v145)
endif()
run("${CMAKE_COMMAND}" -S "${SOURCE}/tests/integration/binary/consumer" -B "${root}/consumer"
  ${consumer_generator}
  "-DCMAKE_BUILD_TYPE=${TEST_CONFIG}" "-DCONSUMER_SANITIZERS=${CONSUMER_SANITIZERS}"
  "-DCMAKE_PREFIX_PATH=${relocated}" "-DEXAMPLES=${SOURCE}/examples" "-DSUPPORT=${SOURCE}/tests/support")
run("${CMAKE_COMMAND}" --build "${root}/consumer" ${config_argument})
if(WIN32)
  # The loader resolves the installed DLL and its dependencies beside the consumer.
  file(GLOB installed_dlls "${runtime}/*.dll")
  file(COPY ${installed_dlls} DESTINATION "${consumer_bin}")
endif()

run("${consumer_bin}/fixed_queue${exe}")

# SDK installation must clean copied vendor libraries as well as CMake targets.
if(APPLE)
  run("${CMAKE_COMMAND}" "-DRUNTIME=${runtime}" -P "${SOURCE}/tests/scripts/check-macho-paths.cmake")
endif()

foreach(language IN ITEMS c cpp)
  run("${consumer_bin}/query_${language}${exe}" "${CATALOG}")
  if(NOT last_output MATCHES "Morning Light")
    message(FATAL_ERROR "Installed ${language} consumer did not read the fixture catalog at ${root}\n${last_output}")
  endif()
  run("${consumer_bin}/query_${language}${exe}" "${CATALOG}" "none")
  if(NOT last_output STREQUAL "")
    message(FATAL_ERROR "Installed ${language} consumer ignored empty scope at ${root}\n${last_output}")
  endif()
  run("${consumer_bin}/query_${language}${exe}" "${CATALOG}" "0")
  if(NOT last_output MATCHES "Morning Light")
    message(FATAL_ERROR "Installed ${language} consumer lost source zero at ${root}\n${last_output}")
  endif()
endforeach()

file(MAKE_DIRECTORY "${root}/config")
run("${CMAKE_COMMAND}" -E env "HOME=${root}/config" "XDG_CONFIG_HOME=${root}/config" "APPDATA=${root}/config"
  "${NODE}" "${CMAKE_CURRENT_LIST_DIR}/run-installed-session.js"
  "${REMOTE_TEST}" "${consumer_bin}/session${exe}" "${CATALOG}")

# use an empty isolated directory even in builds without an output provider.
set(plugins "${runtime}/managed-test-plugins")
file(MAKE_DIRECTORY "${plugins}")
if(INSTALLED_TEST_PLUGIN)
  # Discover only the installed headless plugin. Optional servers must stay off.
  file(RENAME "${runtime}/plugins/${INSTALLED_TEST_PLUGIN}" "${plugins}/${INSTALLED_TEST_PLUGIN}")
  run("${CMAKE_COMMAND}" -E env "HOME=${root}/config" "XDG_CONFIG_HOME=${root}/config" "APPDATA=${root}/config"
    "${consumer_bin}/managed${exe}" "${plugins}")
endif()

# share the indexed fixture, but isolate settings and load only null output.
get_filename_component(config_directory "${CATALOG}/../.." ABSOLUTE)
foreach(language IN ITEMS c cpp)
  run("${CMAKE_COMMAND}" -E env "HOME=${config_directory}" "XDG_CONFIG_HOME=${config_directory}" "APPDATA=${config_directory}"
    "${consumer_bin}/engine_${language}${exe}" "${plugins}")
  if(NOT last_output MATCHES "Morning Light")
    message(FATAL_ERROR "Installed engine ${language} did not read the configured catalog at ${root}\n${last_output}")
  endif()
endforeach()

# playback needs an output consumable by the engine. query/session consumers
# above remain useful while output integration is pending.
if(INSTALLED_TEST_PLUGIN)
  file(GLOB installed_decoders "${runtime}/plugins/*ffmpegdecoder*")
  file(COPY ${installed_decoders} DESTINATION "${plugins}")
  foreach(language IN ITEMS c cpp)
    run("${CMAKE_COMMAND}" -E env "XDG_CONFIG_HOME=${root}/player-${language}" "APPDATA=${root}/player-${language}"
      "${consumer_bin}/player_${language}${exe}" "${config_directory}/media/01-morning.wav"
      "${plugins}")
  endforeach()
else()
  message(STATUS "[plugin-cutover] installed C/C++ player subcases pending output integration")
endif()

file(REMOVE_RECURSE "${root}")
