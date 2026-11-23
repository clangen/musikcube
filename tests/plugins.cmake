# in-source builds must also keep generated DLLs outside the source fixtures.
# bin is the shared ignored test-output directory used by the other helpers.
set(provider_output_directory "${CMAKE_CURRENT_BINARY_DIR}/bin/fixtures/providers")

# provider fixtures compile as C and exchange only installed-shape function tables.
set(provider_variants valid incompatible newer_minor truncated missing_shutdown bad_id bad_utf8 entry_fail init_fail bad_capability no_entry duplicate_id missing_version bad_output)
set(provider_arguments)
set(provider_targets)
foreach(variant IN LISTS provider_variants)
  set(target "musikcore_provider_${variant}")
  add_library(${target} MODULE support/plugins/provider.c support/plugins/unload-probe.c)
  target_include_directories(${target} PRIVATE "${PROJECT_SOURCE_DIR}/include")
  if(NOT variant STREQUAL "no_entry")
    target_compile_definitions(${target} PRIVATE MC_BUILDING_PLUGIN)
  endif()
  if(NOT variant STREQUAL "valid")
    string(TOUPPER "${variant}" definition)
    target_compile_definitions(${target} PRIVATE ${definition})
  endif()
  set_target_properties(${target} PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF
    ARCHIVE_OUTPUT_DIRECTORY "${provider_output_directory}"
    LIBRARY_OUTPUT_DIRECTORY "${provider_output_directory}"
    RUNTIME_OUTPUT_DIRECTORY "${provider_output_directory}"
    SUFFIX "${CMAKE_SHARED_LIBRARY_SUFFIX}")
  if(variant STREQUAL "no_entry")
    musikcube_restrict_exports(${target} not_a_plugin)
  else()
    musikcube_restrict_exports(${target} mc_plugin_get_api)
  endif()
  list(APPEND provider_arguments "$<TARGET_FILE:${target}>")
  list(APPEND provider_targets ${target})
  add_dependencies(musikcube_tests ${target})
  add_dependencies(musikcube_integration_tests ${target})
endforeach()

# compile the same private registry/error implementation without exporting its C++ types.
add_executable(musikcore_plugin_registry_test integration/plugins/plugin-registry.cpp
  "${PROJECT_SOURCE_DIR}/src/musikcore/ffi/plugin_abi/PluginRegistry.cpp"
  "${PROJECT_SOURCE_DIR}/src/musikcore/ffi/plugin_abi/Plugin.cpp"
  "${PROJECT_SOURCE_DIR}/src/musikcore/ffi/common/error.cpp")
target_include_directories(musikcore_plugin_registry_test PRIVATE
  "${PROJECT_SOURCE_DIR}/include" "${PROJECT_SOURCE_DIR}/src")
target_compile_features(musikcore_plugin_registry_test PRIVATE cxx_std_20)
target_link_libraries(musikcore_plugin_registry_test PRIVATE Threads::Threads ${CMAKE_DL_LIBS})
set_target_properties(musikcore_plugin_registry_test PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin")
add_dependencies(musikcore_plugin_registry_test ${provider_targets})
add_dependencies(musikcube_tests musikcore_plugin_registry_test)
add_dependencies(musikcube_integration_tests musikcore_plugin_registry_test)
add_test(NAME musikcore.integration.plugins.lifecycle
  COMMAND musikcore_plugin_registry_test ${provider_arguments})
set_tests_properties(musikcore.integration.plugins.lifecycle PROPERTIES
  LABELS "integration;musikcore;plugins;abi;contract" TIMEOUT 30)

# exercise actual foreign callbacks in isolated processes; fatal assertions must
# fire before the running image is unloaded, including in Release builds.
foreach(mode IN ITEMS plugin-stop plugin-initialize plugin-identify plugin-shutdown)
  add_test(NAME musikcore.integration.plugins.reentry_${mode} COMMAND "${CMAKE_COMMAND}"
    "-DHELPER=$<TARGET_FILE:musikcore_plugin_registry_test>"
    "-DPLUGIN=$<TARGET_FILE:musikcore_provider_valid>" "-DMODE=${mode}"
    -P "${PROJECT_SOURCE_DIR}/tests/scripts/check-plugin-reentry.cmake")
  set_tests_properties(musikcore.integration.plugins.reentry_${mode} PROPERTIES
    LABELS "integration;musikcore;plugins;contract" TIMEOUT 15)
endforeach()

add_executable(musikcore_plugin_discovery_test integration/plugins/plugin-discovery.cpp
  "${PROJECT_SOURCE_DIR}/src/musikcore/ffi/plugin_abi/PluginRegistry.cpp"
  "${PROJECT_SOURCE_DIR}/src/musikcore/ffi/plugin_abi/Plugin.cpp"
  "${PROJECT_SOURCE_DIR}/src/musikcore/ffi/common/error.cpp")
target_include_directories(musikcore_plugin_discovery_test PRIVATE
  "${PROJECT_SOURCE_DIR}/include" "${PROJECT_SOURCE_DIR}/src")
target_compile_features(musikcore_plugin_discovery_test PRIVATE cxx_std_20)
target_link_libraries(musikcore_plugin_discovery_test PRIVATE Threads::Threads ${CMAKE_DL_LIBS})
set_target_properties(musikcore_plugin_discovery_test PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin")
add_dependencies(musikcore_plugin_discovery_test
  musikcore_provider_valid musikcore_provider_init_fail musikcore_provider_no_entry)
add_dependencies(musikcube_tests musikcore_plugin_discovery_test)
add_dependencies(musikcube_integration_tests musikcore_plugin_discovery_test)
add_test(NAME musikcore.integration.plugins.discovery
  COMMAND musikcore_plugin_discovery_test
    $<TARGET_FILE:musikcore_provider_valid>
    $<TARGET_FILE:musikcore_provider_init_fail>
    $<TARGET_FILE:musikcore_provider_no_entry>)
set_tests_properties(musikcore.integration.plugins.discovery PROPERTIES
  LABELS "integration;musikcore;plugins;abi;contract" TIMEOUT 30)

add_executable(musikcore_audio_buffers_test unit/musikcore/audio-buffers.cpp
  "${PROJECT_SOURCE_DIR}/src/musikcore/ffi/plugin_abi/AudioScratchBuffers.cpp"
  "${PROJECT_SOURCE_DIR}/src/musikcore/ffi/common/error.cpp")
target_include_directories(musikcore_audio_buffers_test PRIVATE
  "${PROJECT_SOURCE_DIR}/include" "${PROJECT_SOURCE_DIR}/src")
target_compile_features(musikcore_audio_buffers_test PRIVATE cxx_std_17)
set_target_properties(musikcore_audio_buffers_test PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin")
add_dependencies(musikcube_tests musikcore_audio_buffers_test)
add_dependencies(musikcube_unit_tests musikcore_audio_buffers_test)
add_test(NAME musikcore.unit.plugins.audio_buffers COMMAND musikcore_audio_buffers_test)
set_tests_properties(musikcore.unit.plugins.audio_buffers PROPERTIES
  LABELS "unit;musikcore;plugins;audio;contract")

add_executable(musikcore_plugin_contract_headers_test unit/musikcore/plugin-contract-headers.c)
target_include_directories(musikcore_plugin_contract_headers_test PRIVATE "${PROJECT_SOURCE_DIR}/include")
set_target_properties(musikcore_plugin_contract_headers_test PROPERTIES
  C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin")
add_dependencies(musikcube_tests musikcore_plugin_contract_headers_test)
add_dependencies(musikcube_unit_tests musikcore_plugin_contract_headers_test)
add_test(NAME musikcore.unit.plugins.contract_headers COMMAND musikcore_plugin_contract_headers_test)
set_tests_properties(musikcore.unit.plugins.contract_headers PROPERTIES
  LABELS "unit;musikcore;plugins;abi;contract")

# use the production DLL as the test subject; the host compiles only loader code.
# this helper also keeps API-only builds independent of the application targets.
function(musikcore_add_null_output_contract_test)
  add_executable(musikcore_null_output_test "${PROJECT_SOURCE_DIR}/tests/integration/plugins/null-output.cpp"
    "${PROJECT_SOURCE_DIR}/src/musikcore/ffi/plugin_abi/Plugin.cpp"
    "${PROJECT_SOURCE_DIR}/src/musikcore/ffi/plugin_abi/PluginRegistry.cpp"
    "${PROJECT_SOURCE_DIR}/src/musikcore/ffi/common/error.cpp")
  target_include_directories(musikcore_null_output_test PRIVATE
    "${PROJECT_SOURCE_DIR}/include" "${PROJECT_SOURCE_DIR}/src")
  target_compile_features(musikcore_null_output_test PRIVATE cxx_std_20)
  target_link_libraries(musikcore_null_output_test PRIVATE Threads::Threads ${CMAKE_DL_LIBS})
  set_target_properties(musikcore_null_output_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin")
  add_dependencies(musikcore_null_output_test nullout)
  add_dependencies(musikcube_tests musikcore_null_output_test)
  add_dependencies(musikcube_integration_tests musikcore_null_output_test)
  add_test(NAME musikcore.integration.plugins.null_output
    COMMAND musikcore_null_output_test "$<TARGET_FILE:nullout>")
  set_tests_properties(musikcore.integration.plugins.null_output PROPERTIES
    LABELS "integration;musikcore;plugins;audio;contract" TIMEOUT 30)

  # each child must hit the always-on fatal contract before the active DLL
  # can unload. this runs unchanged in Release and sanitizer configurations.
  foreach(mode IN ITEMS shutdown initialize selection diagnostic nested)
    add_test(NAME musikcore.integration.plugins.reentry_${mode} COMMAND "${CMAKE_COMMAND}"
      "-DHELPER=$<TARGET_FILE:musikcore_null_output_test>" "-DPLUGIN=$<TARGET_FILE:nullout>"
      "-DMODE=${mode}" -P "${PROJECT_SOURCE_DIR}/tests/scripts/check-plugin-reentry.cmake")
    set_tests_properties(musikcore.integration.plugins.reentry_${mode} PROPERTIES
      LABELS "integration;musikcore;plugins;contract" TIMEOUT 15)
  endforeach()

  if(MUSIKCORE_ABI_ONLY AND UNIX AND CMAKE_NM)
    add_test(NAME musikcore.integration.binary.exports.nullout COMMAND "${CMAKE_COMMAND}"
      "-DAPPLE=${APPLE}" "-DNM=${CMAKE_NM}" "-DLIBRARY=$<TARGET_FILE:nullout>"
      "-DALLOWLIST=$<TARGET_PROPERTY:nullout,MUSIKCUBE_EXPORT_ALLOWLIST>"
      -P "${PROJECT_SOURCE_DIR}/tests/scripts/check-exports.cmake")
    set_tests_properties(musikcore.integration.binary.exports.nullout PROPERTIES
      LABELS "integration;musikcore;packaging;contract" TIMEOUT 10)
  endif()
endfunction()

if(MUSIKCORE_ABI_ONLY)
  # the plugin uses only installed-shape C headers and system threads, so its
  # real load/unload contract can run without building the application engine.
  add_subdirectory("${PROJECT_SOURCE_DIR}/src/plugins/nullout" fixtures/nullout)
  musikcube_restrict_exports(nullout mc_plugin_get_api)
  musikcore_add_null_output_contract_test()
endif()

foreach(stage IN ITEMS entry initialize shutdown)
  set(target "musikcore_plugin_${stage}_throws")
  add_library(${target} MODULE support/plugins/throwing.cpp support/plugins/unload-probe.c)
  string(TOUPPER "${stage}_THROWS" definition)
  target_compile_definitions(${target} PRIVATE MC_BUILDING_PLUGIN ${definition})
  target_include_directories(${target} PRIVATE "${PROJECT_SOURCE_DIR}/include")
  target_compile_features(${target} PRIVATE cxx_std_17)
  set_target_properties(${target} PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${provider_output_directory}"
    LIBRARY_OUTPUT_DIRECTORY "${provider_output_directory}"
    RUNTIME_OUTPUT_DIRECTORY "${provider_output_directory}"
    SUFFIX "${CMAKE_SHARED_LIBRARY_SUFFIX}")
  musikcube_restrict_exports(${target} mc_plugin_get_api)
  add_dependencies(musikcore_plugin_registry_test ${target})
  add_test(NAME musikcore.integration.plugins.exception_${stage} COMMAND "${CMAKE_COMMAND}"
    "-DHELPER=$<TARGET_FILE:musikcore_plugin_registry_test>"
    "-DPLUGIN=$<TARGET_FILE:${target}>" "-DSTAGE=${stage}"
    -P "${PROJECT_SOURCE_DIR}/tests/scripts/check-plugin-exception.cmake")
  set_tests_properties(musikcore.integration.plugins.exception_${stage} PROPERTIES
    LABELS "integration;musikcore;plugins;contract" TIMEOUT 15)
endforeach()

# this executable has only public include paths and no core linkage: the C++
# plugin adapter must remain independently usable and strictly header-only.
add_executable(musikcore_output_adapter_test unit/musikcore/output-adapter.cpp)
target_include_directories(musikcore_output_adapter_test PRIVATE "${PROJECT_SOURCE_DIR}/include")
target_compile_features(musikcore_output_adapter_test PRIVATE cxx_std_17)
set_target_properties(musikcore_output_adapter_test PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin")
add_dependencies(musikcube_tests musikcore_output_adapter_test)
add_dependencies(musikcube_unit_tests musikcore_output_adapter_test)
add_test(NAME musikcore.unit.plugins.output_adapter COMMAND musikcore_output_adapter_test)
set_tests_properties(musikcore.unit.plugins.output_adapter PROPERTIES
  LABELS "unit;musikcore;plugins;contract" TIMEOUT 15)
