add_test(NAME musikcore.integration.plugins.retired_sdk_boundary
  COMMAND "${CMAKE_COMMAND}" "-DSOURCE=${PROJECT_SOURCE_DIR}"
    -P "${PROJECT_SOURCE_DIR}/tests/scripts/check-retired-plugin-sdk.cmake")
set_tests_properties(musikcore.integration.plugins.retired_sdk_boundary PROPERTIES
  LABELS "integration;musikcore;plugins;contract" TIMEOUT 20)

add_executable(musikcore_fixed_ring_queue_test unit/common/fixed-ring-queue.cpp)
target_include_directories(musikcore_fixed_ring_queue_test PRIVATE "${PROJECT_SOURCE_DIR}/include")
target_compile_features(musikcore_fixed_ring_queue_test PRIVATE cxx_std_17)
set_target_properties(musikcore_fixed_ring_queue_test PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin")
add_test(NAME musikcore.unit.common.fixed_ring_queue COMMAND musikcore_fixed_ring_queue_test)
set_tests_properties(musikcore.unit.common.fixed_ring_queue PROPERTIES LABELS "unit;musikcore;common;contract")

add_executable(musikcore_abi_header_test
  unit/musikcore/ffi/header.c
  unit/musikcore/ffi/header.cpp
  unit/musikcore/ffi/error.cpp
  "${PROJECT_SOURCE_DIR}/src/musikcore/ffi/common/error.cpp"
  "${PROJECT_SOURCE_DIR}/src/musikcore/ffi/common/version.cpp"
  "${PROJECT_SOURCE_DIR}/src/musikcore/ffi/common/string.cpp"
)
target_include_directories(musikcore_abi_header_test PRIVATE
  "${PROJECT_SOURCE_DIR}/include"
  "${PROJECT_SOURCE_DIR}/src/musikcore/ffi/common"
)
set_target_properties(musikcore_abi_header_test PROPERTIES
  C_STANDARD 11
  C_STANDARD_REQUIRED ON
  C_EXTENSIONS OFF
  CXX_STANDARD 11
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin"
)

add_test(NAME musikcore.abi.header COMMAND musikcore_abi_header_test)
set_tests_properties(musikcore.abi.header PROPERTIES LABELS "unit;musikcore;abi;contract")

add_executable(musikcore_abi0_c_test integration/api/shared.c)
target_link_libraries(musikcore_abi0_c_test PRIVATE musikcore_abi0)
set_target_properties(musikcore_abi0_c_test PROPERTIES
  C_STANDARD 11
  C_STANDARD_REQUIRED ON
  C_EXTENSIONS OFF
  LINKER_LANGUAGE C
  RUNTIME_OUTPUT_DIRECTORY "${MUSIKCORE_ABI0_OUTPUT_DIR}"
)
add_test(NAME musikcore.abi.shared_c COMMAND musikcore_abi0_c_test)
set_tests_properties(musikcore.abi.shared_c PROPERTIES LABELS "integration;musikcore;abi;contract")

if(UNIX)
  add_test(NAME musikcore.abi.exports COMMAND "${CMAKE_COMMAND}"
    "-DNM=${CMAKE_NM}"
    "-DAPPLE=${APPLE}"
    "-DLIBRARY=$<TARGET_FILE:musikcore_abi0>"
    "-DALLOWLIST=${PROJECT_SOURCE_DIR}/src/musikcore/ffi/exports.txt"
    -P "${PROJECT_SOURCE_DIR}/tests/scripts/check-exports.cmake")
  set_tests_properties(musikcore.abi.exports PROPERTIES LABELS "integration;musikcore;abi;contract")
elseif(MSVC)
  add_test(NAME musikcore.abi.exports COMMAND "${CMAKE_COMMAND}"
    "-DLINKER=${CMAKE_LINKER}" "-DLIBRARY=$<TARGET_FILE:musikcore_abi0>"
    "-DALLOWLIST=${PROJECT_SOURCE_DIR}/src/musikcore/ffi/exports.txt"
    -P "${PROJECT_SOURCE_DIR}/tests/scripts/check-windows-core-dll.cmake")
  set_tests_properties(musikcore.abi.exports PROPERTIES LABELS "integration;musikcore;abi;contract")
endif()

add_executable(musikcore_abi_dispatch_test
  integration/api/dispatch.cpp
  integration/api/context-allocation.cpp
  unit/musikcore/ffi/library-storage.cpp
  "${PROJECT_SOURCE_DIR}/src/musikcore/ffi/consumer/context.cpp"
  "${PROJECT_SOURCE_DIR}/src/musikcore/ffi/consumer/audio-abi-only.cpp"
  "${PROJECT_SOURCE_DIR}/src/musikcore/ffi/common/error.cpp"
)
target_include_directories(musikcore_abi_dispatch_test PRIVATE
  "${PROJECT_SOURCE_DIR}/include"
  "${PROJECT_SOURCE_DIR}/src"
  "${PROJECT_SOURCE_DIR}/src/3rdparty/include"
  "${PROJECT_SOURCE_DIR}/src/musikcore/ffi/consumer"
)
target_link_libraries(musikcore_abi_dispatch_test PRIVATE Threads::Threads)
if(MSVC)
  # The allocation fault sweep must avoid MSVC's debug-only iterator proxy
  # allocations in noexcept container constructors.
  target_compile_definitions(musikcore_abi_dispatch_test PRIVATE _ITERATOR_DEBUG_LEVEL=0)
endif()
set_target_properties(musikcore_abi_dispatch_test PROPERTIES
  CXX_STANDARD 20
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin"
)
add_test(NAME musikcore.abi.dispatch COMMAND musikcore_abi_dispatch_test)
set_tests_properties(musikcore.abi.dispatch PROPERTIES LABELS "integration;musikcore;abi;contract" TIMEOUT 20)

add_executable(musikcore_abi_query_test integration/api/query.c integration/api/query-support.cpp)
target_link_libraries(musikcore_abi_query_test PRIVATE musikcore_abi0 musikcore_abi0_sqlite Threads::Threads)
set_target_properties(musikcore_abi_query_test PROPERTIES
  C_STANDARD 11
  C_STANDARD_REQUIRED ON
  C_EXTENSIONS OFF
  CXX_STANDARD 11
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
  RUNTIME_OUTPUT_DIRECTORY "${MUSIKCORE_ABI0_OUTPUT_DIR}"
)
find_program(MUSIKCORE_TEST_NODE NAMES node)
if(NOT MUSIKCORE_TEST_NODE)
  message(FATAL_ERROR "Node.js is required for isolated ABI test fixtures")
endif()
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_CXX_COMPILER MATCHES "zig-[^/]+/bin/c[+][+]$")
  add_test(NAME build.zig.shared_finalizers COMMAND "${MUSIKCORE_TEST_NODE}"
    "${PROJECT_SOURCE_DIR}/tests/scripts/check-zig-shared-finalizers.js"
    "${CMAKE_CXX_COMPILER}" "${CMAKE_C_COMPILER}" "${CMAKE_CURRENT_BINARY_DIR}/bin")
  set_tests_properties(build.zig.shared_finalizers PROPERTIES LABELS "integration;build;contract")
endif()
set(isolated_test "${PROJECT_SOURCE_DIR}/tests/scripts/run-isolated-test.js")
set(isolated_root "${CMAKE_CURRENT_BINARY_DIR}/bin/isolated")
add_test(NAME musikcore.abi.inventory COMMAND "${MUSIKCORE_TEST_NODE}"
  "${PROJECT_SOURCE_DIR}/tests/scripts/audit-core-api.js" --check)
set_tests_properties(musikcore.abi.inventory PROPERTIES LABELS "unit;musikcore;abi;contract")
add_test(NAME musikcore.abi.link_boundary COMMAND "${CMAKE_COMMAND}"
  "-DFIXTURE=${CMAKE_CURRENT_SOURCE_DIR}/integration/binary/link-boundary"
  "-DBUILD=${CMAKE_CURRENT_BINARY_DIR}/bin/link-boundary"
  "-DGUARD=${PROJECT_SOURCE_DIR}/.cmake/VerifyCoreBoundary.cmake"
  -P "${PROJECT_SOURCE_DIR}/tests/scripts/check-link-boundary.cmake")
set_tests_properties(musikcore.abi.link_boundary PROPERTIES LABELS "integration;musikcore;abi;contract")
# Configure the real support targets without production/vendor dependencies.
# A tests-disabled build must not require or create the test aggregate targets.
add_test(NAME musikcore.integration.binary.testing_disabled COMMAND "${CMAKE_COMMAND}"
  -S "${CMAKE_CURRENT_SOURCE_DIR}/integration/binary/testing-disabled"
  -B "${CMAKE_CURRENT_BINARY_DIR}/bin/testing-disabled"
  "-DSUPPORT=${PROJECT_SOURCE_DIR}/tests/support")
set_tests_properties(musikcore.integration.binary.testing_disabled PROPERTIES
  LABELS "integration;musikcore;binary;build;contract" TIMEOUT 30)

add_test(NAME musikcore.abi.query COMMAND "${MUSIKCORE_TEST_NODE}" "${isolated_test}"
  "${isolated_root}" "$<TARGET_FILE:musikcore_abi_query_test>" @origin)
set_tests_properties(musikcore.abi.query PROPERTIES LABELS "integration;musikcore;abi;contract" TIMEOUT 20)

add_custom_target(musikcube_tests DEPENDS
  musikcore_abi_header_test musikcore_fixed_ring_queue_test musikcore_abi0_c_test
  musikcore_abi_dispatch_test musikcore_abi_query_test)

# build both assertion modes even when the enclosing configuration selects one.
# the subprocess driver checks real aborts without terminating the test runner.
foreach(mode IN ITEMS debug ndebug)
  set(target "musikcore_contract_${mode}_test")
  add_executable(${target} unit/common/contract.cpp)
  target_include_directories(${target} PRIVATE "${PROJECT_SOURCE_DIR}/src")
  target_compile_features(${target} PRIVATE cxx_std_20)
  if(mode STREQUAL "ndebug")
    target_compile_definitions(${target} PRIVATE MUSIKCORE_TEST_NDEBUG=1)
  else()
    target_compile_definitions(${target} PRIVATE MUSIKCORE_TEST_NDEBUG=0)
  endif()
  set_target_properties(${target} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin")
  add_dependencies(musikcube_tests ${target})
  add_test(NAME musikcore.unit.contract.${mode} COMMAND "${CMAKE_COMMAND}"
    "-DHELPER=$<TARGET_FILE:${target}>" -P "${PROJECT_SOURCE_DIR}/tests/scripts/check-contract.cmake")
  set_tests_properties(musikcore.unit.contract.${mode} PROPERTIES
    LABELS "unit;common;contract" TIMEOUT 20)
endforeach()

# Compile the queue on its own: these tests should not need audio, plugins,
# global library services, or the production precompiled header.
add_executable(musikcore_message_queue_test
  unit/musikcore/runtime/message-queue.cpp
  "${PROJECT_SOURCE_DIR}/src/common/runtime/MessageQueue.cpp"
  "${PROJECT_SOURCE_DIR}/src/common/runtime/Message.cpp")
target_include_directories(musikcore_message_queue_test PRIVATE
  "${PROJECT_SOURCE_DIR}/src" "${PROJECT_SOURCE_DIR}/src/3rdparty/include")
target_link_libraries(musikcore_message_queue_test PRIVATE Threads::Threads)
set_target_properties(musikcore_message_queue_test PROPERTIES
  CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF
  DISABLE_PRECOMPILE_HEADERS ON RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin")
add_dependencies(musikcube_tests musikcore_message_queue_test)
foreach(case IN ITEMS registration_order filtered_removal unregister_during_dispatch
    reentrant_removal reentrant_handler destructor_cleanup delay_debounce broadcasts
    shared_payload_lifetime wait_wakeup wait_ready concurrent_posting)
  add_test(NAME musikcore.runtime.message_queue.${case} COMMAND musikcore_message_queue_test "${case}")
  set_tests_properties(musikcore.runtime.message_queue.${case} PROPERTIES LABELS "unit;musikcore;runtime" TIMEOUT 10)
endforeach()

# This application helper is header-only and needs no running core context.
add_executable(musikcube_async_callback_scope_test unit/musikcube/async-callback-scope.cpp)
target_include_directories(musikcube_async_callback_scope_test PRIVATE
  "${PROJECT_SOURCE_DIR}/src/musikcube" "${PROJECT_SOURCE_DIR}/include")
set_target_properties(musikcube_async_callback_scope_test PROPERTIES
  CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin")
add_dependencies(musikcube_tests musikcube_async_callback_scope_test)
add_test(NAME musikcube.unit.async_callback_scope COMMAND musikcube_async_callback_scope_test)
set_tests_properties(musikcube.unit.async_callback_scope PROPERTIES LABELS "unit;musikcube;app" TIMEOUT 10)

add_executable(musikcube_transport_format_test unit/musikcube/transport-format.cpp)
target_include_directories(musikcube_transport_format_test PRIVATE
  "${PROJECT_SOURCE_DIR}/src/musikcube" "${PROJECT_SOURCE_DIR}/include")
set_target_properties(musikcube_transport_format_test PROPERTIES
  CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin")
add_dependencies(musikcube_tests musikcube_transport_format_test)
add_test(NAME musikcube.unit.transport_format COMMAND musikcube_transport_format_test)
set_tests_properties(musikcube.unit.transport_format PROPERTIES LABELS "unit;musikcube;transport" TIMEOUT 10)

add_executable(musikcore_abi_arguments_test integration/api/arguments.c)
target_link_libraries(musikcore_abi_arguments_test PRIVATE musikcore_abi0)
set_target_properties(musikcore_abi_arguments_test PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON
  C_EXTENSIONS OFF LINKER_LANGUAGE C RUNTIME_OUTPUT_DIRECTORY "${MUSIKCORE_ABI0_OUTPUT_DIR}")
add_dependencies(musikcube_tests musikcore_abi_arguments_test)
add_test(NAME musikcore.abi.arguments COMMAND musikcore_abi_arguments_test)
set_tests_properties(musikcore.abi.arguments PROPERTIES LABELS "integration;musikcore;abi;contract" TIMEOUT 20)

add_executable(musikcore_result_contracts_test integration/api/result-contracts.cpp support/host-wakeup.cpp support/fail-new.cpp)
target_include_directories(musikcore_result_contracts_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/support")
target_link_libraries(musikcore_result_contracts_test PRIVATE musikcore_cpp_abi0 musikcore_abi0_sqlite Threads::Threads)
set_target_properties(musikcore_result_contracts_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON
  RUNTIME_OUTPUT_DIRECTORY "${MUSIKCORE_ABI0_OUTPUT_DIR}")
add_dependencies(musikcube_tests musikcore_result_contracts_test)
add_test(NAME musikcore.abi.result_contracts COMMAND "${MUSIKCORE_TEST_NODE}" "${isolated_test}"
  "${isolated_root}" "$<TARGET_FILE:musikcore_result_contracts_test>" @origin)
set_tests_properties(musikcore.abi.result_contracts PROPERTIES LABELS "integration;musikcore;abi;contract" TIMEOUT 20)

add_executable(musikcore_abi_cpp_test
  integration/api/cpp-wrapper.cpp integration/api/cpp-wrapper-query.cpp integration/api/query-support.cpp)
target_link_libraries(musikcore_abi_cpp_test PRIVATE musikcore_cpp_abi0 Threads::Threads)
set_target_properties(musikcore_abi_cpp_test PROPERTIES
  CXX_EXTENSIONS OFF
  RUNTIME_OUTPUT_DIRECTORY "${MUSIKCORE_ABI0_OUTPUT_DIR}"
)
add_dependencies(musikcube_tests musikcore_abi_cpp_test)

add_executable(musikcore_typed_track_queries_test
  integration/library/typed-track-queries.cpp
  "${PROJECT_SOURCE_DIR}/src/musikcore/db/SqliteExtensions.cpp"
  "${PROJECT_SOURCE_DIR}/src/musikcore/library/query/TrackReadQueries.cpp"
)
target_include_directories(musikcore_typed_track_queries_test PRIVATE
  "${PROJECT_SOURCE_DIR}/include"
  "${PROJECT_SOURCE_DIR}/src"
  "${PROJECT_SOURCE_DIR}/src/3rdparty/include"
)
target_compile_features(musikcore_typed_track_queries_test PRIVATE cxx_std_17)
target_link_libraries(musikcore_typed_track_queries_test PRIVATE musikcore_abi0_sqlite)
set_target_properties(musikcore_typed_track_queries_test PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin"
)
add_dependencies(musikcube_tests musikcore_typed_track_queries_test)
add_test(NAME musikcore.abi.typed_track_queries COMMAND musikcore_typed_track_queries_test)
set_tests_properties(musikcore.abi.typed_track_queries PROPERTIES LABELS "integration;musikcore;library;contract")

add_executable(musikcore_abi_library_switch_test integration/app/library-switch.cpp integration/api/query-support.cpp)
target_include_directories(musikcore_abi_library_switch_test PRIVATE "${PROJECT_SOURCE_DIR}/src/musikcube")
target_link_libraries(musikcore_abi_library_switch_test PRIVATE
  musikcore_cpp_abi0 musikcore_abi0_sqlite Threads::Threads)
set_target_properties(musikcore_abi_library_switch_test PROPERTIES
  CXX_EXTENSIONS OFF
  RUNTIME_OUTPUT_DIRECTORY "${MUSIKCORE_ABI0_OUTPUT_DIR}"
)
add_test(NAME musikcore.abi.library_switch COMMAND "${MUSIKCORE_TEST_NODE}" "${isolated_test}"
  "${isolated_root}" "$<TARGET_FILE:musikcore_abi_library_switch_test>" @origin @destination)
set_tests_properties(musikcore.abi.library_switch PROPERTIES LABELS "integration;musikcube;api;contract" TIMEOUT 20)
add_dependencies(musikcube_tests musikcore_abi_library_switch_test)

if(APPLE)
  add_library(musikcore_test_profile SHARED support/profile-macos.c)
  set_target_properties(musikcore_test_profile PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON
    C_VISIBILITY_PRESET hidden LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin")
  target_compile_options(musikcore_test_profile PRIVATE -fno-builtin-memcpy -fno-builtin-memmove)
  add_dependencies(musikcube_tests musikcore_test_profile)
  # Exercise the real macOS script launcher so socket-input failures and feeder
  # cleanup regressions are caught without building the full application.
  add_test(NAME musikcube.ui.terminal_driver COMMAND "${MUSIKCORE_TEST_NODE}" --test
    "${PROJECT_SOURCE_DIR}/tests/ui/driver/terminal.test.js")
  set_tests_properties(musikcube.ui.terminal_driver PROPERTIES LABELS "integration;musikcube;terminal" TIMEOUT 45)
endif()

# Compile each public header in its own translation unit using public includes only.
set(public_c_headers plugin/library plugin/playback-remote plugin/indexer plugin/tag plugin/metadata plugin/analyzer plugin/dsp plugin/encoder plugin/settings plugin/audio plugin/services plugin/media plugin/stream plugin/decoder plugin/output abi context subscriptions library playback player media output equalizer indexer visualizer preferences plugins plugin paths locale log online musikcore)
set(public_cpp_headers plugin/playback-remote plugin/registration plugin/indexer plugin/encoder plugin/dsp plugin/decoder plugin/output plugin/stream plugin/subtrack-source context subscriptions library playback player media output equalizer indexer visualizer preferences plugins paths locale log online musikcore track-result-window detail/async detail/track-page-cache)
set(header_sources)
foreach(header IN LISTS public_c_headers)
  foreach(language IN ITEMS c cpp)
    set(unit "${CMAKE_CURRENT_BINARY_DIR}/bin/headers/${header}.h.${language}")
    file(GENERATE OUTPUT "${unit}" CONTENT "#include <musikcore/${header}.h>\n")
    list(APPEND header_sources "${unit}")
  endforeach()
endforeach()
add_executable(musikcore_api_baseline integration/performance/core-api.cpp support/host-wakeup.cpp)
target_include_directories(musikcore_api_baseline PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/support")
target_link_libraries(musikcore_api_baseline PRIVATE musikcore_cpp_abi0 musikcore_abi0_sqlite Threads::Threads)
set_target_properties(musikcore_api_baseline PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF RUNTIME_OUTPUT_DIRECTORY "${MUSIKCORE_ABI0_OUTPUT_DIR}")
add_dependencies(musikcube_tests musikcore_api_baseline)
foreach(header IN LISTS public_cpp_headers)
  set(unit "${CMAKE_CURRENT_BINARY_DIR}/bin/headers/${header}.hpp.cpp")
  file(GENERATE OUTPUT "${unit}" CONTENT "#include <musikcore/${header}.hpp>\n")
  list(APPEND header_sources "${unit}")
endforeach()
add_library(musikcore_public_headers OBJECT ${header_sources})
target_include_directories(musikcore_public_headers PRIVATE "${PROJECT_SOURCE_DIR}/include")
set_target_properties(musikcore_public_headers PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON
  C_EXTENSIONS OFF CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
add_dependencies(musikcube_tests musikcore_public_headers)

foreach(language IN ITEMS c cpp)
  add_executable(musikcore_example_${language}
    "${PROJECT_SOURCE_DIR}/examples/core-query.${language}" support/host-wakeup.cpp)
  target_include_directories(musikcore_example_${language} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/support")
  target_link_libraries(musikcore_example_${language} PRIVATE musikcore_cpp_abi0 Threads::Threads)
  set_target_properties(musikcore_example_${language} PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON
    C_EXTENSIONS OFF CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF
    RUNTIME_OUTPUT_DIRECTORY "${MUSIKCORE_ABI0_OUTPUT_DIR}")
  add_dependencies(musikcube_tests musikcore_example_${language})
endforeach()

# Driver-model tests run without launching the app or requiring native plugins.
add_test(NAME musikcube.unit.terminal_screen COMMAND "${MUSIKCORE_TEST_NODE}" --test
  "${PROJECT_SOURCE_DIR}/tests/ui/driver/screen.test.js")
set_tests_properties(musikcube.unit.terminal_screen PROPERTIES
  LABELS "unit;musikcube;terminal" TIMEOUT 20)
add_test(NAME musikcube.unit.ui_artifacts COMMAND "${MUSIKCORE_TEST_NODE}" --test
  "${PROJECT_SOURCE_DIR}/tests/ui/driver/artifacts.test.js")
set_tests_properties(musikcube.unit.ui_artifacts PROPERTIES
  LABELS "unit;musikcube;terminal" TIMEOUT 20)

# Level-specific targets keep a quick unit run from building the full app.
add_custom_target(musikcube_unit_tests DEPENDS musikcore_contract_debug_test musikcore_contract_ndebug_test musikcore_abi_header_test
  musikcore_message_queue_test musikcore_fixed_ring_queue_test musikcube_async_callback_scope_test musikcube_transport_format_test
  musikcore_public_headers)
add_custom_target(musikcube_integration_tests DEPENDS musikcore_abi0_c_test
  musikcore_abi_dispatch_test musikcore_abi_query_test musikcore_abi_arguments_test
  musikcore_result_contracts_test musikcore_abi_cpp_test musikcore_typed_track_queries_test
  musikcore_abi_library_switch_test)
add_custom_target(musikcube_ui_tests)

set(driver_tests musikcube.unit.terminal_screen musikcube.unit.ui_artifacts)
if(APPLE)
  list(APPEND driver_tests musikcube.ui.terminal_driver)
endif()
set_tests_properties(${driver_tests} PROPERTIES
  ENVIRONMENT "MUSIKCUBE_TEST_ARTIFACT_ROOT=${CMAKE_CURRENT_BINARY_DIR}/bin/driver")

add_executable(musikcore_playback_math_test unit/musikcore/playback-math.cpp)
target_include_directories(musikcore_playback_math_test PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_compile_features(musikcore_playback_math_test PRIVATE cxx_std_20)
set_target_properties(musikcore_playback_math_test PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin")
add_dependencies(musikcube_tests musikcore_playback_math_test)
add_dependencies(musikcube_unit_tests musikcore_playback_math_test)
add_test(NAME musikcore.unit.playback_math COMMAND musikcore_playback_math_test)
set_tests_properties(musikcore.unit.playback_math PROPERTIES LABELS "unit;musikcore;playback")

add_executable(musikcore_subscriptions_test unit/musikcore/subscriptions.cpp)
target_include_directories(musikcore_subscriptions_test PRIVATE "${PROJECT_SOURCE_DIR}/include")
target_compile_features(musikcore_subscriptions_test PRIVATE cxx_std_20)
target_link_libraries(musikcore_subscriptions_test PRIVATE musikcore_abi0 Threads::Threads)
set_target_properties(musikcore_subscriptions_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${MUSIKCORE_ABI0_OUTPUT_DIR}")
add_dependencies(musikcube_tests musikcore_subscriptions_test)
add_dependencies(musikcube_unit_tests musikcore_subscriptions_test)
add_test(NAME musikcore.unit.subscriptions COMMAND musikcore_subscriptions_test)
set_tests_properties(musikcore.unit.subscriptions PROPERTIES LABELS "unit;musikcore;playback;contract" TIMEOUT 10)

if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_SYSTEM_NAME STREQUAL "Linux"
    AND CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "^(x86_64|amd64|AMD64)$"
    AND CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|amd64|AMD64)$")
  add_test(NAME musikcube.integration.packaging.standalone_elf
    COMMAND "${MUSIKCORE_TEST_NODE}" "${PROJECT_SOURCE_DIR}/tests/scripts/check-standalone-elf.js"
    "${CMAKE_C_COMPILER}" "${PROJECT_SOURCE_DIR}/script/scan-standalone.js"
    "${CMAKE_CURRENT_BINARY_DIR}/bin/fixtures" "${PROJECT_SOURCE_DIR}/script/patch-rpath.sh")
  set_tests_properties(musikcube.integration.packaging.standalone_elf PROPERTIES
    LABELS "integration;musikcube;packaging;contract" TIMEOUT 20)
endif()

# Deliberate faults must fail in both languages; successful probes stay quiet.
if(MUSIKCUBE_SANITIZERS_LIST AND CMAKE_BUILD_TYPE STREQUAL "Debug")
  foreach(language IN ITEMS c cpp)
    add_executable(musikcore_sanitizer_probe_${language} integration/binary/sanitizers/probe.${language})
    set_target_properties(musikcore_sanitizer_probe_${language} PROPERTIES
      RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin")
    add_dependencies(musikcube_tests musikcore_sanitizer_probe_${language})
    add_dependencies(musikcube_integration_tests musikcore_sanitizer_probe_${language})
    add_test(NAME musikcore.integration.binary.sanitizers.${language}
      COMMAND "${MUSIKCORE_TEST_NODE}" "${PROJECT_SOURCE_DIR}/tests/scripts/check-sanitizers.js"
      "$<TARGET_FILE:musikcore_sanitizer_probe_${language}>" ${MUSIKCUBE_SANITIZERS_LIST})
    set_tests_properties(musikcore.integration.binary.sanitizers.${language} PROPERTIES
      LABELS "integration;musikcore;binary;sanitizer;contract" TIMEOUT 30)
  endforeach()
endif()

# Repeated dlclose must not leave stale global redzones, and an actual overflow
# after reload must still fail. This isolates the AppleClang 21 runtime defect.
if(APPLE AND CMAKE_BUILD_TYPE STREQUAL "Debug" AND "address" IN_LIST MUSIKCUBE_SANITIZERS_LIST)
  foreach(name IN ITEMS short long)
    add_library(musikcore_sanitizer_${name} MODULE integration/binary/sanitizers/module.c)
    set_target_properties(musikcore_sanitizer_${name} PROPERTIES
      LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin/fixtures/sanitizers")
    musikcube_restrict_exports(musikcore_sanitizer_${name} module_name module_byte)
    add_dependencies(musikcube_tests musikcore_sanitizer_${name})
    add_dependencies(musikcube_integration_tests musikcore_sanitizer_${name})
  endforeach()
  target_compile_definitions(musikcore_sanitizer_long PRIVATE
    MODULE_NAME="the considerably longer name of the second instrumented module")
  add_executable(musikcore_sanitizer_reload_test integration/binary/sanitizers/reload.cpp)
  set_target_properties(musikcore_sanitizer_reload_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin")
  add_dependencies(musikcube_tests musikcore_sanitizer_reload_test)
  add_dependencies(musikcube_integration_tests musikcore_sanitizer_reload_test)
  add_test(NAME musikcore.integration.binary.sanitizer_reload COMMAND "${MUSIKCORE_TEST_NODE}"
    "${PROJECT_SOURCE_DIR}/tests/scripts/check-sanitizer-reload.js"
    "$<TARGET_FILE:musikcore_sanitizer_reload_test>" "$<TARGET_FILE:musikcore_sanitizer_short>"
    "$<TARGET_FILE:musikcore_sanitizer_long>")
  set_tests_properties(musikcore.integration.binary.sanitizer_reload PROPERTIES
    LABELS "integration;musikcore;binary;sanitizer;contract" TIMEOUT 45)
endif()

include("${CMAKE_CURRENT_LIST_DIR}/plugins.cmake")

# declaration macros are shared by core and app owners; compile-time assertions
# guard their copy/move and constructor policies without loading either runtime.
add_executable(musikcube_special_members_test unit/common/special-members.cpp)
target_include_directories(musikcube_special_members_test PRIVATE "${PROJECT_SOURCE_DIR}/src")
set_target_properties(musikcube_special_members_test PROPERTIES
  CXX_STANDARD 11 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin")
add_dependencies(musikcube_tests musikcube_special_members_test)
add_dependencies(musikcube_unit_tests musikcube_special_members_test)
add_test(NAME common.unit.special_members COMMAND musikcube_special_members_test)
set_tests_properties(common.unit.special_members PROPERTIES LABELS "unit;common;contract")

add_executable(musikcore_library_scope_test integration/api/library-scope.cpp)
target_link_libraries(musikcore_library_scope_test PRIVATE musikcore_cpp_abi0 musikcore_abi0_sqlite Threads::Threads)
target_compile_features(musikcore_library_scope_test PRIVATE cxx_std_17)
set_target_properties(musikcore_library_scope_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin")
add_dependencies(musikcube_tests musikcore_library_scope_test)
add_test(NAME musikcore.integration.api.library_scope COMMAND musikcore_library_scope_test "${CMAKE_CURRENT_BINARY_DIR}/bin/scope-fixtures")
set_tests_properties(musikcore.integration.api.library_scope PROPERTIES LABELS "integration;musikcore;library;contract" TIMEOUT 15)
