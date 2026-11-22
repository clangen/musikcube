set(core_fixture_library musikcore)
if(TARGET musikcore_internal)
  set(core_fixture_library musikcore_internal)
endif()

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

# Keep the seeder available independently; everything below is test-only.
if(NOT BUILD_TESTING)
  return()
endif()

if(BUILD_TESTING)

  if(APPLE)
    add_test(NAME musikcore.integration.binary.macos_package COMMAND "${MUSIKCORE_TEST_NODE}"
      "${PROJECT_SOURCE_DIR}/tests/scripts/check-macos-package.js"
      "${CMAKE_CURRENT_BINARY_DIR}/bin/isolated" "${CMAKE_C_COMPILER}" "${CMAKE_COMMAND}")
    set_tests_properties(musikcore.integration.binary.macos_package PROPERTIES
      LABELS "integration;musikcore;packaging;contract" TIMEOUT 60)
  endif()

  if(UNIX AND CMAKE_NM)
    foreach(host IN ITEMS musikcube musikcubed)
      if(TARGET ${host})
        add_test(NAME ${host}.integration.core_boundary COMMAND "${CMAKE_COMMAND}"
          "-DNM=${CMAKE_NM}" "-DAPPLE=${APPLE}" "-DAPP=$<TARGET_FILE:${host}>"
          "-DSOURCE=${PROJECT_SOURCE_DIR}" "-DHOST=${host}"
          -P "${PROJECT_SOURCE_DIR}/tests/scripts/check-cube-boundary.cmake")
        set_tests_properties(${host}.integration.core_boundary PROPERTIES
          LABELS "integration;${host};abi;contract")
        add_dependencies(musikcube_tests ${host})
      endif()
    endforeach()
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
  target_link_libraries(musikcore_remote_query_transport_test PRIVATE ${core_fixture_library} Threads::Threads)
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

  add_test(NAME musikcore.integration.api.remote_versions COMMAND "${MUSIKCORE_TEST_NODE}"
    "${PROJECT_SOURCE_DIR}/tests/scripts/run-isolated-test.js" "${CMAKE_CURRENT_BINARY_DIR}/bin/isolated"
    "$<TARGET_FILE:musikcore_remote_query_transport_test>" --stage --versions)
  set_tests_properties(musikcore.integration.api.remote_versions PROPERTIES
    LABELS "integration;musikcore;api;transport;contract" TIMEOUT 20)

  find_program(MUSIKCUBE_TEST_NODE NAMES node)
  if(MUSIKCUBE_TEST_NODE)
    add_executable(musikcore_production_api_test
      ../integration/api/cpp-wrapper.cpp
      ../integration/api/cpp-wrapper-query.cpp
      ../integration/api/cpp-wrapper-playback.cpp
      ../integration/api/cpp-wrapper-production.cpp
      ../integration/api/query-support.cpp
      fixture-library.cpp)
    target_compile_definitions(musikcore_production_api_test PRIVATE MUSIKCORE_TEST_PRODUCTION_RUNTIME)
    target_include_directories(musikcore_production_api_test PRIVATE
      "${PROJECT_SOURCE_DIR}/src" "${CMAKE_CURRENT_SOURCE_DIR}" ${VENDOR_INCLUDE_DIRECTORIES})
    set_target_properties(musikcore_production_api_test PROPERTIES DISABLE_PRECOMPILE_HEADERS ON)
    target_compile_features(musikcore_production_api_test PRIVATE cxx_std_20)
    target_link_libraries(musikcore_production_api_test PRIVATE ${core_fixture_library} Threads::Threads)
    if(WIN32)
      target_link_libraries(musikcore_production_api_test PRIVATE musikcube_windows vendor::ssl pdh psapi ws2_32 wldap32 comctl32)
    endif()
    add_dependencies(musikcube_tests musikcube_seed_library musikcore_production_api_test)
    add_dependencies(musikcube_integration_tests musikcube_seed_library musikcore_production_api_test)
    set(indexed_arguments
      "-DCONSUMER_SANITIZERS=${sanitizers_flags}"
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

    if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.16 AND
        CMAKE_SYSTEM_NAME STREQUAL CMAKE_HOST_SYSTEM_NAME AND
        CMAKE_SYSTEM_PROCESSOR STREQUAL CMAKE_HOST_SYSTEM_PROCESSOR)
      set(installed_plugin_args)
      if(TARGET nullout)
        list(APPEND installed_plugin_args "-DINSTALLED_TEST_PLUGIN=$<TARGET_FILE_NAME:nullout>")
        add_dependencies(musikcube_tests nullout)
      endif()
      add_test(NAME musikcore.integration.api.installed_sdk COMMAND "${CMAKE_COMMAND}"
        ${indexed_arguments} "-DINSTALLED_SDK_SOURCE=${PROJECT_SOURCE_DIR}" "-DTEST_CONFIG=$<CONFIG>"
        ${installed_plugin_args} -P "${indexed_script}")
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

    if(TARGET ffmpegdecoder)
      add_library(musikcore_pcm_capture_output MODULE pcm-capture-output.cpp)
      target_include_directories(musikcore_pcm_capture_output PRIVATE "${PROJECT_SOURCE_DIR}/src" "${PROJECT_SOURCE_DIR}/include")
      target_compile_features(musikcore_pcm_capture_output PRIVATE cxx_std_20)
      target_link_libraries(musikcore_pcm_capture_output PRIVATE Threads::Threads)
      set_target_properties(musikcore_pcm_capture_output PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/fixtures"
        DISABLE_PRECOMPILE_HEADERS ON)

      if(APPLE)
        set_target_properties(musikcore_pcm_capture_output PROPERTIES SUFFIX ".dylib")
      endif()

      add_executable(musikcore_playback_pcm_test ../integration/transport/playback-pcm.cpp)
      target_include_directories(musikcore_playback_pcm_test PRIVATE
        "${PROJECT_SOURCE_DIR}/src" "${CMAKE_CURRENT_SOURCE_DIR}" ${VENDOR_INCLUDE_DIRECTORIES})
      target_link_libraries(musikcore_playback_pcm_test PRIVATE ${core_fixture_library} Threads::Threads ${CMAKE_DL_LIBS})
      if(WIN32)
        target_link_libraries(musikcore_playback_pcm_test PRIVATE musikcube_windows)
        add_dependencies(musikcore_playback_pcm_test musikcube_runtime)
      endif()
      set_target_properties(musikcore_playback_pcm_test PROPERTIES DISABLE_PRECOMPILE_HEADERS ON)
      add_dependencies(musikcore_playback_pcm_test musikcore_pcm_capture_output ffmpegdecoder)
      add_dependencies(musikcube_tests musikcore_playback_pcm_test)
      add_dependencies(musikcube_integration_tests musikcore_playback_pcm_test)

      add_custom_target(musikcube_playback_benchmark
        COMMAND "${MUSIKCUBE_TEST_NODE}" "${PROJECT_SOURCE_DIR}/tests/scripts/benchmark-playback.js"
          "${PROJECT_BINARY_DIR}" "$<TARGET_FILE:musikcore_playback_pcm_test>"
          "$<TARGET_FILE:musikcore_pcm_capture_output>"
        DEPENDS musikcore_playback_pcm_test
        USES_TERMINAL VERBATIM)

      foreach(mode IN ITEMS gapless crossfade teardown seek completion detach_listener replace_next paused_crossfade cancel_open)
        add_test(NAME musikcore.integration.transport.${mode}_pcm COMMAND "${MUSIKCUBE_TEST_NODE}"
          "${PROJECT_SOURCE_DIR}/tests/scripts/run-isolated-test.js" "${CMAKE_CURRENT_BINARY_DIR}/bin/isolated"
          "$<TARGET_FILE:musikcore_playback_pcm_test>" --stage
          --plugin "$<TARGET_FILE:musikcore_pcm_capture_output>" "${mode}"
          "$<TARGET_FILE_NAME:musikcore_pcm_capture_output>" @origin)
        set_tests_properties(musikcore.integration.transport.${mode}_pcm PROPERTIES
          LABELS "integration;musikcore;transport;playback" TIMEOUT 35)
      endforeach()
      set_tests_properties(musikcore.integration.transport.paused_crossfade_pcm PROPERTIES TIMEOUT 25)
    endif()

    if(TARGET nullout AND TARGET ffmpegdecoder AND (TARGET taglibreader OR TARGET taglib_plugin))
      add_executable(musikcore_plugin_playback_test ../integration/musikcore/plugin-playback.cpp)
      target_include_directories(musikcore_plugin_playback_test PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}" ${VENDOR_INCLUDE_DIRECTORIES})
      set_target_properties(musikcore_plugin_playback_test PROPERTIES DISABLE_PRECOMPILE_HEADERS ON)
      target_link_libraries(musikcore_plugin_playback_test PRIVATE ${core_fixture_library} Threads::Threads)
      if(WIN32)
        target_link_libraries(musikcore_plugin_playback_test PRIVATE musikcube_windows)
      endif()
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
      target_compile_features(musikcore_managed_runtime_test PRIVATE cxx_std_20)
      if(WIN32)
        target_link_libraries(musikcore_managed_runtime_test PRIVATE musikcube_windows ws2_32)
        target_compile_definitions(musikcore_managed_runtime_test PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
      endif()
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
      add_test(NAME musikcore.integration.api.playback_without_indexer COMMAND "${CMAKE_COMMAND}"
        ${indexed_arguments} "-DMANAGED_RUNTIME_TEST=$<TARGET_FILE:musikcore_managed_runtime_test>"
        ${managed_runtime_plugins} -DPLAYBACK_ONLY=ON
        "-DNULL_OUTPUT=$<TARGET_FILE:nullout>" -P "${indexed_script}")
      set_tests_properties(musikcore.integration.api.playback_without_indexer PROPERTIES
        LABELS "integration;musikcore;runtime;playback;contract" TIMEOUT 30)
      if(TARGET server AND TARGET httpdatastream)
        add_dependencies(musikcore_managed_runtime_test httpdatastream)
        add_test(NAME musikcore.integration.api.remote_audio COMMAND "${CMAKE_COMMAND}"
          ${indexed_arguments} "-DMANAGED_RUNTIME_TEST=$<TARGET_FILE:musikcore_managed_runtime_test>"
          "-DSERVER_PLUGIN=$<TARGET_FILE:server>" "-DHTTP_DATASTREAM_PLUGIN=$<TARGET_FILE:httpdatastream>"
          "-DNULL_OUTPUT=$<TARGET_FILE:nullout>" -P "${indexed_script}")
        set_tests_properties(musikcore.integration.api.remote_audio PROPERTIES
          LABELS "integration;musikcore;transport;playback;contract" TIMEOUT 30)
        if(TARGET stockencoders)
          add_dependencies(musikcore_managed_runtime_test stockencoders)
          foreach(format IN ITEMS mp3 flac)
            add_test(NAME musikcore.integration.api.remote_transcode.${format} COMMAND "${CMAKE_COMMAND}"
              ${indexed_arguments} "-DMANAGED_RUNTIME_TEST=$<TARGET_FILE:musikcore_managed_runtime_test>"
              "-DSERVER_PLUGIN=$<TARGET_FILE:server>" "-DHTTP_DATASTREAM_PLUGIN=$<TARGET_FILE:httpdatastream>"
              "-DNULL_OUTPUT=$<TARGET_FILE:nullout>" "-DENCODER_PLUGIN=$<TARGET_FILE:stockencoders>"
              "-DTRANSCODE_FORMAT=${format}" -P "${indexed_script}")
            set_tests_properties(musikcore.integration.api.remote_transcode.${format} PROPERTIES
              LABELS "integration;musikcore;transport;playback;contract" TIMEOUT 30)
          endforeach()
        endif()
      endif()
    endif()
  endif()
endif()

# Keep small production UI checks in the ordinary suite; catalog/latency
# benchmarks remain explicit commands instead of machine-dependent assertions.
if(BUILD_TESTING AND TARGET musikcube AND TARGET ffmpegdecoder AND (TARGET taglibreader OR TARGET taglib_plugin))
  set(ui_frontend musikcube)
  set(ui_tag_reader taglibreader)
  set(ui_platform_arguments)
  if(WIN32)
    set(ui_frontend musikcube-cmd)
    set(ui_tag_reader taglib_plugin)
    set(ui_platform_arguments --conpty-host "$<TARGET_FILE:musikcube_conpty_host>")
    add_dependencies(musikcube_ui_tests musikcube_conpty_host)
  endif()
  add_dependencies(musikcube_tests ${ui_frontend} ${ui_tag_reader} ffmpegdecoder)
  add_dependencies(musikcube_ui_tests ${ui_frontend} musikcube_seed_library ${ui_tag_reader} ffmpegdecoder)
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
      --build-dir "${PROJECT_BINARY_DIR}" --bin-dir "$<TARGET_FILE_DIR:${ui_frontend}>"
      --tracks 1000 ${ui_platform_arguments} ${arguments})
    set_tests_properties(musikcube.ui.${scenario} PROPERTIES
      LABELS "ui;musikcube;${scenario}" TIMEOUT 90)
  endforeach()
  if(TARGET nullout)
    add_test(NAME musikcube.ui.lyrics COMMAND "${MUSIKCORE_TEST_NODE}"
      "${PROJECT_SOURCE_DIR}/tests/ui/scenarios/lyrics.js"
      --build-dir "${PROJECT_BINARY_DIR}" --bin-dir "$<TARGET_FILE_DIR:${ui_frontend}>"
      --tracks 20 ${ui_platform_arguments})
    set_tests_properties(musikcube.ui.lyrics PROPERTIES
      LABELS "ui;musikcube;lyrics" TIMEOUT 30)
  endif()
  if(TARGET musikcore_pcm_capture_output)
    add_dependencies(musikcube_ui_tests musikcore_pcm_capture_output)
    foreach(mode IN ITEMS gapless crossfade)
      add_test(NAME musikcube.ui.transport.${mode} COMMAND "${MUSIKCORE_TEST_NODE}"
        "${PROJECT_SOURCE_DIR}/tests/ui/scenarios/transport.js"
        --build-dir "${PROJECT_BINARY_DIR}" --bin-dir "$<TARGET_FILE_DIR:${ui_frontend}>"
        --tracks 1000 --playback-mode "${mode}"
        --capture-output "$<TARGET_FILE:musikcore_pcm_capture_output>" ${ui_platform_arguments})
      set_tests_properties(musikcube.ui.transport.${mode} PROPERTIES
        LABELS "ui;musikcube;transport" TIMEOUT 90)
    endforeach()
  endif()
endif()

# exercise URI resources through the actual shared C boundary without catalogs.
if(TARGET nullout AND TARGET ffmpegdecoder)
  foreach(language IN ITEMS c cpp)
    add_executable(musikcore_example_player_${language} "${PROJECT_SOURCE_DIR}/examples/core-player.${language}" host-wakeup.cpp)
    target_include_directories(musikcore_example_player_${language} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(musikcore_example_player_${language} PRIVATE musikcore Threads::Threads)
    add_dependencies(musikcube_tests musikcore_example_player_${language})
  endforeach()
  add_executable(musikcore_uri_player_test ../integration/musikcore/uri-player.cpp host-wakeup.cpp)
  target_link_libraries(musikcore_uri_player_test PRIVATE musikcore Threads::Threads)
  set_target_properties(musikcore_uri_player_test PROPERTIES DISABLE_PRECOMPILE_HEADERS ON)
  add_dependencies(musikcore_uri_player_test nullout ffmpegdecoder)
  add_dependencies(musikcube_tests musikcore_uri_player_test)
  add_dependencies(musikcube_integration_tests musikcore_uri_player_test)
  set(uri_http)
  if(TARGET httpdatastream)
    add_dependencies(musikcore_uri_player_test httpdatastream)
    set(uri_http "$<TARGET_FILE:httpdatastream>")
  endif()
  add_test(NAME musikcore.integration.api.uri_player COMMAND "${MUSIKCORE_TEST_NODE}"
    "${PROJECT_SOURCE_DIR}/tests/scripts/check-uri-player.js" "${CMAKE_CURRENT_BINARY_DIR}/bin/isolated"
    "$<TARGET_FILE:musikcore_uri_player_test>" "$<TARGET_FILE:nullout>" ${uri_http})
  set_tests_properties(musikcore.integration.api.uri_player PROPERTIES
    LABELS "integration;musikcore;playback;contract" TIMEOUT 40)
endif()

# Keep runtime hardening coverage native on macOS and Linux, without devices.
add_executable(musikcore_runtime_hardening_test
  ../integration/musikcore/runtime-hardening.cpp
  ../integration/musikcore/runtime-hardening-context.cpp
  ../integration/musikcore/runtime-hardening-database.cpp
  ../integration/musikcore/runtime-hardening-plugins.cpp
  ../integration/musikcore/runtime-hardening-indexer.cpp
  host-wakeup.cpp)
target_include_directories(musikcore_runtime_hardening_test PRIVATE "${PROJECT_SOURCE_DIR}/src"
  "${PROJECT_SOURCE_DIR}/src/3rdparty/include/sqlite" ${VENDOR_INCLUDE_DIRECTORIES})
target_link_libraries(musikcore_runtime_hardening_test PRIVATE ${core_fixture_library} Threads::Threads)
set_target_properties(musikcore_runtime_hardening_test PROPERTIES DISABLE_PRECOMPILE_HEADERS ON)
if(WIN32)
  target_link_libraries(musikcore_runtime_hardening_test PRIVATE musikcube_windows vendor::ssl pdh psapi ws2_32 wldap32 comctl32)
endif()
add_dependencies(musikcube_tests musikcore_runtime_hardening_test)
add_dependencies(musikcube_integration_tests musikcore_runtime_hardening_test)
foreach(case IN ITEMS services plugin_directory registry startup owner read_isolation locked_read_shutdown database_failure)
  add_test(NAME musikcore.integration.runtime.${case} COMMAND "${MUSIKCORE_TEST_NODE}"
    "${PROJECT_SOURCE_DIR}/tests/scripts/run-isolated-test.js" "${CMAKE_CURRENT_BINARY_DIR}/bin/isolated"
    "$<TARGET_FILE:musikcore_runtime_hardening_test>" --stage ${case})
  set_tests_properties(musikcore.integration.runtime.${case} PROPERTIES
    LABELS "integration;musikcore;runtime;contract" TIMEOUT 30)
endforeach()

add_test(NAME musikcore.integration.runtime.indexer_cancel COMMAND "${MUSIKCORE_TEST_NODE}"
  "${PROJECT_SOURCE_DIR}/tests/scripts/run-isolated-test.js" "${CMAKE_CURRENT_BINARY_DIR}/bin/isolated"
  "$<TARGET_FILE:musikcore_runtime_hardening_test>" --stage indexer_cancel)
set_tests_properties(musikcore.integration.runtime.indexer_cancel PROPERTIES
  LABELS "integration;musikcore;runtime;indexer;contract" TIMEOUT 30)

add_library(musikcore_runtime_indexer MODULE ../integration/musikcore/runtime-indexer-plugin.cpp)
target_include_directories(musikcore_runtime_indexer PRIVATE "${PROJECT_SOURCE_DIR}/src" "${PROJECT_SOURCE_DIR}/include")
set_target_properties(musikcore_runtime_indexer PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/fixtures/runtime-indexer"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/fixtures/runtime-indexer"
  DISABLE_PRECOMPILE_HEADERS ON)
if(APPLE)
  set_target_properties(musikcore_runtime_indexer PROPERTIES SUFFIX ".dylib")
endif()
add_dependencies(musikcore_runtime_hardening_test musikcore_runtime_indexer)
add_test(NAME musikcore.integration.runtime.indexer_provider_cancel COMMAND "${MUSIKCORE_TEST_NODE}"
  "${PROJECT_SOURCE_DIR}/tests/scripts/run-isolated-test.js" "${CMAKE_CURRENT_BINARY_DIR}/bin/isolated"
  "$<TARGET_FILE:musikcore_runtime_hardening_test>" --stage --plugin "$<TARGET_FILE:musikcore_runtime_indexer>"
  indexer_provider_cancel)
set_tests_properties(musikcore.integration.runtime.indexer_provider_cancel PROPERTIES
  LABELS "integration;musikcore;runtime;indexer;contract" TIMEOUT 30)

# separate processes are essential: a static migration flag cannot survive them.
set(migration_media)
if(TARGET ffmpegdecoder AND (TARGET taglibreader OR TARGET taglib_plugin))
  add_dependencies(musikcore_runtime_hardening_test musikcube_seed_library ffmpegdecoder)
  if(TARGET taglibreader)
    add_dependencies(musikcore_runtime_hardening_test taglibreader)
  else()
    add_dependencies(musikcore_runtime_hardening_test taglib_plugin)
  endif()
  list(APPEND migration_media "$<TARGET_FILE:musikcube_seed_library>" "${PROJECT_BINARY_DIR}")
endif()
add_test(NAME musikcore.integration.runtime.migration_scan COMMAND "${MUSIKCORE_TEST_NODE}"
  "${PROJECT_SOURCE_DIR}/tests/scripts/check-migration-scan.js" "${CMAKE_CURRENT_BINARY_DIR}/bin/isolated"
  "$<TARGET_FILE:musikcore_runtime_hardening_test>" ${migration_media})
set_tests_properties(musikcore.integration.runtime.migration_scan PROPERTIES
  LABELS "integration;musikcore;runtime;indexer;contract" TIMEOUT 45)

# a headless provider makes cache retirement testable without platform UI plugins.
add_library(musikcore_runtime_visualizer MODULE ../integration/musikcore/runtime-visualizer-plugin.cpp)
target_include_directories(musikcore_runtime_visualizer PRIVATE "${PROJECT_SOURCE_DIR}/src" "${PROJECT_SOURCE_DIR}/include")
set_target_properties(musikcore_runtime_visualizer PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/fixtures/runtime-visualizer"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/fixtures/runtime-visualizer"
  DISABLE_PRECOMPILE_HEADERS ON)
if(APPLE)
  set_target_properties(musikcore_runtime_visualizer PROPERTIES SUFFIX ".dylib")
endif()
add_dependencies(musikcore_runtime_hardening_test musikcore_runtime_visualizer)
add_test(NAME musikcore.integration.runtime.visualizer_session COMMAND "${MUSIKCORE_TEST_NODE}"
  "${PROJECT_SOURCE_DIR}/tests/scripts/run-isolated-test.js" "${CMAKE_CURRENT_BINARY_DIR}/bin/isolated"
  "$<TARGET_FILE:musikcore_runtime_hardening_test>" --stage
  --plugin "$<TARGET_FILE:musikcore_runtime_visualizer>" visualizer_session)
set_tests_properties(musikcore.integration.runtime.visualizer_session PROPERTIES
  LABELS "integration;musikcore;runtime;visualizer;contract" TIMEOUT 30)

# SDK 22 objects must retire before unload, without querying their factories.
foreach(name IN ITEMS CurrentSdk ObsoleteSdk)
  add_library(musikcore_sdk_${name} MODULE ../integration/musikcore/sdk-version-plugin.cpp)
  target_include_directories(musikcore_sdk_${name} PRIVATE "${PROJECT_SOURCE_DIR}/src" "${PROJECT_SOURCE_DIR}/include")
  target_compile_definitions(musikcore_sdk_${name} PRIVATE SDK_FIXTURE_NAME="${name}")
  if(name STREQUAL "ObsoleteSdk")
    target_compile_definitions(musikcore_sdk_${name} PRIVATE SDK_FIXTURE_OBSOLETE)
  endif()
  set_target_properties(musikcore_sdk_${name} PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/fixtures/sdk-version"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/fixtures/sdk-version"
    DISABLE_PRECOMPILE_HEADERS ON)
  if(APPLE)
    set_target_properties(musikcore_sdk_${name} PROPERTIES SUFFIX ".dylib")
  endif()
  add_dependencies(musikcore_runtime_hardening_test musikcore_sdk_${name})
endforeach()
# Exercise nested stream adapters only when their actual providers are built.
set(stream_sdk_plugins)
foreach(provider IN ITEMS gmedecoder openmptdecoder)
  set(target "${provider}")
  if(provider STREQUAL "openmptdecoder" AND TARGET libopenmptdecoder)
    set(target libopenmptdecoder)
  endif()
  if(TARGET ${target})
    list(APPEND stream_sdk_plugins --plugin "$<TARGET_FILE:${target}>")
    target_compile_definitions(musikcore_runtime_hardening_test PRIVATE TEST_STREAM_${provider})
    add_dependencies(musikcore_runtime_hardening_test ${target})
  endif()
endforeach()
add_test(NAME musikcore.integration.runtime.stream_sdk_contract COMMAND "${MUSIKCORE_TEST_NODE}"
  "${PROJECT_SOURCE_DIR}/tests/scripts/run-isolated-test.js" "${CMAKE_CURRENT_BINARY_DIR}/bin/isolated"
  "$<TARGET_FILE:musikcore_runtime_hardening_test>" --stage
  --plugin "$<TARGET_FILE:musikcore_sdk_CurrentSdk>"
  --plugin "$<TARGET_FILE:musikcore_sdk_ObsoleteSdk>" ${stream_sdk_plugins} stream_sdk_contract)
set_tests_properties(musikcore.integration.runtime.stream_sdk_contract PROPERTIES
  LABELS "integration;musikcore;plugin;runtime;contract" TIMEOUT 30)

if(WIN32)
  set(windows_plugin_fixtures)
  foreach(provider IN ITEMS directsoundout wasapiout waveout win32gdivis win32globalhotkeys cddadecoder)
    add_dependencies(musikcore_runtime_hardening_test ${provider})
    list(APPEND windows_plugin_fixtures --plugin "$<TARGET_FILE:${provider}>")
  endforeach()
  add_test(NAME musikcore.integration.runtime.windows_plugins COMMAND "${MUSIKCORE_TEST_NODE}"
    "${PROJECT_SOURCE_DIR}/tests/scripts/run-isolated-test.js" "${CMAKE_CURRENT_BINARY_DIR}/bin/isolated"
    "$<TARGET_FILE:musikcore_runtime_hardening_test>" --stage ${windows_plugin_fixtures} windows_plugins)
  set_tests_properties(musikcore.integration.runtime.windows_plugins PROPERTIES
    LABELS "integration;musikcore;plugin;runtime;contract" TIMEOUT 30)
endif()

if(UNIX AND CMAKE_NM)
  musikcube_restrict_exports(musikcore_runtime_hardening_test)
  get_property(export_targets GLOBAL PROPERTY MUSIKCUBE_RESTRICTED_TARGETS)
  list(REMOVE_ITEM export_targets musikcore_abi0)
  foreach(target IN LISTS export_targets)
    add_dependencies(musikcube_tests ${target})
    add_dependencies(musikcube_integration_tests ${target})
    add_test(NAME musikcore.integration.binary.exports.${target} COMMAND "${CMAKE_COMMAND}"
      "-DAPPLE=${APPLE}" "-DNM=${CMAKE_NM}" "-DLIBRARY=$<TARGET_FILE:${target}>"
      "-DALLOWLIST=$<TARGET_PROPERTY:${target},MUSIKCUBE_EXPORT_ALLOWLIST>"
      -P "${PROJECT_SOURCE_DIR}/tests/scripts/check-exports.cmake")
    set_tests_properties(musikcore.integration.binary.exports.${target} PROPERTIES
      LABELS "integration;musikcore;packaging;contract" TIMEOUT 10)
  endforeach()
endif()

# Two independent legacy providers fail the same hook in the same session.
foreach(name IN ITEMS First Second)
  add_library(musikcore_runtime_failure_${name} MODULE ../integration/musikcore/runtime-failure-plugin.cpp)
  target_include_directories(musikcore_runtime_failure_${name} PRIVATE "${PROJECT_SOURCE_DIR}/src" "${PROJECT_SOURCE_DIR}/include")
  target_compile_definitions(musikcore_runtime_failure_${name} PRIVATE RUNTIME_FIXTURE_NAME="RuntimeFailure${name}")
  set_target_properties(musikcore_runtime_failure_${name} PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/fixtures/runtime-failure"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/fixtures/runtime-failure"
    DISABLE_PRECOMPILE_HEADERS ON)
  if(APPLE)
    set_target_properties(musikcore_runtime_failure_${name} PROPERTIES SUFFIX ".dylib")
  endif()
  add_dependencies(musikcore_runtime_hardening_test musikcore_runtime_failure_${name})
endforeach()
foreach(case IN ITEMS failure caches)
  add_test(NAME musikcore.integration.runtime.${case} COMMAND "${MUSIKCORE_TEST_NODE}"
    "${PROJECT_SOURCE_DIR}/tests/scripts/run-isolated-test.js" "${CMAKE_CURRENT_BINARY_DIR}/bin/isolated"
    "$<TARGET_FILE:musikcore_runtime_hardening_test>" --stage
    --plugin "$<TARGET_FILE:musikcore_runtime_failure_First>"
    --plugin "$<TARGET_FILE:musikcore_runtime_failure_Second>" ${case})
  set_tests_properties(musikcore.integration.runtime.${case} PROPERTIES
    LABELS "integration;musikcore;runtime;contract" TIMEOUT 30)
endforeach()

if(UNIX AND TARGET musikcubed)
  add_dependencies(musikcube_tests musikcubed)
  add_dependencies(musikcube_integration_tests musikcubed)
  add_test(NAME musikcubed.integration.lifecycle COMMAND "${MUSIKCORE_TEST_NODE}"
    "${PROJECT_SOURCE_DIR}/tests/scripts/check-daemon.js" "${CMAKE_CURRENT_BINARY_DIR}/bin/isolated"
    "$<TARGET_FILE:musikcubed>")
  set_tests_properties(musikcubed.integration.lifecycle PROPERTIES
    LABELS "integration;musikcubed;runtime;contract" TIMEOUT 20)
endif()

# offline startup must not contact a saved remote before the host opts in.
add_executable(musikcore_runtime_local_startup_test ../integration/musikcore/runtime-local-startup.cpp)
target_include_directories(musikcore_runtime_local_startup_test PRIVATE
  "${PROJECT_SOURCE_DIR}/src" ${VENDOR_INCLUDE_DIRECTORIES})
target_link_libraries(musikcore_runtime_local_startup_test PRIVATE ${core_fixture_library} Threads::Threads)
target_compile_features(musikcore_runtime_local_startup_test PRIVATE cxx_std_17)
set_target_properties(musikcore_runtime_local_startup_test PROPERTIES DISABLE_PRECOMPILE_HEADERS ON)
if(WIN32)
  target_link_libraries(musikcore_runtime_local_startup_test PRIVATE
    musikcube_windows vendor::ssl pdh psapi ws2_32 wldap32 comctl32)
endif()
add_dependencies(musikcube_tests musikcore_runtime_local_startup_test)
add_dependencies(musikcube_integration_tests musikcore_runtime_local_startup_test)
add_test(NAME musikcore.integration.runtime.local_startup COMMAND "${MUSIKCORE_TEST_NODE}"
  "${PROJECT_SOURCE_DIR}/tests/scripts/run-isolated-test.js" "${CMAKE_CURRENT_BINARY_DIR}/bin/isolated"
  "$<TARGET_FILE:musikcore_runtime_local_startup_test>" --stage)
set_tests_properties(musikcore.integration.runtime.local_startup PROPERTIES
  LABELS "integration;musikcore;runtime;transport;contract" TIMEOUT 25)

# contextual facets use the existing remote category protocol without client composition.
add_executable(musikcore_remote_facets_test ../integration/transport/remote-facets.cpp)
target_include_directories(musikcore_remote_facets_test PRIVATE
  "${PROJECT_SOURCE_DIR}/src" "${CMAKE_CURRENT_SOURCE_DIR}" ${VENDOR_INCLUDE_DIRECTORIES})
target_link_libraries(musikcore_remote_facets_test PRIVATE ${core_fixture_library} Threads::Threads)
target_compile_features(musikcore_remote_facets_test PRIVATE cxx_std_17)
set_target_properties(musikcore_remote_facets_test PROPERTIES DISABLE_PRECOMPILE_HEADERS ON)
if(WIN32)
  target_link_libraries(musikcore_remote_facets_test PRIVATE
    musikcube_windows vendor::ssl pdh psapi ws2_32 wldap32 comctl32)
endif()
add_dependencies(musikcube_tests musikcore_remote_facets_test)
add_dependencies(musikcube_integration_tests musikcore_remote_facets_test)
add_test(NAME musikcore.integration.api.remote_facets COMMAND "${MUSIKCORE_TEST_NODE}"
  "${PROJECT_SOURCE_DIR}/tests/scripts/run-isolated-test.js" "${CMAKE_CURRENT_BINARY_DIR}/bin/isolated"
  "$<TARGET_FILE:musikcore_remote_facets_test>" --stage)
set_tests_properties(musikcore.integration.api.remote_facets PROPERTIES
  LABELS "integration;musikcore;api;transport;contract" TIMEOUT 25)
