# provider fixtures compile as C and exchange only installed-shape function tables.
set(provider_variants valid incompatible newer_minor truncated missing_shutdown bad_id bad_utf8 entry_fail init_fail bad_capability no_entry duplicate_id missing_version)
set(provider_arguments)
foreach(variant IN LISTS provider_variants)
  set(target "musikcore_provider_${variant}")
  add_library(${target} MODULE support/plugins/provider.c)
  target_include_directories(${target} PRIVATE "${PROJECT_SOURCE_DIR}/include")
  if(NOT variant STREQUAL "no_entry")
    target_compile_definitions(${target} PRIVATE MC_BUILDING_PLUGIN)
  endif()
  if(NOT variant STREQUAL "valid")
    string(TOUPPER "${variant}" definition)
    target_compile_definitions(${target} PRIVATE ${definition})
  endif()
  set_target_properties(${target} PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/fixtures/providers"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/fixtures/providers"
    SUFFIX "${CMAKE_SHARED_LIBRARY_SUFFIX}")
  if(variant STREQUAL "no_entry")
    musikcube_restrict_exports(${target} not_a_plugin)
  else()
    musikcube_restrict_exports(${target} mc_plugin_get_api)
  endif()
  list(APPEND provider_arguments "$<TARGET_FILE:${target}>")
  add_dependencies(musikcube_tests ${target})
  add_dependencies(musikcube_integration_tests ${target})
endforeach()

# compile the same private loader/error implementation without exporting its C++ types.
add_executable(musikcore_provider_host_test integration/plugins/provider-host.cpp
  "${PROJECT_SOURCE_DIR}/src/musikcore/ffi/plugin_abi/ProviderHost.cpp"
  "${PROJECT_SOURCE_DIR}/src/musikcore/ffi/common/error.cpp")
target_include_directories(musikcore_provider_host_test PRIVATE
  "${PROJECT_SOURCE_DIR}/include" "${PROJECT_SOURCE_DIR}/src")
target_compile_features(musikcore_provider_host_test PRIVATE cxx_std_17)
target_link_libraries(musikcore_provider_host_test PRIVATE Threads::Threads ${CMAKE_DL_LIBS})
set_target_properties(musikcore_provider_host_test PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin")
add_dependencies(musikcube_tests musikcore_provider_host_test)
add_dependencies(musikcube_integration_tests musikcore_provider_host_test)
add_test(NAME musikcore.integration.plugins.lifecycle
  COMMAND musikcore_provider_host_test ${provider_arguments})
set_tests_properties(musikcore.integration.plugins.lifecycle PROPERTIES
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
