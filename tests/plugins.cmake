# in-source builds must also keep generated DLLs outside the source fixtures.
# bin is the shared ignored test-output directory used by the other helpers.
set(provider_output_directory "${CMAKE_CURRENT_BINARY_DIR}/bin/fixtures/providers")

# provider fixtures compile as C and exchange only installed-shape function tables.
set(provider_variants valid incompatible newer_minor truncated missing_shutdown bad_id bad_utf8 entry_fail init_fail bad_capability no_entry duplicate_id missing_version bad_output bad_stream bad_decoder bad_configuration bad_encoder bad_dsp bad_indexer bad_number_configuration duplicate_configuration bad_playback_remote)
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

# load a pure-C decoder from the same build-only fixture directory as the malformed providers.
add_library(musikcore_byte_decoder MODULE support/plugins/byte-decoder.c)
target_include_directories(musikcore_byte_decoder PRIVATE "${PROJECT_SOURCE_DIR}/include")
target_compile_definitions(musikcore_byte_decoder PRIVATE MC_BUILDING_PLUGIN)
set_target_properties(musikcore_byte_decoder PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF
  ARCHIVE_OUTPUT_DIRECTORY "${provider_output_directory}" LIBRARY_OUTPUT_DIRECTORY "${provider_output_directory}"
  RUNTIME_OUTPUT_DIRECTORY "${provider_output_directory}" SUFFIX "${CMAKE_SHARED_LIBRARY_SUFFIX}")
musikcube_restrict_exports(musikcore_byte_decoder mc_plugin_get_api)

add_executable(musikcore_decoder_stream_test integration/plugins/decoder-stream.cpp
  "${PROJECT_SOURCE_DIR}/src/musikcore/io/LocalFileSource.cpp"
  "${PROJECT_SOURCE_DIR}/src/musikcore/ffi/plugin_abi/AudioScratchBuffers.cpp"
  "${PROJECT_SOURCE_DIR}/src/musikcore/ffi/plugin_abi/PluginRegistry.cpp"
  "${PROJECT_SOURCE_DIR}/src/musikcore/ffi/plugin_abi/Plugin.cpp"
  "${PROJECT_SOURCE_DIR}/src/musikcore/ffi/common/error.cpp")
target_include_directories(musikcore_decoder_stream_test PRIVATE "${PROJECT_SOURCE_DIR}/include" "${PROJECT_SOURCE_DIR}/src")
target_compile_features(musikcore_decoder_stream_test PRIVATE cxx_std_20)
target_link_libraries(musikcore_decoder_stream_test PRIVATE Threads::Threads ${CMAKE_DL_LIBS})
set_target_properties(musikcore_decoder_stream_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin")
add_dependencies(musikcore_decoder_stream_test musikcore_byte_decoder)
add_dependencies(musikcube_tests musikcore_decoder_stream_test)
add_dependencies(musikcube_integration_tests musikcore_decoder_stream_test)
add_test(NAME musikcore.integration.plugins.decoder_stream
  COMMAND musikcore_decoder_stream_test "$<TARGET_FILE:musikcore_byte_decoder>")
set_tests_properties(musikcore.integration.plugins.decoder_stream PROPERTIES LABELS "integration;musikcore;plugins;audio;contract")

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

add_executable(musikcore_local_file_source_test unit/musikcore/local-file-source.cpp
  "${PROJECT_SOURCE_DIR}/src/musikcore/io/LocalFileSource.cpp")
target_include_directories(musikcore_local_file_source_test PRIVATE "${PROJECT_SOURCE_DIR}/include" "${PROJECT_SOURCE_DIR}/src")
target_compile_features(musikcore_local_file_source_test PRIVATE cxx_std_20)
set_target_properties(musikcore_local_file_source_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin")
add_dependencies(musikcube_tests musikcore_local_file_source_test)
add_dependencies(musikcube_unit_tests musikcore_local_file_source_test)
add_test(NAME musikcore.unit.plugins.local_file_source COMMAND musikcore_local_file_source_test
  "${CMAKE_CURRENT_BINARY_DIR}/bin/local-source-runs")
set_tests_properties(musikcore.unit.plugins.local_file_source PROPERTIES LABELS "unit;musikcore;plugins;audio;contract")

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

# separate sessions must not share the bootstrap host captured by an indexer source.
add_executable(musikcore_indexer_adapter_test unit/musikcore/indexer-adapter.cpp)
target_include_directories(musikcore_indexer_adapter_test PRIVATE "${PROJECT_SOURCE_DIR}/include")
target_compile_features(musikcore_indexer_adapter_test PRIVATE cxx_std_17)
set_target_properties(musikcore_indexer_adapter_test PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin")
add_dependencies(musikcube_tests musikcore_indexer_adapter_test)
add_dependencies(musikcube_unit_tests musikcore_indexer_adapter_test)
add_test(NAME musikcore.unit.plugins.indexer_adapter COMMAND musikcore_indexer_adapter_test)
set_tests_properties(musikcore.unit.plugins.indexer_adapter PROPERTIES
  LABELS "unit;musikcore;plugins;contract" TIMEOUT 15)

# verify the public C++ remote helper without linking private core code.
add_executable(musikcore_playback_remote_adapter_test unit/musikcore/playback-remote-adapter.cpp)
target_include_directories(musikcore_playback_remote_adapter_test PRIVATE "${PROJECT_SOURCE_DIR}/include")
target_compile_features(musikcore_playback_remote_adapter_test PRIVATE cxx_std_17)
set_target_properties(musikcore_playback_remote_adapter_test PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin")
add_dependencies(musikcube_tests musikcore_playback_remote_adapter_test)
add_dependencies(musikcube_unit_tests musikcore_playback_remote_adapter_test)
add_test(NAME musikcore.unit.plugins.playback_remote_adapter COMMAND musikcore_playback_remote_adapter_test)
set_tests_properties(musikcore.unit.plugins.playback_remote_adapter PROPERTIES
  LABELS "unit;musikcore;plugins" TIMEOUT 15)

# a recording DLL proves validation and lifecycle order at the actual engine
# output boundary; it uses the same C++ helper available to external plugins.
add_library(musikcore_output_recording MODULE support/plugins/output-recording.cpp support/plugins/unload-probe.c)
target_include_directories(musikcore_output_recording PRIVATE "${PROJECT_SOURCE_DIR}/include")
target_compile_definitions(musikcore_output_recording PRIVATE MC_BUILDING_PLUGIN)
target_compile_features(musikcore_output_recording PRIVATE cxx_std_17)
target_link_libraries(musikcore_output_recording PRIVATE Threads::Threads)
set_target_properties(musikcore_output_recording PROPERTIES SUFFIX "${CMAKE_SHARED_LIBRARY_SUFFIX}"
  ARCHIVE_OUTPUT_DIRECTORY "${provider_output_directory}"
  LIBRARY_OUTPUT_DIRECTORY "${provider_output_directory}"
  RUNTIME_OUTPUT_DIRECTORY "${provider_output_directory}")
musikcube_restrict_exports(musikcore_output_recording mc_plugin_get_api)
add_executable(musikcore_output_host_test integration/plugins/output-host.cpp
  "${PROJECT_SOURCE_DIR}/src/musikcore/audio/Output.cpp"
  "${PROJECT_SOURCE_DIR}/src/musikcore/ffi/plugin_abi/Plugin.cpp"
  "${PROJECT_SOURCE_DIR}/src/musikcore/ffi/plugin_abi/PluginRegistry.cpp"
  "${PROJECT_SOURCE_DIR}/src/musikcore/ffi/common/error.cpp")
target_include_directories(musikcore_output_host_test PRIVATE "${PROJECT_SOURCE_DIR}/src" "${PROJECT_SOURCE_DIR}/include")
target_compile_features(musikcore_output_host_test PRIVATE cxx_std_20)
target_link_libraries(musikcore_output_host_test PRIVATE Threads::Threads ${CMAKE_DL_LIBS})
set_target_properties(musikcore_output_host_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin")
add_dependencies(musikcore_output_host_test musikcore_output_recording)
add_dependencies(musikcube_tests musikcore_output_host_test)
add_dependencies(musikcube_integration_tests musikcore_output_host_test)
add_test(NAME musikcore.integration.plugins.output_host
  COMMAND musikcore_output_host_test "$<TARGET_FILE:musikcore_output_recording>")
set_tests_properties(musikcore.integration.plugins.output_host PROPERTIES LABELS "integration;musikcore;plugins;audio;contract" TIMEOUT 20)

# Stop failure must remain fatal in Release, before calling a destructor that
# cannot safely assume callbacks/workers relinquished the output instance.
add_test(NAME musikcore.integration.plugins.output_stop_failure COMMAND "${CMAKE_COMMAND}"
  "-DHELPER=$<TARGET_FILE:musikcore_output_host_test>" "-DPLUGIN=$<TARGET_FILE:musikcore_output_recording>" -DMODE=stop
  -P "${PROJECT_SOURCE_DIR}/tests/scripts/check-output-host-fatal.cmake")
set_tests_properties(musikcore.integration.plugins.output_stop_failure PROPERTIES
  LABELS "integration;musikcore;plugins;audio;contract" TIMEOUT 15)

# callback corruption is fatal before a stale or duplicate token can recycle PCM
# that belongs to a different submission or player.
foreach(mode IN ITEMS duplicate stale)
  add_test(NAME musikcore.integration.plugins.output_${mode}_completion COMMAND "${CMAKE_COMMAND}"
    "-DHELPER=$<TARGET_FILE:musikcore_output_host_test>" "-DPLUGIN=$<TARGET_FILE:musikcore_output_recording>" "-DMODE=${mode}"
    -P "${PROJECT_SOURCE_DIR}/tests/scripts/check-output-host-fatal.cmake")
  set_tests_properties(musikcore.integration.plugins.output_${mode}_completion PROPERTIES
    LABELS "integration;musikcore;plugins;audio;contract" TIMEOUT 15)
endforeach()

# exercise the encoder ABI with a C-only provider and two independent instances.
add_library(musikcore_byte_encoder MODULE support/plugins/byte-encoder.c)
target_include_directories(musikcore_byte_encoder PRIVATE "${PROJECT_SOURCE_DIR}/include")
target_compile_definitions(musikcore_byte_encoder PRIVATE MC_BUILDING_PLUGIN)
set_target_properties(musikcore_byte_encoder PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF
  ARCHIVE_OUTPUT_DIRECTORY "${provider_output_directory}"
  LIBRARY_OUTPUT_DIRECTORY "${provider_output_directory}"
  RUNTIME_OUTPUT_DIRECTORY "${provider_output_directory}"
  SUFFIX "${CMAKE_SHARED_LIBRARY_SUFFIX}")
musikcube_restrict_exports(musikcore_byte_encoder mc_plugin_get_api)

add_executable(musikcore_encoder_test integration/plugins/encoder.cpp
  "${PROJECT_SOURCE_DIR}/src/musikcore/ffi/plugin_abi/PluginRegistry.cpp"
  "${PROJECT_SOURCE_DIR}/src/musikcore/ffi/plugin_abi/Plugin.cpp"
  "${PROJECT_SOURCE_DIR}/src/musikcore/ffi/common/error.cpp")
target_include_directories(musikcore_encoder_test PRIVATE "${PROJECT_SOURCE_DIR}/include" "${PROJECT_SOURCE_DIR}/src")
target_compile_features(musikcore_encoder_test PRIVATE cxx_std_20)
target_link_libraries(musikcore_encoder_test PRIVATE Threads::Threads ${CMAKE_DL_LIBS})
set_target_properties(musikcore_encoder_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin")
add_dependencies(musikcore_encoder_test musikcore_byte_encoder)
add_dependencies(musikcube_tests musikcore_encoder_test)
add_dependencies(musikcube_integration_tests musikcore_encoder_test)
add_test(NAME musikcore.integration.plugins.encoder
  COMMAND musikcore_encoder_test "$<TARGET_FILE:musikcore_byte_encoder>")
set_tests_properties(musikcore.integration.plugins.encoder PROPERTIES
  LABELS "integration;musikcore;plugins;audio;contract" TIMEOUT 15)

# metadata callbacks cross the C boundary, including on allocation failure.
add_executable(musikcore_metadata_writer_test unit/musikcore/metadata-writer.cpp
  "${PROJECT_SOURCE_DIR}/src/musikcore/plugin/MetadataWriter.cpp"
  "${PROJECT_SOURCE_DIR}/src/musikcore/ffi/common/error.cpp")
target_include_directories(musikcore_metadata_writer_test PRIVATE "${PROJECT_SOURCE_DIR}/include" "${PROJECT_SOURCE_DIR}/src")
target_compile_features(musikcore_metadata_writer_test PRIVATE cxx_std_20)
set_target_properties(musikcore_metadata_writer_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin")
add_dependencies(musikcube_tests musikcore_metadata_writer_test)
add_dependencies(musikcube_unit_tests musikcore_metadata_writer_test)
add_test(NAME musikcore.unit.plugins.metadata_writer COMMAND musikcore_metadata_writer_test)
set_tests_properties(musikcore.unit.plugins.metadata_writer PROPERTIES LABELS "unit;musikcore;plugins;contract" TIMEOUT 15)
