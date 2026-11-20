add_executable(musikcube_seed_library seed-library.cpp)
target_include_directories(musikcube_seed_library PRIVATE "${PROJECT_SOURCE_DIR}/src" "${CMAKE_CURRENT_SOURCE_DIR}" ${VENDOR_INCLUDE_DIRECTORIES})
set_target_properties(musikcube_seed_library PROPERTIES DISABLE_PRECOMPILE_HEADERS ON)
target_compile_features(musikcube_seed_library PRIVATE cxx_std_17)
target_link_libraries(musikcube_seed_library PRIVATE musikcore Threads::Threads)

if(WIN32)
  target_link_libraries(musikcube_seed_library PRIVATE musikcube_windows vendor::ssl pdh psapi ws2_32 wldap32 comctl32)
  add_dependencies(musikcube_seed_library musikcube_runtime)
else()
  target_link_libraries(musikcube_seed_library PRIVATE ${musikcube_LINK_LIBS})
  foreach(plugin IN ITEMS taglibreader ffmpegdecoder)
    if(TARGET ${plugin})
      add_dependencies(musikcube_seed_library ${plugin})
    endif()
  endforeach()
endif()

if(BUILD_TESTING)
  add_executable(musikcore_remote_query_transport_test ../integration/transport/remote-query-transport.cpp)
  target_include_directories(musikcore_remote_query_transport_test PRIVATE
    "${PROJECT_SOURCE_DIR}/src" "${CMAKE_CURRENT_SOURCE_DIR}" ${VENDOR_INCLUDE_DIRECTORIES})
  target_link_libraries(musikcore_remote_query_transport_test PRIVATE musikcore Threads::Threads)
  set_target_properties(musikcore_remote_query_transport_test PROPERTIES
    DISABLE_PRECOMPILE_HEADERS ON)
  if(WIN32)
    target_link_libraries(musikcore_remote_query_transport_test PRIVATE
      musikcube_windows vendor::ssl pdh psapi ws2_32 wldap32 comctl32)
  endif()
  add_dependencies(musikcube_tests musikcore_remote_query_transport_test)
  add_dependencies(musikcube_integration_tests musikcore_remote_query_transport_test)
  add_test(NAME musikcore.abi.remote_query_transport COMMAND "${MUSIKCORE_TEST_NODE}"
    "${PROJECT_SOURCE_DIR}/tests/scripts/run-isolated-test.js" "${CMAKE_CURRENT_BINARY_DIR}/bin/isolated"
    "$<TARGET_FILE:musikcore_remote_query_transport_test>" --stage)
  set_tests_properties(musikcore.abi.remote_query_transport PROPERTIES
    LABELS "integration;musikcore;abi;contract"
    TIMEOUT 20)

  find_program(MUSIKCUBE_TEST_NODE NAMES node)
  if(MUSIKCUBE_TEST_NODE)
    add_executable(musikcore_production_api_test
      ../integration/api/cpp-wrapper.cpp ../integration/api/query-support.cpp)
    target_compile_definitions(musikcore_production_api_test PRIVATE MUSIKCORE_TEST_APP_BINDING)
    target_include_directories(musikcore_production_api_test PRIVATE
      "${PROJECT_SOURCE_DIR}/src" "${CMAKE_CURRENT_SOURCE_DIR}" ${VENDOR_INCLUDE_DIRECTORIES})
    set_target_properties(musikcore_production_api_test PROPERTIES DISABLE_PRECOMPILE_HEADERS ON)
    target_compile_features(musikcore_production_api_test PRIVATE cxx_std_17)
    target_link_libraries(musikcore_production_api_test PRIVATE musikcore Threads::Threads)
    if(WIN32)
      target_link_libraries(musikcore_production_api_test PRIVATE musikcube_windows vendor::ssl pdh psapi ws2_32 wldap32 comctl32)
    endif()
    add_dependencies(musikcube_tests musikcube_seed_library musikcore_production_api_test)
    add_dependencies(musikcube_integration_tests musikcube_seed_library musikcore_production_api_test)
    set(indexed_arguments
      "-DNODE=${MUSIKCUBE_TEST_NODE}"
      "-DSEED_SCRIPT=${PROJECT_SOURCE_DIR}/tests/scripts/seed-test-library.js"
      "-DISOLATED_RUNNER=${PROJECT_SOURCE_DIR}/tests/scripts/run-isolated-test.js"
      "-DBUILD_DIR=${PROJECT_BINARY_DIR}"
      "-DINDEXER=$<TARGET_FILE:musikcube_seed_library>"
      "-DQUERY_TEST=$<TARGET_FILE:musikcore_abi_query_test>"
      "-DCPP_TEST=$<TARGET_FILE:musikcore_abi_cpp_test>"
      "-DPRODUCTION_TEST=$<TARGET_FILE:musikcore_production_api_test>"
      "-DC_EXAMPLE=$<TARGET_FILE:musikcore_example_c>"
      "-DCPP_EXAMPLE=$<TARGET_FILE:musikcore_example_cpp>"
      "-DREMOTE_TEST=$<TARGET_FILE:musikcore_remote_query_transport_test>")
    set(indexed_script "${PROJECT_SOURCE_DIR}/tests/scripts/check-indexed-library.cmake")
    add_test(NAME musikcore.abi.indexed_query COMMAND "${CMAKE_COMMAND}"
      ${indexed_arguments} -P "${indexed_script}")
    set_tests_properties(musikcore.abi.indexed_query PROPERTIES
      LABELS "integration;musikcore;api;contract" TIMEOUT 90)

    # Preserve the public-only consumer and production adapter as separate
    # processes while reporting each behavior independently through CTest.
    foreach(mode IN ITEMS cpp production)
      if(mode STREQUAL "production")
        set(cases query search result_windows message_cleanup playback_commands
          remote_payloads browse_reads selected_backend)
        set(production ON)
      else()
        set(cases query search result_windows)
        set(production OFF)
      endif()
      foreach(case IN LISTS cases)
        set(name "musikcore.integration.api.${mode}.${case}")
        add_test(NAME "${name}" COMMAND "${CMAKE_COMMAND}" ${indexed_arguments}
          "-DWRAPPER_CASE=${case}" "-DWRAPPER_PRODUCTION=${production}" -P "${indexed_script}")
        set_tests_properties("${name}" PROPERTIES
          LABELS "integration;musikcore;api;contract" TIMEOUT 90)
      endforeach()
    endforeach()
    add_test(NAME musikcore.integration.api.backend_parity COMMAND "${CMAKE_COMMAND}"
      ${indexed_arguments} -DBACKEND_PARITY=ON -P "${indexed_script}")
    set_tests_properties(musikcore.integration.api.backend_parity PROPERTIES
      LABELS "integration;musikcore;api;transport;contract" TIMEOUT 90)
  endif()
endif()

# Keep small production UI checks in the ordinary suite; catalog/latency
# benchmarks remain explicit commands instead of machine-dependent assertions.
if(BUILD_TESTING AND UNIX AND TARGET musikcube AND TARGET taglibreader AND TARGET ffmpegdecoder)
  add_dependencies(musikcube_tests musikcube taglibreader ffmpegdecoder)
  add_dependencies(musikcube_ui_tests musikcube musikcube_seed_library taglibreader ffmpegdecoder)
  foreach(scenario IN ITEMS search queue playlists browse)
    add_test(NAME musikcube.ui.${scenario} COMMAND "${MUSIKCORE_TEST_NODE}"
      "${PROJECT_SOURCE_DIR}/tests/ui/scenarios/track-search.js"
      --build-dir "${PROJECT_BINARY_DIR}" --bin-dir "$<TARGET_FILE_DIR:musikcube>"
      --tracks 1000 --windows --scenario "${scenario}")
    set_tests_properties(musikcube.ui.${scenario} PROPERTIES
      LABELS "ui;musikcube;${scenario}" TIMEOUT 90)
  endforeach()
endif()
