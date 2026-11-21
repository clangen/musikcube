add_executable(musikcube_seed_library seed-library.cpp host-wakeup.cpp)
set_target_properties(musikcube_seed_library PROPERTIES DISABLE_PRECOMPILE_HEADERS ON)
target_compile_features(musikcube_seed_library PRIVATE cxx_std_17)
target_link_libraries(musikcube_seed_library PRIVATE musikcore Threads::Threads)

if(WIN32)
  target_link_libraries(musikcube_seed_library PRIVATE musikcube_windows vendor::ssl pdh psapi ws2_32 wldap32 comctl32)
  add_dependencies(musikcube_seed_library musikcube_runtime)
else()
  foreach(plugin IN ITEMS taglibreader ffmpegdecoder)
    if(TARGET ${plugin})
      add_dependencies(musikcube_seed_library ${plugin})
    endif()
  endforeach()
endif()

if(BUILD_TESTING)

  if(UNIX AND TARGET musikcube AND CMAKE_NM)
    add_test(NAME musikcube.integration.core_boundary COMMAND "${CMAKE_COMMAND}"
      "-DNM=${CMAKE_NM}" "-DAPPLE=${APPLE}" "-DAPP=$<TARGET_FILE:musikcube>"
      "-DSOURCE=${PROJECT_SOURCE_DIR}" -P "${PROJECT_SOURCE_DIR}/tests/scripts/check-cube-boundary.cmake")
    set_tests_properties(musikcube.integration.core_boundary PROPERTIES
      LABELS "integration;musikcube;abi;contract")
    add_dependencies(musikcube_tests musikcube)
  endif()

  add_executable(musikcore_example_remote
    "${PROJECT_SOURCE_DIR}/examples/core-remote.c" host-wakeup.cpp)
  target_include_directories(musikcore_example_remote PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
  target_link_libraries(musikcore_example_remote PRIVATE musikcore Threads::Threads)
  set_target_properties(musikcore_example_remote PROPERTIES
    C_STANDARD 11 C_STANDARD_REQUIRED ON DISABLE_PRECOMPILE_HEADERS ON)
  add_dependencies(musikcube_tests musikcore_example_remote)

  add_executable(musikcore_remote_query_transport_test ../integration/transport/remote-query-transport.cpp fixture-library.cpp)
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
  add_test(NAME musikcore.integration.api.managed_remote COMMAND "${MUSIKCORE_TEST_NODE}"
    "${PROJECT_SOURCE_DIR}/tests/scripts/run-isolated-test.js" "${CMAKE_CURRENT_BINARY_DIR}/bin/isolated"
    "$<TARGET_FILE:musikcore_remote_query_transport_test>" --stage --managed)
  set_tests_properties(musikcore.integration.api.managed_remote PROPERTIES
    LABELS "integration;musikcore;api;transport;contract" TIMEOUT 20)

  find_program(MUSIKCUBE_TEST_NODE NAMES node)
  if(MUSIKCUBE_TEST_NODE)
    add_executable(musikcore_production_api_test
      ../integration/api/cpp-wrapper.cpp ../integration/api/query-support.cpp fixture-library.cpp)
    target_compile_definitions(musikcore_production_api_test PRIVATE MUSIKCORE_TEST_PRODUCTION_RUNTIME)
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

    if(UNIX AND CMAKE_VERSION VERSION_GREATER_EQUAL 3.16 AND
        CMAKE_SYSTEM_NAME STREQUAL CMAKE_HOST_SYSTEM_NAME AND
        CMAKE_SYSTEM_PROCESSOR STREQUAL CMAKE_HOST_SYSTEM_PROCESSOR)
      set(installed_plugin_args)
      if(TARGET nullout)
        list(APPEND installed_plugin_args "-DINSTALLED_TEST_PLUGIN=$<TARGET_FILE_NAME:nullout>")
        add_dependencies(musikcube_tests nullout)
      endif()
      add_test(NAME musikcore.integration.api.installed_sdk COMMAND "${CMAKE_COMMAND}"
        ${indexed_arguments} "-DINSTALLED_SDK_SOURCE=${PROJECT_SOURCE_DIR}" ${installed_plugin_args} -P "${indexed_script}")
      set_tests_properties(musikcore.integration.api.installed_sdk PROPERTIES
        LABELS "integration;musikcore;api;packaging;contract" TIMEOUT 90)
    endif()

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

    if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND TARGET ffmpegdecoder)
      add_library(musikcore_pcm_capture_output MODULE pcm-capture-output.cpp)
      target_include_directories(musikcore_pcm_capture_output PRIVATE "${PROJECT_SOURCE_DIR}/src")
      target_compile_features(musikcore_pcm_capture_output PRIVATE cxx_std_20)
      target_link_libraries(musikcore_pcm_capture_output PRIVATE Threads::Threads)
      set_target_properties(musikcore_pcm_capture_output PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/fixtures"
        DISABLE_PRECOMPILE_HEADERS ON)

      add_executable(musikcore_playback_pcm_test ../integration/transport/playback-pcm.cpp)
      target_include_directories(musikcore_playback_pcm_test PRIVATE
        "${PROJECT_SOURCE_DIR}/src" "${CMAKE_CURRENT_SOURCE_DIR}" ${VENDOR_INCLUDE_DIRECTORIES})
      target_link_libraries(musikcore_playback_pcm_test PRIVATE musikcore Threads::Threads ${CMAKE_DL_LIBS})
      set_target_properties(musikcore_playback_pcm_test PROPERTIES DISABLE_PRECOMPILE_HEADERS ON)
      add_dependencies(musikcore_playback_pcm_test musikcore_pcm_capture_output ffmpegdecoder)
      add_dependencies(musikcube_tests musikcore_playback_pcm_test)
      add_dependencies(musikcube_integration_tests musikcore_playback_pcm_test)

      foreach(mode IN ITEMS gapless crossfade)
        add_test(NAME musikcore.integration.transport.${mode}_pcm COMMAND "${MUSIKCUBE_TEST_NODE}"
          "${PROJECT_SOURCE_DIR}/tests/scripts/run-isolated-test.js" "${CMAKE_CURRENT_BINARY_DIR}/bin/isolated"
          "$<TARGET_FILE:musikcore_playback_pcm_test>" --stage
          --plugin "$<TARGET_FILE:musikcore_pcm_capture_output>" "${mode}"
          "$<TARGET_FILE_NAME:musikcore_pcm_capture_output>" @origin)
        set_tests_properties(musikcore.integration.transport.${mode}_pcm PROPERTIES
          LABELS "integration;musikcore;transport;playback" TIMEOUT 35)
      endforeach()
    endif()

    if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND TARGET nullout AND TARGET ffmpegdecoder AND TARGET taglibreader)
      add_executable(musikcore_plugin_playback_test ../integration/musikcore/plugin-playback.cpp)
      target_include_directories(musikcore_plugin_playback_test PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}" ${VENDOR_INCLUDE_DIRECTORIES})
      set_target_properties(musikcore_plugin_playback_test PROPERTIES DISABLE_PRECOMPILE_HEADERS ON)
      target_link_libraries(musikcore_plugin_playback_test PRIVATE musikcore Threads::Threads)
      add_dependencies(musikcore_plugin_playback_test nullout musikcube_seed_library)
      add_dependencies(musikcube_tests musikcore_plugin_playback_test)
      add_dependencies(musikcube_integration_tests musikcore_plugin_playback_test)
      add_test(NAME musikcore.integration.plugin.playback_reload COMMAND "${CMAKE_COMMAND}"
        ${indexed_arguments} "-DPLUGIN_PLAYBACK_TEST=$<TARGET_FILE:musikcore_plugin_playback_test>"
        "-DNULL_OUTPUT=$<TARGET_FILE:nullout>" -P "${indexed_script}")
      set_tests_properties(musikcore.integration.plugin.playback_reload PROPERTIES
        LABELS "integration;musikcore;plugin;playback" TIMEOUT 30)

      add_executable(musikcore_managed_runtime_test ../integration/musikcore/managed-runtime.cpp)
      set_target_properties(musikcore_managed_runtime_test PROPERTIES DISABLE_PRECOMPILE_HEADERS ON)
      target_link_libraries(musikcore_managed_runtime_test PRIVATE musikcore Threads::Threads)
      add_dependencies(musikcore_managed_runtime_test nullout musikcube_seed_library)
      add_dependencies(musikcube_tests musikcore_managed_runtime_test)
      add_dependencies(musikcube_integration_tests musikcore_managed_runtime_test)
      set(managed_runtime_plugins)
      if(TARGET supereqdsp)
        add_dependencies(musikcore_managed_runtime_test supereqdsp)
        list(APPEND managed_runtime_plugins "-DEQUALIZER_PLUGIN=$<TARGET_FILE:supereqdsp>")
      endif()
      if(TARGET server)
        add_dependencies(musikcore_managed_runtime_test server)
        list(APPEND managed_runtime_plugins "-DSERVER_PLUGIN=$<TARGET_FILE:server>")
      endif()
      add_test(NAME musikcore.integration.api.managed_runtime COMMAND "${CMAKE_COMMAND}"
        ${indexed_arguments} "-DMANAGED_RUNTIME_TEST=$<TARGET_FILE:musikcore_managed_runtime_test>"
        ${managed_runtime_plugins}
        "-DNULL_OUTPUT=$<TARGET_FILE:nullout>" -P "${indexed_script}")
      set_tests_properties(musikcore.integration.api.managed_runtime PROPERTIES
        LABELS "integration;musikcore;runtime;contract" TIMEOUT 30)
      if(TARGET server AND TARGET httpdatastream)
        add_dependencies(musikcore_managed_runtime_test httpdatastream)
        add_test(NAME musikcore.integration.api.remote_audio COMMAND "${CMAKE_COMMAND}"
          ${indexed_arguments} "-DMANAGED_RUNTIME_TEST=$<TARGET_FILE:musikcore_managed_runtime_test>"
          "-DSERVER_PLUGIN=$<TARGET_FILE:server>" "-DHTTP_DATASTREAM_PLUGIN=$<TARGET_FILE:httpdatastream>"
          "-DNULL_OUTPUT=$<TARGET_FILE:nullout>" -P "${indexed_script}")
        set_tests_properties(musikcore.integration.api.remote_audio PROPERTIES
          LABELS "integration;musikcore;transport;playback;contract" TIMEOUT 30)
      endif()
    endif()
  endif()
endif()

# Keep small production UI checks in the ordinary suite; catalog/latency
# benchmarks remain explicit commands instead of machine-dependent assertions.
if(BUILD_TESTING AND UNIX AND TARGET musikcube AND TARGET taglibreader AND TARGET ffmpegdecoder)
  add_dependencies(musikcube_tests musikcube taglibreader ffmpegdecoder)
  add_dependencies(musikcube_ui_tests musikcube musikcube_seed_library taglibreader ffmpegdecoder)
  foreach(plugin IN ITEMS nullout supereqdsp)
    if(TARGET ${plugin})
      add_dependencies(musikcube_tests ${plugin})
      add_dependencies(musikcube_ui_tests ${plugin})
    endif()
  endforeach()
  foreach(scenario IN ITEMS search queue playlists browse settings locale)
    set(script "${scenario}")
    set(arguments)
    if(scenario STREQUAL "search")
      set(script "track-search")
      set(arguments --windows)
    endif()
    add_test(NAME musikcube.ui.${scenario} COMMAND "${MUSIKCORE_TEST_NODE}"
      "${PROJECT_SOURCE_DIR}/tests/ui/scenarios/${script}.js"
      --build-dir "${PROJECT_BINARY_DIR}" --bin-dir "$<TARGET_FILE_DIR:musikcube>"
      --tracks 1000 ${arguments})
    set_tests_properties(musikcube.ui.${scenario} PROPERTIES
      LABELS "ui;musikcube;${scenario}" TIMEOUT 90)
  endforeach()
  if(TARGET nullout)
    add_test(NAME musikcube.ui.lyrics COMMAND "${MUSIKCORE_TEST_NODE}"
      "${PROJECT_SOURCE_DIR}/tests/ui/scenarios/lyrics.js"
      --build-dir "${PROJECT_BINARY_DIR}" --bin-dir "$<TARGET_FILE_DIR:musikcube>" --tracks 20)
    set_tests_properties(musikcube.ui.lyrics PROPERTIES
      LABELS "ui;musikcube;lyrics" TIMEOUT 30)
  endif()
  if(TARGET musikcore_pcm_capture_output)
    add_dependencies(musikcube_ui_tests musikcore_pcm_capture_output)
    foreach(mode IN ITEMS gapless crossfade)
      add_test(NAME musikcube.ui.transport.${mode} COMMAND "${MUSIKCORE_TEST_NODE}"
        "${PROJECT_SOURCE_DIR}/tests/ui/scenarios/transport.js"
        --build-dir "${PROJECT_BINARY_DIR}" --bin-dir "$<TARGET_FILE_DIR:musikcube>"
        --tracks 1000 --playback-mode "${mode}"
        --capture-output "$<TARGET_FILE:musikcore_pcm_capture_output>")
      set_tests_properties(musikcube.ui.transport.${mode} PROPERTIES
        LABELS "ui;musikcube;transport" TIMEOUT 90)
    endforeach()
  endif()
endif()
