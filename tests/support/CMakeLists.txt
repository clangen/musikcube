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

  add_executable(musikcore_playlist_track_queries_test ../integration/library/playlist-track-queries.cpp)
  target_include_directories(musikcore_playlist_track_queries_test PRIVATE
    "${PROJECT_SOURCE_DIR}/src" ${VENDOR_INCLUDE_DIRECTORIES})
  target_link_libraries(musikcore_playlist_track_queries_test PRIVATE ${core_fixture_library} Threads::Threads)
  if(WIN32)
    target_link_libraries(musikcore_playlist_track_queries_test PRIVATE
      musikcube_windows vendor::ssl pdh psapi ws2_32 wldap32 comctl32)
  endif()
  add_dependencies(musikcube_tests musikcore_playlist_track_queries_test)
  add_dependencies(musikcube_integration_tests musikcore_playlist_track_queries_test)
  add_test(NAME musikcore.integration.library.playlist_track_queries COMMAND musikcore_playlist_track_queries_test)
  set_tests_properties(musikcore.integration.library.playlist_track_queries PROPERTIES
    LABELS "integration;musikcore;library;contract")

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
    "$<TARGET_FILE:musikcore_remote_query_transport_test>" --stage --managed
    "$<TARGET_FILE:musikcore_playback_remote_fixture>")
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
      target_compile_definitions(musikcore_pcm_capture_output PRIVATE MC_BUILDING_PLUGIN)
      target_link_libraries(musikcore_pcm_capture_output PRIVATE Threads::Threads)
      set_target_properties(musikcore_pcm_capture_output PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin/fixtures"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin/fixtures"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin/fixtures"
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

      foreach(mode IN ITEMS
          gapless crossfade teardown seek seek_eof seek_tail completion replace_next
          paused_crossfade paused_runtime paused_short_runtime retired_crossfade failed_next_crossfade
          replace_paused_crossfade replace_stopped_crossfade replace_failed_crossfade seek_paused_crossfade
          cancel_open output_queue_failure output_callback_failure
          gapless_control_failure crossfade_control_failure uri_control_failure blocked_control)
        add_test(NAME musikcore.integration.transport.${mode}_pcm COMMAND "${MUSIKCUBE_TEST_NODE}"
          "${PROJECT_SOURCE_DIR}/tests/scripts/run-isolated-test.js" "${CMAKE_CURRENT_BINARY_DIR}/bin/isolated"
          "$<TARGET_FILE:musikcore_playback_pcm_test>" --stage
          --plugin "$<TARGET_FILE:musikcore_pcm_capture_output>" "${mode}"
          "$<TARGET_FILE_NAME:musikcore_pcm_capture_output>" @origin)
        set_tests_properties(musikcore.integration.transport.${mode}_pcm PROPERTIES
          LABELS "integration;musikcore;transport;playback" TIMEOUT 35)
      endforeach()
      add_dependencies(musikcore_playback_pcm_test musikcore_byte_decoder)
      foreach(mode IN ITEMS decoder_contract decoder_drain)
        add_test(NAME musikcore.integration.transport.${mode}_pcm COMMAND "${MUSIKCORE_TEST_NODE}"
          "${PROJECT_SOURCE_DIR}/tests/scripts/run-isolated-test.js" "${CMAKE_CURRENT_BINARY_DIR}/bin/isolated"
          "$<TARGET_FILE:musikcore_playback_pcm_test>" --stage --plugin "$<TARGET_FILE:musikcore_byte_decoder>"
          --plugin "$<TARGET_FILE:musikcore_pcm_capture_output>" ${mode}
          "$<TARGET_FILE_NAME:musikcore_pcm_capture_output>" @origin)
        set_tests_properties(musikcore.integration.transport.${mode}_pcm PROPERTIES
          LABELS "integration;musikcore;transport;playback;contract" TIMEOUT 30)
      endforeach()
      add_test(NAME musikcore.integration.plugins.partial_media_create COMMAND "${MUSIKCORE_TEST_NODE}"
        "${PROJECT_SOURCE_DIR}/tests/scripts/run-isolated-test.js" "${CMAKE_CURRENT_BINARY_DIR}/bin/isolated"
        "$<TARGET_FILE:musikcore_playback_pcm_test>" --expect-abort --stage
        --plugin "$<TARGET_FILE:musikcore_pcm_capture_output>" partial_media_create)
      set_tests_properties(musikcore.integration.plugins.partial_media_create PROPERTIES
        LABELS "integration;musikcore;plugins;contract" TIMEOUT 30
        PASS_REGULAR_EXPRESSION "failed media factory returned an instance"
        FAIL_REGULAR_EXPRESSION "fatal-test: stack unwound;Test failed")
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
          foreach(mode IN ITEMS mp3 flac mp3_sync mp3_range_fallback mp3_no_cache)
            set(format "${mode}")
            set(sync OFF)
            set(range_fallback OFF)
            set(no_cache OFF)
            if(mode STREQUAL "mp3_sync")
              set(format mp3)
              set(sync ON)
            elseif(mode STREQUAL "mp3_range_fallback")
              set(format mp3)
              set(range_fallback ON)
            elseif(mode STREQUAL "mp3_no_cache")
              set(format mp3)
              set(no_cache ON)
            endif()
            add_test(NAME musikcore.integration.api.remote_transcode.${mode} COMMAND "${CMAKE_COMMAND}"
              ${indexed_arguments} "-DMANAGED_RUNTIME_TEST=$<TARGET_FILE:musikcore_managed_runtime_test>"
              "-DSERVER_PLUGIN=$<TARGET_FILE:server>" "-DHTTP_DATASTREAM_PLUGIN=$<TARGET_FILE:httpdatastream>"
              "-DNULL_OUTPUT=$<TARGET_FILE:nullout>" "-DENCODER_PLUGIN=$<TARGET_FILE:stockencoders>"
              "-DTRANSCODE_FORMAT=${format}" "-DSYNCHRONOUS_TRANSCODE=${sync}"
              "-DRANGE_FALLBACK=${range_fallback}" "-DNO_TRANSCODE_CACHE=${no_cache}" -P "${indexed_script}")
            set_tests_properties(musikcore.integration.api.remote_transcode.${mode} PROPERTIES
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
  if(UNIX)
    add_test(NAME musikcube.ui.term COMMAND "${MUSIKCORE_TEST_NODE}"
      "${PROJECT_SOURCE_DIR}/tests/ui/scenarios/term.js"
      --build-dir "${PROJECT_BINARY_DIR}" --bin-dir "$<TARGET_FILE_DIR:${ui_frontend}>" --tracks 20)
    set_tests_properties(musikcube.ui.term PROPERTIES LABELS "ui;musikcube;terminal;contract" TIMEOUT 90)
  endif()
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
# reuse the C registry host to exercise the production HTTP source directly.
# decoder prebuffering otherwise hides the point at which a byte read blocks.
if(TARGET httpdatastream)
  target_sources(musikcore_decoder_stream_test PRIVATE "${PROJECT_SOURCE_DIR}/tests/integration/plugins/http-read.cpp")
  target_include_directories(musikcore_decoder_stream_test PRIVATE "${PROJECT_SOURCE_DIR}/src/3rdparty/asio/asio/include")
  target_compile_definitions(musikcore_decoder_stream_test PRIVATE MUSIKCORE_TEST_HTTP_SOURCE ASIO_STANDALONE)
  add_dependencies(musikcore_decoder_stream_test httpdatastream)
  add_test(NAME musikcore.integration.plugins.progressive_http_read
    COMMAND musikcore_decoder_stream_test --progressive-http "$<TARGET_FILE:httpdatastream>")
  add_test(NAME musikcore.integration.plugins.shared_http_cache
    COMMAND "${MUSIKCORE_TEST_NODE}" "${PROJECT_SOURCE_DIR}/tests/scripts/check-http-cache.js"
      "$<TARGET_FILE:musikcore_decoder_stream_test>" "$<TARGET_FILE:httpdatastream>")
  set_tests_properties(musikcore.integration.plugins.progressive_http_read musikcore.integration.plugins.shared_http_cache PROPERTIES
    LABELS "integration;musikcore;plugins;contract" TIMEOUT 20)
endif()

if(TARGET nullout AND TARGET ffmpegdecoder)
  foreach(language IN ITEMS c cpp)
    add_executable(musikcore_example_player_${language} "${PROJECT_SOURCE_DIR}/examples/core-player.${language}" host-wakeup.cpp)
    target_include_directories(musikcore_example_player_${language} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(musikcore_example_player_${language} PRIVATE musikcore Threads::Threads)
    add_dependencies(musikcube_tests musikcore_example_player_${language})
  endforeach()
  add_executable(musikcore_uri_player_test ../integration/musikcore/uri-player.cpp host-wakeup.cpp)
  target_compile_features(musikcore_uri_player_test PRIVATE cxx_std_20)
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
if(TARGET server)
  target_sources(musikcore_runtime_hardening_test PRIVATE
    ../integration/plugins/server-transcode.cpp
    "${PROJECT_SOURCE_DIR}/src/plugins/server/ServerMedia.cpp"
    "${PROJECT_SOURCE_DIR}/src/plugins/server/BlockingTranscoder.cpp"
    "${PROJECT_SOURCE_DIR}/src/plugins/server/TranscodingAudioDataStream.cpp")
  target_compile_definitions(musikcore_runtime_hardening_test PRIVATE TEST_SERVER_TRANSCODE)
  add_dependencies(musikcore_runtime_hardening_test musikcore_byte_decoder)
  set(server_encoder_fixture)
  if(TARGET stockencoders)
    target_compile_definitions(musikcore_runtime_hardening_test PRIVATE TEST_SERVER_STOCK_ENCODER)
    add_dependencies(musikcore_runtime_hardening_test stockencoders)
    set(server_encoder_fixture --plugin "$<TARGET_FILE:stockencoders>")
  endif()
  add_test(NAME musikcore.integration.server.transcode_body COMMAND "${MUSIKCORE_TEST_NODE}"
    "${PROJECT_SOURCE_DIR}/tests/scripts/run-isolated-test.js" "${CMAKE_CURRENT_BINARY_DIR}/bin/isolated"
    "$<TARGET_FILE:musikcore_runtime_hardening_test>" --stage --plugin "$<TARGET_FILE:musikcore_byte_decoder>" ${server_encoder_fixture} server_transcode)
  set_tests_properties(musikcore.integration.server.transcode_body PROPERTIES LABELS "integration;server;audio;contract" TIMEOUT 30)
endif()
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
target_compile_definitions(musikcore_runtime_indexer PRIVATE MC_BUILDING_PLUGIN)
set_target_properties(musikcore_runtime_indexer PROPERTIES
  ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin/fixtures/runtime-indexer"
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin/fixtures/runtime-indexer"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin/fixtures/runtime-indexer"
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
target_compile_definitions(musikcore_runtime_visualizer PRIVATE MC_BUILDING_PLUGIN)
target_include_directories(musikcore_runtime_visualizer PRIVATE "${PROJECT_SOURCE_DIR}/src" "${PROJECT_SOURCE_DIR}/include")
set_target_properties(musikcore_runtime_visualizer PROPERTIES
  ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin/fixtures/runtime-visualizer"
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin/fixtures/runtime-visualizer"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin/fixtures/runtime-visualizer"
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

# Old entry points must not be called during C plugin discovery.
add_library(musikcore_legacy_entry_fixture MODULE plugins/legacy-entry-sentinel.cpp)
target_include_directories(musikcore_legacy_entry_fixture PRIVATE "${PROJECT_SOURCE_DIR}/include")
set_target_properties(musikcore_legacy_entry_fixture PROPERTIES
  ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin/fixtures/legacy-entry"
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin/fixtures/legacy-entry"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin/fixtures/legacy-entry"
  DISABLE_PRECOMPILE_HEADERS ON)
if(APPLE)
  set_target_properties(musikcore_legacy_entry_fixture PROPERTIES SUFFIX ".dylib")
endif()
add_dependencies(musikcore_runtime_hardening_test musikcore_legacy_entry_fixture)
# exercise the real virtual-URI providers when their optional codec libraries are installed.
if(TARGET taglibreader OR TARGET ffmpegdecoder OR TARGET gmedecoder OR TARGET openmptdecoder OR TARGET libopenmptdecoder)
  add_executable(musikcore_codec_providers_test "${PROJECT_SOURCE_DIR}/tests/integration/plugins/codec-providers.cpp"
    "${PROJECT_SOURCE_DIR}/src/musikcore/io/LocalFileSource.cpp"
    "${PROJECT_SOURCE_DIR}/src/musikcore/ffi/plugin_abi/AudioScratchBuffers.cpp"
    "${PROJECT_SOURCE_DIR}/src/musikcore/ffi/plugin_abi/PluginRegistry.cpp"
    "${PROJECT_SOURCE_DIR}/src/musikcore/ffi/plugin_abi/Plugin.cpp"
    "${PROJECT_SOURCE_DIR}/src/musikcore/ffi/common/error.cpp")
  target_include_directories(musikcore_codec_providers_test PRIVATE "${PROJECT_SOURCE_DIR}/include" "${PROJECT_SOURCE_DIR}/src")
  target_compile_features(musikcore_codec_providers_test PRIVATE cxx_std_20)
  target_link_libraries(musikcore_codec_providers_test PRIVATE Threads::Threads ${CMAKE_DL_LIBS})
  add_dependencies(musikcube_tests musikcore_codec_providers_test)
  add_dependencies(musikcube_integration_tests musikcore_codec_providers_test)
  foreach(provider IN ITEMS taglibreader ffmpegdecoder gmedecoder openmptdecoder libopenmptdecoder)
    if(TARGET ${provider})
      if(provider STREQUAL taglibreader)
        set(family taglib)
        set(scenario taglib_metadata)
      elseif(provider STREQUAL ffmpegdecoder)
        set(family ffmpeg)
        set(scenario ffmpeg_resample)
      elseif(provider STREQUAL gmedecoder)
        set(scenario subsong_gme)
        set(family gme)
      else()
        set(family openmpt)
        set(scenario subsong_openmpt)
      endif()
      add_dependencies(musikcore_codec_providers_test ${provider})
      add_test(NAME musikcore.integration.plugins.${scenario}
        COMMAND "${MUSIKCORE_TEST_NODE}" "${PROJECT_SOURCE_DIR}/tests/scripts/run-isolated-test.js"
        "${CMAKE_CURRENT_BINARY_DIR}/bin/isolated" "$<TARGET_FILE:musikcore_codec_providers_test>" --stage
        --plugin "$<TARGET_FILE:${provider}>" @origin "${family}")
      set_tests_properties(musikcore.integration.plugins.${scenario} PROPERTIES
        LABELS "integration;musikcore;plugins;audio;contract" TIMEOUT 20)
    endif()
  endforeach()
endif()

# exercise the real nested sources through the runtime when their optional libraries are installed.
add_library(musikcore_indexer_gate MODULE ../integration/plugins/indexer-gate-plugin.cpp)
target_include_directories(musikcore_indexer_gate PRIVATE "${PROJECT_SOURCE_DIR}/include")
target_compile_features(musikcore_indexer_gate PRIVATE cxx_std_20)
musikcube_configure_c_plugin(musikcore_indexer_gate)
set_target_properties(musikcore_indexer_gate PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin/fixtures/indexer-gate"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin/fixtures/indexer-gate")
if(APPLE)
  set_target_properties(musikcore_indexer_gate PROPERTIES SUFFIX ".dylib")
endif()
add_executable(musikcore_indexer_gate_test ../integration/plugins/indexer-gate.cpp host-wakeup.cpp)
target_include_directories(musikcore_indexer_gate_test PRIVATE "${PROJECT_SOURCE_DIR}/src"
  "${PROJECT_SOURCE_DIR}/src/3rdparty/include/sqlite")
target_link_libraries(musikcore_indexer_gate_test PRIVATE ${core_fixture_library} Threads::Threads)
target_compile_features(musikcore_indexer_gate_test PRIVATE cxx_std_20)
set_target_properties(musikcore_indexer_gate_test PROPERTIES DISABLE_PRECOMPILE_HEADERS ON)
if(WIN32)
  target_link_libraries(musikcore_indexer_gate_test PRIVATE
    musikcube_windows vendor::ssl pdh psapi ws2_32 wldap32 comctl32)
endif()
add_dependencies(musikcore_indexer_gate_test musikcore_indexer_gate)
add_dependencies(musikcube_tests musikcore_indexer_gate_test)
add_dependencies(musikcube_integration_tests musikcore_indexer_gate_test)
foreach(mode IN ITEMS cancel fail database ignore)
  add_test(NAME musikcore.integration.plugins.indexer_${mode}
    COMMAND "${MUSIKCORE_TEST_NODE}" "${PROJECT_SOURCE_DIR}/tests/scripts/run-isolated-test.js"
    "${CMAKE_CURRENT_BINARY_DIR}/bin/isolated" "$<TARGET_FILE:musikcore_indexer_gate_test>" --stage
    --plugin "$<TARGET_FILE:musikcore_indexer_gate>" "${mode}")
  set_tests_properties(musikcore.integration.plugins.indexer_${mode} PROPERTIES
    LABELS "integration;musikcore;plugins;indexer;contract" TIMEOUT 30)
endforeach()

# a source that fails to initialize must stop before a scan can remove its tracks.
add_test(NAME musikcore.integration.plugins.indexer_create-fail
  COMMAND "${CMAKE_COMMAND}"
    "-DNODE=${MUSIKCORE_TEST_NODE}"
    "-DRUNNER=${PROJECT_SOURCE_DIR}/tests/scripts/run-isolated-test.js"
    "-DROOT=${CMAKE_CURRENT_BINARY_DIR}/bin/isolated"
    "-DTEST_BINARY=$<TARGET_FILE:musikcore_indexer_gate_test>"
    "-DPLUGIN=$<TARGET_FILE:musikcore_indexer_gate>"
    -P "${PROJECT_SOURCE_DIR}/tests/scripts/check-indexer-source-fatal.cmake")
set_tests_properties(musikcore.integration.plugins.indexer_create-fail PROPERTIES
  LABELS "integration;musikcore;plugins;indexer;contract" TIMEOUT 30)

if(TARGET gmedecoder OR TARGET openmptdecoder OR TARGET libopenmptdecoder)
  add_executable(musikcore_indexer_providers_test
    ../integration/plugins/indexer-providers.cpp host-wakeup.cpp)
  target_include_directories(musikcore_indexer_providers_test PRIVATE "${PROJECT_SOURCE_DIR}/src"
    "${PROJECT_SOURCE_DIR}/src/3rdparty/include/sqlite")
  target_link_libraries(musikcore_indexer_providers_test PRIVATE ${core_fixture_library} Threads::Threads)
  target_compile_features(musikcore_indexer_providers_test PRIVATE cxx_std_20)
  set_target_properties(musikcore_indexer_providers_test PROPERTIES DISABLE_PRECOMPILE_HEADERS ON)
  if(WIN32)
    target_link_libraries(musikcore_indexer_providers_test PRIVATE
      musikcube_windows vendor::ssl pdh psapi ws2_32 wldap32 comctl32)
  endif()
  add_dependencies(musikcube_tests musikcore_indexer_providers_test)
  add_dependencies(musikcube_integration_tests musikcore_indexer_providers_test)
  foreach(provider IN ITEMS gme openmpt)
    set(indexer_target ${provider}decoder)
    if(provider STREQUAL "openmpt" AND TARGET libopenmptdecoder)
      set(indexer_target libopenmptdecoder)
    endif()
    if(TARGET ${indexer_target})
      add_dependencies(musikcore_indexer_providers_test ${indexer_target})
      add_test(NAME musikcore.integration.plugins.indexer_${provider}
        COMMAND "${MUSIKCORE_TEST_NODE}" "${PROJECT_SOURCE_DIR}/tests/scripts/run-isolated-test.js"
        "${CMAKE_CURRENT_BINARY_DIR}/bin/isolated" "$<TARGET_FILE:musikcore_indexer_providers_test>" --stage
        --plugin "$<TARGET_FILE:${indexer_target}>" @origin "${provider}")
      set_tests_properties(musikcore.integration.plugins.indexer_${provider} PROPERTIES
        LABELS "integration;musikcore;plugins;indexer;contract" TIMEOUT 60)
    endif()
  endforeach()
endif()

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
  --plugin "$<TARGET_FILE:musikcore_legacy_entry_fixture>" ${stream_sdk_plugins} stream_sdk_contract)
set_tests_properties(musikcore.integration.runtime.stream_sdk_contract PROPERTIES
  LABELS "integration;musikcore;plugin;runtime;contract" TIMEOUT 30)

if(WIN32)
  add_executable(musikcore_cdda_plugin_test ../integration/plugins/cdda-plugin.cpp)
  target_include_directories(musikcore_cdda_plugin_test PRIVATE "${PROJECT_SOURCE_DIR}/include")
  target_compile_features(musikcore_cdda_plugin_test PRIVATE cxx_std_20)
  add_dependencies(musikcore_cdda_plugin_test cddadecoder)
  add_dependencies(musikcube_tests musikcore_cdda_plugin_test)
  add_dependencies(musikcube_integration_tests musikcore_cdda_plugin_test)
  add_test(NAME musikcore.integration.plugins.cdda_plugin
    COMMAND musikcore_cdda_plugin_test "$<TARGET_FILE:cddadecoder>")
  set_tests_properties(musikcore.integration.plugins.cdda_plugin PROPERTIES
    LABELS "integration;musikcore;plugins;audio;contract" TIMEOUT 20)
  add_executable(musikcore_cdda_stream_test ../integration/plugins/cdda-stream.cpp
    ../../src/plugins/cddadecoder/CddaStream.cpp ../../src/plugins/cddadecoder/CddaPcmDecoder.cpp
    ../../src/plugins/cddadecoder/CddaMetadata.cpp)
  target_include_directories(musikcore_cdda_stream_test PRIVATE "${PROJECT_SOURCE_DIR}/include"
    "${PROJECT_SOURCE_DIR}/src/3rdparty/include")
  target_compile_features(musikcore_cdda_stream_test PRIVATE cxx_std_20)
  target_link_libraries(musikcore_cdda_stream_test PRIVATE musikcube_windows bcrypt vendor::curl)
  add_dependencies(musikcube_tests musikcore_cdda_stream_test)
  add_dependencies(musikcube_integration_tests musikcore_cdda_stream_test)
  add_test(NAME musikcore.integration.plugins.cdda_stream COMMAND musikcore_cdda_stream_test)
  set_tests_properties(musikcore.integration.plugins.cdda_stream PROPERTIES
    LABELS "integration;musikcore;plugins;audio" TIMEOUT 20)
  add_executable(musikcore_directsound_ring_test ../unit/musikcore/directsound-ring.cpp
    ../../src/plugins/directsoundout/DirectSoundDevices.cpp)
  target_include_directories(musikcore_directsound_ring_test PRIVATE "${PROJECT_SOURCE_DIR}/include" "${PROJECT_SOURCE_DIR}/src")
  target_compile_features(musikcore_directsound_ring_test PRIVATE cxx_std_20)
  target_link_libraries(musikcore_directsound_ring_test PRIVATE dsound dxguid ole32)
  add_dependencies(musikcube_tests musikcore_directsound_ring_test)
  add_dependencies(musikcube_unit_tests musikcore_directsound_ring_test)
  add_test(NAME musikcore.unit.plugins.directsound_ring COMMAND musikcore_directsound_ring_test)
  set_tests_properties(musikcore.unit.plugins.directsound_ring PROPERTIES
    LABELS "unit;musikcore;plugins;audio" TIMEOUT 20)
  add_executable(musikcore_directsound_output_test ../integration/plugins/directsound-output.cpp)
  target_include_directories(musikcore_directsound_output_test PRIVATE "${PROJECT_SOURCE_DIR}/include")
  target_compile_features(musikcore_directsound_output_test PRIVATE cxx_std_20)
  add_dependencies(musikcore_directsound_output_test directsoundout)
  add_dependencies(musikcube_tests musikcore_directsound_output_test)
  add_dependencies(musikcube_integration_tests musikcore_directsound_output_test)
  add_test(NAME musikcore.integration.plugins.directsound_output
    COMMAND musikcore_directsound_output_test "$<TARGET_FILE:directsoundout>")
  set_tests_properties(musikcore.integration.plugins.directsound_output PROPERTIES
    LABELS "integration;musikcore;plugins;audio;contract" TIMEOUT 20)
  add_executable(musikcore_wasapi_output_test ../integration/plugins/wasapi-output.cpp)
  target_include_directories(musikcore_wasapi_output_test PRIVATE "${PROJECT_SOURCE_DIR}/include")
  target_compile_features(musikcore_wasapi_output_test PRIVATE cxx_std_20)
  add_dependencies(musikcore_wasapi_output_test wasapiout)
  add_dependencies(musikcube_tests musikcore_wasapi_output_test)
  add_dependencies(musikcube_integration_tests musikcore_wasapi_output_test)
  add_test(NAME musikcore.integration.plugins.wasapi_output
    COMMAND musikcore_wasapi_output_test "$<TARGET_FILE:wasapiout>")
  set_tests_properties(musikcore.integration.plugins.wasapi_output PROPERTIES
    LABELS "integration;musikcore;plugins;audio;contract" TIMEOUT 20)
  set(windows_plugin_fixtures)
  foreach(provider IN ITEMS nullout directsoundout wasapiout win32gdivis win32globalhotkeys cddadecoder)
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

add_test(NAME musikcore.integration.runtime.caches COMMAND "${MUSIKCORE_TEST_NODE}"
  "${PROJECT_SOURCE_DIR}/tests/scripts/run-isolated-test.js" "${CMAKE_CURRENT_BINARY_DIR}/bin/isolated"
  "$<TARGET_FILE:musikcore_runtime_hardening_test>" --stage caches)
set_tests_properties(musikcore.integration.runtime.caches PROPERTIES
  LABELS "integration;musikcore;runtime;contract" TIMEOUT 30)

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

# exercise provider discovery through the shared public application boundary.
add_executable(musikcore_provider_runtime_test ../integration/plugins/runtime.cpp host-wakeup.cpp)
target_include_directories(musikcore_provider_runtime_test PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(musikcore_provider_runtime_test PRIVATE musikcore)
target_compile_features(musikcore_provider_runtime_test PRIVATE cxx_std_17)
set_target_properties(musikcore_provider_runtime_test PROPERTIES DISABLE_PRECOMPILE_HEADERS ON)
add_dependencies(musikcube_tests musikcore_provider_runtime_test musikcore_provider_valid)
add_dependencies(musikcube_integration_tests musikcore_provider_runtime_test musikcore_provider_valid)
add_test(NAME musikcore.integration.plugins.runtime COMMAND "${MUSIKCORE_TEST_NODE}"
  "${PROJECT_SOURCE_DIR}/tests/scripts/run-isolated-test.js" "${CMAKE_CURRENT_BINARY_DIR}/bin/isolated"
  "$<TARGET_FILE:musikcore_provider_runtime_test>" --stage-minimal --plugin "$<TARGET_FILE:musikcore_provider_valid>"
  --plugin "$<TARGET_FILE:musikcore_provider_incompatible>" --plugin "$<TARGET_FILE:musikcore_provider_init_fail>")
set_tests_properties(musikcore.integration.plugins.runtime PROPERTIES
  LABELS "integration;musikcore;plugins;runtime;contract" TIMEOUT 45)
set(installed_decoder_arguments)
if(TARGET musikcore_playback_pcm_test AND TARGET musikcore_pcm_capture_output)
  list(APPEND installed_decoder_arguments
    "-DNODE=${MUSIKCORE_TEST_NODE}"
    "-DDECODER_HOST=$<TARGET_FILE:musikcore_playback_pcm_test>"
    "-DCAPTURE_OUTPUT=$<TARGET_FILE:musikcore_pcm_capture_output>")
endif()
add_test(NAME musikcore.integration.plugins.installed COMMAND "${CMAKE_COMMAND}"
  "-DBUILD_DIR=${PROJECT_BINARY_DIR}" "-DSOURCE=${PROJECT_SOURCE_DIR}" "-DTEST_CONFIG=$<CONFIG>"
  "-DHOST=$<TARGET_FILE:musikcore_plugin_registry_test>" "-DNM=${CMAKE_NM}"
  "-DTEST_GENERATOR=${CMAKE_GENERATOR}" "-DTEST_PLATFORM=${CMAKE_GENERATOR_PLATFORM}"
  "-DOUTPUT_HOST=$<TARGET_FILE:musikcore_null_output_test>"
  "-DCONSUMER_SANITIZERS=${sanitizers_flags}"
  ${installed_decoder_arguments}
  -P "${PROJECT_SOURCE_DIR}/tests/scripts/check-installed-provider.cmake")
set_tests_properties(musikcore.integration.plugins.installed PROPERTIES
  LABELS "integration;musikcore;plugins;packaging;contract" TIMEOUT 90)

# Exercise the actual production DLL through its sole C entry, without compiling
# its implementation into the host or linking it against musikcore.
if(TARGET nullout)
  musikcore_add_null_output_contract_test()

endif()

# native hardware coverage also drives the public URI player through the
# existing decoder. the isolated runner protects user catalogs/preferences.
if(APPLE AND TARGET coreaudioout AND TARGET ffmpegdecoder)
  add_executable(musikcore_coreaudio_output_test ../integration/plugins/coreaudio-output.cpp host-wakeup.cpp)
  target_include_directories(musikcore_coreaudio_output_test PRIVATE "${PROJECT_SOURCE_DIR}/src" "${PROJECT_SOURCE_DIR}/include")
  target_compile_features(musikcore_coreaudio_output_test PRIVATE cxx_std_20)
  target_link_libraries(musikcore_coreaudio_output_test PRIVATE ${core_fixture_library} Threads::Threads ${CMAKE_DL_LIBS})
  set_target_properties(musikcore_coreaudio_output_test PROPERTIES DISABLE_PRECOMPILE_HEADERS ON)
  add_dependencies(musikcore_coreaudio_output_test coreaudioout ffmpegdecoder)
  add_dependencies(musikcube_tests musikcore_coreaudio_output_test)
  add_dependencies(musikcube_integration_tests musikcore_coreaudio_output_test)
  add_test(NAME musikcore.integration.plugins.coreaudio_output COMMAND "${MUSIKCORE_TEST_NODE}"
    "${PROJECT_SOURCE_DIR}/tests/scripts/run-isolated-test.js" "${CMAKE_CURRENT_BINARY_DIR}/bin/isolated"
    "$<TARGET_FILE:musikcore_coreaudio_output_test>" "$<TARGET_FILE:coreaudioout>" "$<TARGET_FILE:ffmpegdecoder>")
  set_tests_properties(musikcore.integration.plugins.coreaudio_output PROPERTIES
    LABELS "integration;musikcore;plugins;audio;hardware;contract" SKIP_RETURN_CODE 77 TIMEOUT 30)
endif()


if(APPLE AND TARGET portaudioout)
  add_executable(musikcore_portaudio_failure_test
    ../unit/musikcore/portaudio-output.cpp "${PROJECT_SOURCE_DIR}/src/plugins/portaudioout/PortAudioOut.cpp")
  target_include_directories(musikcore_portaudio_failure_test PRIVATE
    "${PROJECT_SOURCE_DIR}/include" "${PROJECT_SOURCE_DIR}/src" "${PORTAUDIO_INCLUDE_DIR}")
  target_compile_features(musikcore_portaudio_failure_test PRIVATE cxx_std_20)
  target_link_libraries(musikcore_portaudio_failure_test PRIVATE Threads::Threads)
  add_dependencies(musikcube_tests musikcore_portaudio_failure_test)
  add_test(NAME musikcore.unit.plugins.portaudio_failure COMMAND musikcore_portaudio_failure_test)
  set_tests_properties(musikcore.unit.plugins.portaudio_failure PROPERTIES
    LABELS "unit;musikcore;plugins;audio;contract" TIMEOUT 10)

  add_executable(musikcore_portaudio_output_test ../integration/plugins/portaudio-output.cpp)
  target_include_directories(musikcore_portaudio_output_test PRIVATE "${PROJECT_SOURCE_DIR}/include")
  target_compile_features(musikcore_portaudio_output_test PRIVATE cxx_std_20)
  target_link_libraries(musikcore_portaudio_output_test PRIVATE ${CMAKE_DL_LIBS} Threads::Threads)
  add_dependencies(musikcore_portaudio_output_test portaudioout)
  add_dependencies(musikcube_tests musikcore_portaudio_output_test)
  add_dependencies(musikcube_integration_tests musikcore_portaudio_output_test)
  add_test(NAME musikcore.integration.plugins.portaudio_output
    COMMAND musikcore_portaudio_output_test "$<TARGET_FILE:portaudioout>")
  set_tests_properties(musikcore.integration.plugins.portaudio_output PROPERTIES
    LABELS "integration;musikcore;plugins;audio;hardware;contract" SKIP_RETURN_CODE 77 TIMEOUT 30)
endif()

if(APPLE AND TARGET macosmediakeys)
  find_library(MACOS_MEDIA_KEYS_APPKIT AppKit REQUIRED)
  add_executable(musikcore_macos_media_keys_test ../integration/plugins/macos-media-keys.mm)
  target_include_directories(musikcore_macos_media_keys_test PRIVATE "${PROJECT_SOURCE_DIR}/include")
  target_compile_features(musikcore_macos_media_keys_test PRIVATE cxx_std_20)
  target_link_libraries(musikcore_macos_media_keys_test PRIVATE ${MACOS_MEDIA_KEYS_APPKIT} ${CMAKE_DL_LIBS})
  add_dependencies(musikcore_macos_media_keys_test macosmediakeys)
  add_dependencies(musikcube_tests musikcore_macos_media_keys_test)
  add_dependencies(musikcube_integration_tests musikcore_macos_media_keys_test)
  add_test(NAME musikcore.integration.plugins.macos_media_keys
    COMMAND "$<TARGET_FILE:musikcore_macos_media_keys_test>" "$<TARGET_FILE:macosmediakeys>")
  set_tests_properties(musikcore.integration.plugins.macos_media_keys PROPERTIES
    LABELS "integration;musikcore;plugins;contract" SKIP_RETURN_CODE 77 TIMEOUT 30)
endif()

if(TARGET supereqdsp)
  add_library(musikcore_test_failing_dsp MODULE plugins/failing-dsp.c)
  target_include_directories(musikcore_test_failing_dsp PRIVATE "${PROJECT_SOURCE_DIR}/include")
  target_compile_definitions(musikcore_test_failing_dsp PRIVATE MC_BUILDING_PLUGIN)
  set_target_properties(musikcore_test_failing_dsp PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin/fixtures/dsp"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin/fixtures/dsp"
    SUFFIX "${CMAKE_SHARED_LIBRARY_SUFFIX}")
  musikcube_restrict_exports(musikcore_test_failing_dsp mc_plugin_get_api)
  add_dependencies(musikcore_runtime_hardening_test musikcore_test_failing_dsp musikcore_byte_decoder)
  add_test(NAME musikcore.integration.plugins.dsp_failure COMMAND "${MUSIKCORE_TEST_NODE}"
    "${PROJECT_SOURCE_DIR}/tests/scripts/run-isolated-test.js"
    "${CMAKE_CURRENT_BINARY_DIR}/bin/isolated" "$<TARGET_FILE:musikcore_runtime_hardening_test>"
    --stage --plugin "$<TARGET_FILE:musikcore_test_failing_dsp>" --plugin "$<TARGET_FILE:musikcore_byte_decoder>"
    dsp_failure)
  set_tests_properties(musikcore.integration.plugins.dsp_failure PROPERTIES
    LABELS "integration;musikcore;plugins;audio;contract" TIMEOUT 20)
  target_sources(musikcore_runtime_hardening_test PRIVATE ../integration/plugins/dsp.cpp)
  target_compile_definitions(musikcore_runtime_hardening_test PRIVATE TEST_DSP_PROCESSING)
  add_dependencies(musikcore_runtime_hardening_test supereqdsp)
  add_test(NAME musikcore.integration.plugins.dsp_processing
    COMMAND "${MUSIKCORE_TEST_NODE}" "${PROJECT_SOURCE_DIR}/tests/scripts/run-isolated-test.js"
      "${CMAKE_CURRENT_BINARY_DIR}/bin/isolated" "$<TARGET_FILE:musikcore_runtime_hardening_test>"
      --stage --plugin "$<TARGET_FILE:supereqdsp>" dsp_processing)
  set_tests_properties(musikcore.integration.plugins.dsp_processing PROPERTIES
    LABELS "integration;musikcore;plugins;audio;contract" TIMEOUT 20)
endif()

if(BUILD_TESTING AND TARGET ffmpegdecoder AND TARGET musikcube_seed_library)
  # exercise C analyzers through the real indexer and a deterministic WAV.
  add_library(musikcore_test_analysis_block MODULE plugins/blocking-analysis-stream.cpp)
  target_include_directories(musikcore_test_analysis_block PRIVATE "${PROJECT_SOURCE_DIR}/include")
  target_compile_definitions(musikcore_test_analysis_block PRIVATE MC_BUILDING_PLUGIN)
  target_compile_features(musikcore_test_analysis_block PRIVATE cxx_std_20)
  target_link_libraries(musikcore_test_analysis_block PRIVATE Threads::Threads)
  set_target_properties(musikcore_test_analysis_block PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin/fixtures/analyzer"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin/fixtures/analyzer"
    SUFFIX "${CMAKE_SHARED_LIBRARY_SUFFIX}")
  musikcube_restrict_exports(musikcore_test_analysis_block mc_plugin_get_api)
  add_dependencies(musikcube_tests musikcore_test_analysis_block)
  add_dependencies(musikcube_integration_tests musikcore_test_analysis_block)
  add_test(NAME musikcore.integration.plugins.analyzer_cancel
    COMMAND "${MUSIKCORE_TEST_NODE}" "${PROJECT_SOURCE_DIR}/tests/scripts/check-analyzer-cancel.js"
      "$<TARGET_FILE:musikcube_seed_library>" "$<TARGET_FILE:musikcore_test_analyzer>"
      "$<TARGET_FILE:musikcore_test_analysis_block>" "$<TARGET_FILE:musikcore_analyzer_metadata_test>")
  set_tests_properties(musikcore.integration.plugins.analyzer_cancel PROPERTIES
    LABELS "integration;musikcore;plugins;indexer;audio;contract" TIMEOUT 40)
  add_library(musikcore_test_analyzer MODULE plugins/analyzer.c)
  target_include_directories(musikcore_test_analyzer PRIVATE "${PROJECT_SOURCE_DIR}/include")
  target_compile_definitions(musikcore_test_analyzer PRIVATE MC_BUILDING_PLUGIN)
  set_target_properties(musikcore_test_analyzer PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin/fixtures/analyzer"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin/fixtures/analyzer"
    SUFFIX "${CMAKE_SHARED_LIBRARY_SUFFIX}")
  musikcube_restrict_exports(musikcore_test_analyzer mc_plugin_get_api)
  add_library(musikcore_test_failing_analyzer MODULE plugins/analyzer.c)
  target_include_directories(musikcore_test_failing_analyzer PRIVATE "${PROJECT_SOURCE_DIR}/include")
  target_compile_definitions(musikcore_test_failing_analyzer PRIVATE MC_BUILDING_PLUGIN ANALYZER_FAILING)
  set_target_properties(musikcore_test_failing_analyzer PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin/fixtures/analyzer"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin/fixtures/analyzer"
    SUFFIX "${CMAKE_SHARED_LIBRARY_SUFFIX}")
  musikcube_restrict_exports(musikcore_test_failing_analyzer mc_plugin_get_api)
  add_executable(musikcore_analyzer_metadata_test plugins/analyzer-metadata.c)
  target_link_libraries(musikcore_analyzer_metadata_test PRIVATE musikcore_abi0_sqlite)
  add_dependencies(musikcube_tests musikcore_test_analyzer musikcore_test_failing_analyzer musikcore_analyzer_metadata_test)
  add_dependencies(musikcube_integration_tests musikcore_test_analyzer musikcore_test_failing_analyzer musikcore_analyzer_metadata_test)
  add_test(NAME musikcore.integration.plugins.analyzer
    COMMAND "${MUSIKCORE_TEST_NODE}" "${PROJECT_SOURCE_DIR}/tests/scripts/check-analyzer.js"
      "$<TARGET_FILE:musikcube_seed_library>" "$<TARGET_FILE:musikcore_test_analyzer>"
      "$<TARGET_FILE:musikcore_test_failing_analyzer>" "$<TARGET_FILE:musikcore_analyzer_metadata_test>")
  set_tests_properties(musikcore.integration.plugins.analyzer PROPERTIES
    LABELS "integration;musikcore;plugins;indexer;audio;contract" TIMEOUT 120)
endif()

# a real C plugin verifies playback remote activation and shutdown order.
add_library(musikcore_playback_remote_fixture MODULE ../integration/plugins/playback-remote-plugin.cpp)
add_dependencies(musikcore_remote_query_transport_test musikcore_playback_remote_fixture)
add_library(musikcore_playback_remote_peer MODULE ../integration/plugins/playback-remote-plugin.cpp)
target_include_directories(musikcore_playback_remote_peer PRIVATE "${PROJECT_SOURCE_DIR}/include")
target_compile_features(musikcore_playback_remote_peer PRIVATE cxx_std_17)
target_compile_definitions(musikcore_playback_remote_peer PRIVATE MUSIKCORE_TEST_REMOTE_PEER)
target_link_libraries(musikcore_playback_remote_peer PRIVATE musikcore Threads::Threads)
musikcube_configure_c_plugin(musikcore_playback_remote_peer)
set_target_properties(musikcore_playback_remote_peer PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin/fixtures/playback-remote-peer"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin/fixtures/playback-remote-peer")
target_include_directories(musikcore_playback_remote_fixture PRIVATE "${PROJECT_SOURCE_DIR}/include")
target_compile_features(musikcore_playback_remote_fixture PRIVATE cxx_std_17)
target_link_libraries(musikcore_playback_remote_fixture PRIVATE musikcore Threads::Threads)
musikcube_configure_c_plugin(musikcore_playback_remote_fixture)
set_target_properties(musikcore_playback_remote_fixture PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin/fixtures/playback-remote"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin/fixtures/playback-remote")
# fixtures load both from the build tree and from staged runtime/plugins.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set_target_properties(musikcore_playback_remote_fixture musikcore_playback_remote_peer PROPERTIES
    INSTALL_RPATH "$ORIGIN:$ORIGIN/..:$ORIGIN/../lib:$ORIGIN/../../../../../bin")
endif()
if(APPLE)
  set_target_properties(musikcore_playback_remote_fixture PROPERTIES SUFFIX ".dylib")
  set_target_properties(musikcore_playback_remote_peer PROPERTIES SUFFIX ".dylib")
endif()
add_executable(musikcore_playback_remote_lifecycle_test ../integration/plugins/playback-remote.cpp host-wakeup.cpp)
target_include_directories(musikcore_playback_remote_lifecycle_test PRIVATE "${PROJECT_SOURCE_DIR}/include")
target_compile_features(musikcore_playback_remote_lifecycle_test PRIVATE cxx_std_17)
target_link_libraries(musikcore_playback_remote_lifecycle_test PRIVATE ${core_fixture_library} Threads::Threads)
if(WIN32)
  target_link_libraries(musikcore_playback_remote_lifecycle_test PRIVATE
    musikcube_windows vendor::ssl pdh psapi ws2_32 wldap32 comctl32)
endif()
add_dependencies(musikcore_playback_remote_lifecycle_test musikcore_playback_remote_fixture musikcore_playback_remote_peer
  musikcore_legacy_entry_fixture nullout)
add_dependencies(musikcube_tests musikcore_playback_remote_lifecycle_test)
add_dependencies(musikcube_integration_tests musikcore_playback_remote_lifecycle_test)
add_test(NAME musikcore.integration.plugins.playback_remote_lifecycle
  COMMAND "${MUSIKCORE_TEST_NODE}" "${PROJECT_SOURCE_DIR}/tests/scripts/run-isolated-test.js"
    "${CMAKE_CURRENT_BINARY_DIR}/bin/isolated" "$<TARGET_FILE:musikcore_playback_remote_lifecycle_test>"
    --stage-minimal --plugin "$<TARGET_FILE:musikcore_playback_remote_fixture>" --plugin "$<TARGET_FILE:musikcore_legacy_entry_fixture>"
    --plugin "$<TARGET_FILE:nullout>")
set_tests_properties(musikcore.integration.plugins.playback_remote_lifecycle PROPERTIES
  LABELS "integration;musikcore;plugins;contract" TIMEOUT 30)
add_test(NAME musikcore.integration.plugins.playback_remote_stop_order
  COMMAND "${MUSIKCORE_TEST_NODE}" "${PROJECT_SOURCE_DIR}/tests/scripts/run-isolated-test.js"
    "${CMAKE_CURRENT_BINARY_DIR}/bin/isolated" "$<TARGET_FILE:musikcore_playback_remote_lifecycle_test>"
    --stage-minimal --plugin "$<TARGET_FILE:musikcore_playback_remote_fixture>"
    --plugin "$<TARGET_FILE:musikcore_playback_remote_peer>" --plugin "$<TARGET_FILE:nullout>" two)
set_tests_properties(musikcore.integration.plugins.playback_remote_stop_order PROPERTIES
  LABELS "integration;musikcore;plugins;contract" TIMEOUT 30)
if(UNIX)
  add_test(NAME musikcore.integration.plugins.playback_remote_startup_rollback
    COMMAND "${MUSIKCORE_TEST_NODE}" "${PROJECT_SOURCE_DIR}/tests/scripts/run-isolated-test.js"
      "${CMAKE_CURRENT_BINARY_DIR}/bin/isolated" "$<TARGET_FILE:musikcore_playback_remote_lifecycle_test>"
      --stage-minimal --plugin "$<TARGET_FILE:musikcore_playback_remote_fixture>"
      --plugin "$<TARGET_FILE:musikcore_playback_remote_peer>" --plugin "$<TARGET_FILE:nullout>" startup-failure)
  set_tests_properties(musikcore.integration.plugins.playback_remote_startup_rollback PROPERTIES
    LABELS "integration;musikcore;plugins;contract" TIMEOUT 30)
endif()
add_test(NAME musikcore.integration.plugins.playback_remote_factory_throws
  COMMAND "${CMAKE_COMMAND}"
    "-DNODE=${MUSIKCORE_TEST_NODE}"
    "-DRUNNER=${PROJECT_SOURCE_DIR}/tests/scripts/run-isolated-test.js"
    "-DROOT=${CMAKE_CURRENT_BINARY_DIR}/bin/isolated"
    "-DTEST_BINARY=$<TARGET_FILE:musikcore_playback_remote_lifecycle_test>"
    "-DPLUGIN=$<TARGET_FILE:musikcore_playback_remote_fixture>"
    "-DOUTPUT=$<TARGET_FILE:nullout>"
    -DMODE=throw
    -P "${PROJECT_SOURCE_DIR}/tests/scripts/check-playback-remote-fatal.cmake")
set_tests_properties(musikcore.integration.plugins.playback_remote_factory_throws PROPERTIES
  LABELS "integration;musikcore;plugins;contract" TIMEOUT 30)
add_test(NAME musikcore.integration.plugins.playback_remote_start_throws
  COMMAND "${CMAKE_COMMAND}"
    "-DNODE=${MUSIKCORE_TEST_NODE}"
    "-DRUNNER=${PROJECT_SOURCE_DIR}/tests/scripts/run-isolated-test.js"
    "-DROOT=${CMAKE_CURRENT_BINARY_DIR}/bin/isolated"
    "-DTEST_BINARY=$<TARGET_FILE:musikcore_playback_remote_lifecycle_test>"
    "-DPLUGIN=$<TARGET_FILE:musikcore_playback_remote_fixture>"
    "-DOUTPUT=$<TARGET_FILE:nullout>"
    -DMODE=throw-start
    -P "${PROJECT_SOURCE_DIR}/tests/scripts/check-playback-remote-fatal.cmake")
set_tests_properties(musikcore.integration.plugins.playback_remote_start_throws PROPERTIES
  LABELS "integration;musikcore;plugins;contract" TIMEOUT 30)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND TARGET ffmpegdecoder AND TARGET taglibreader)
  add_executable(musikcore_linux_output_runtime_test ../integration/plugins/linux-output-runtime.cpp host-wakeup.cpp)
  target_include_directories(musikcore_linux_output_runtime_test PRIVATE "${PROJECT_SOURCE_DIR}/include")
  target_compile_features(musikcore_linux_output_runtime_test PRIVATE cxx_std_20)
  target_link_libraries(musikcore_linux_output_runtime_test PRIVATE musikcore Threads::Threads)
  add_dependencies(musikcore_linux_output_runtime_test ffmpegdecoder taglibreader)
  foreach(output IN ITEMS alsaout pulseout pipewireout)
    if(TARGET ${output})
      add_dependencies(musikcore_linux_output_runtime_test ${output})
    endif()
  endforeach()
  add_dependencies(musikcube_tests musikcore_linux_output_runtime_test)
  add_dependencies(musikcube_integration_tests musikcore_linux_output_runtime_test)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND TARGET alsaout)
  add_executable(musikcore_alsa_output_test ../integration/plugins/alsa-output.cpp)
  target_include_directories(musikcore_alsa_output_test PRIVATE "${PROJECT_SOURCE_DIR}/include")
  target_compile_features(musikcore_alsa_output_test PRIVATE cxx_std_20)
  target_link_libraries(musikcore_alsa_output_test PRIVATE ${CMAKE_DL_LIBS})
  add_library(musikcore_alsa_stall SHARED plugins/alsa-stall.cpp)
  target_include_directories(musikcore_alsa_stall PRIVATE ${VENDOR_INCLUDE_DIRECTORIES})
  target_compile_features(musikcore_alsa_stall PRIVATE cxx_std_20)
  target_link_libraries(musikcore_alsa_stall PRIVATE ${CMAKE_DL_LIBS})
  add_dependencies(musikcore_alsa_output_test musikcore_alsa_stall)
  add_dependencies(musikcore_alsa_output_test alsaout)
  add_dependencies(musikcube_tests musikcore_alsa_output_test)
  add_dependencies(musikcube_integration_tests musikcore_alsa_output_test)
  add_test(NAME musikcore.integration.plugins.alsa_output
    COMMAND musikcore_alsa_output_test "$<TARGET_FILE:alsaout>")
  set_tests_properties(musikcore.integration.plugins.alsa_output PROPERTIES
    LABELS "integration;musikcore;plugins;audio;contract" TIMEOUT 20)
  add_test(NAME musikcore.integration.plugins.alsa_output_cancel
    COMMAND "${CMAKE_COMMAND}" -E env "LD_PRELOAD=$<TARGET_FILE:musikcore_alsa_stall>"
      "$<TARGET_FILE:musikcore_alsa_output_test>" "$<TARGET_FILE:alsaout>" stalled)
  set_tests_properties(musikcore.integration.plugins.alsa_output_cancel PROPERTIES
    LABELS "integration;musikcore;plugins;audio;contract" TIMEOUT 20)
  if(TARGET musikcore_linux_output_runtime_test)
    add_test(NAME musikcore.integration.plugins.alsa_output_runtime
      COMMAND "${MUSIKCORE_TEST_NODE}" "${PROJECT_SOURCE_DIR}/tests/scripts/run-isolated-test.js"
        "${CMAKE_CURRENT_BINARY_DIR}/bin/fixtures" "$<TARGET_FILE:musikcore_linux_output_runtime_test>"
        --stage --plugin "$<TARGET_FILE:alsaout>" AlsaOut null)
    set_tests_properties(musikcore.integration.plugins.alsa_output_runtime PROPERTIES
      LABELS "integration;musikcore;plugins;audio;contract" TIMEOUT 30)
  endif()
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND TARGET pulseout)
  add_library(musikcore_pulse_output_fixture SHARED
    "${PROJECT_SOURCE_DIR}/src/plugins/pulseout/pulseout_plugin.cpp"
    "${PROJECT_SOURCE_DIR}/src/plugins/pulseout/PulseOut.cpp"
    plugins/pulse-stall.cpp)
  target_include_directories(musikcore_pulse_output_fixture PRIVATE "${PROJECT_SOURCE_DIR}/src/plugins")
  target_compile_features(musikcore_pulse_output_fixture PRIVATE cxx_std_20)
  target_link_libraries(musikcore_pulse_output_fixture PRIVATE ${LIBPULSE} Threads::Threads)
  musikcube_configure_c_plugin(musikcore_pulse_output_fixture)
  set_target_properties(musikcore_pulse_output_fixture PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin/fixtures/pulse-output")
  add_executable(musikcore_pulse_output_test ../integration/plugins/pulse-output.cpp)
  target_include_directories(musikcore_pulse_output_test PRIVATE "${PROJECT_SOURCE_DIR}/include")
  target_compile_features(musikcore_pulse_output_test PRIVATE cxx_std_20)
  target_link_libraries(musikcore_pulse_output_test PRIVATE ${CMAKE_DL_LIBS} Threads::Threads)
  add_dependencies(musikcore_pulse_output_test musikcore_pulse_output_fixture)
  add_dependencies(musikcube_tests musikcore_pulse_output_test)
  add_dependencies(musikcube_integration_tests musikcore_pulse_output_test)
  add_test(NAME musikcore.integration.plugins.pulse_output
    COMMAND musikcore_pulse_output_test "$<TARGET_FILE:musikcore_pulse_output_fixture>" fixture)
  set_tests_properties(musikcore.integration.plugins.pulse_output PROPERTIES
    LABELS "integration;musikcore;plugins;audio;contract" TIMEOUT 20)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND TARGET pipewireout)
  find_program(MUSIKCORE_DBUS_RUN_SESSION dbus-run-session)
  find_program(MUSIKCORE_PIPEWIRE pipewire)
  find_program(MUSIKCORE_PIPEWIRE_PULSE pipewire-pulse)
  find_program(MUSIKCORE_WIREPLUMBER wireplumber)
  find_program(MUSIKCORE_PACTL pactl)
  find_program(MUSIKCORE_PW_DUMP pw-dump)
  find_program(MUSIKCORE_PW_CLI pw-cli)
  if(MUSIKCORE_DBUS_RUN_SESSION AND MUSIKCORE_PIPEWIRE AND MUSIKCORE_PIPEWIRE_PULSE AND
      MUSIKCORE_WIREPLUMBER AND MUSIKCORE_PACTL AND MUSIKCORE_PW_DUMP AND MUSIKCORE_PW_CLI)
    if(TARGET pulseout)
      add_library(musikcore_pulse_nowait SHARED plugins/pulse-nowait.cpp)
      target_include_directories(musikcore_pulse_nowait PRIVATE ${VENDOR_INCLUDE_DIRECTORIES})
      target_link_libraries(musikcore_pulse_nowait PRIVATE ${CMAKE_DL_LIBS})
      add_dependencies(musikcore_pulse_output_test musikcore_pulse_nowait pulseout)
      add_test(NAME musikcore.integration.plugins.pulse_control_nowait
        COMMAND "${MUSIKCORE_DBUS_RUN_SESSION}" -- "${MUSIKCORE_TEST_NODE}"
          "${PROJECT_SOURCE_DIR}/tests/scripts/run-pipewire-output.js" "${PROJECT_BINARY_DIR}"
          "$<TARGET_FILE:musikcore_pulse_output_test>" "$<TARGET_FILE:pulseout>" ""
          "$<TARGET_FILE:musikcore_pulse_nowait>")
      set_tests_properties(musikcore.integration.plugins.pulse_control_nowait PROPERTIES
        LABELS "integration;musikcore;plugins;audio;contract" TIMEOUT 45 RESOURCE_LOCK isolated_pipewire)
    endif()

    add_executable(musikcore_pipewire_output_test ../integration/plugins/pipewire-output.cpp)
    target_include_directories(musikcore_pipewire_output_test PRIVATE "${PROJECT_SOURCE_DIR}/include")
    target_compile_features(musikcore_pipewire_output_test PRIVATE cxx_std_20)
    target_link_libraries(musikcore_pipewire_output_test PRIVATE ${CMAKE_DL_LIBS} Threads::Threads)
    add_dependencies(musikcore_pipewire_output_test pipewireout)
    add_dependencies(musikcube_tests musikcore_pipewire_output_test)
    add_dependencies(musikcube_integration_tests musikcore_pipewire_output_test)
    add_test(NAME musikcore.integration.plugins.pipewire_output
      COMMAND "${MUSIKCORE_DBUS_RUN_SESSION}" -- "${MUSIKCORE_TEST_NODE}"
        "${PROJECT_SOURCE_DIR}/tests/scripts/run-pipewire-output.js" "${PROJECT_BINARY_DIR}"
        "$<TARGET_FILE:musikcore_pipewire_output_test>" "$<TARGET_FILE:pipewireout>")
    set_tests_properties(musikcore.integration.plugins.pipewire_output PROPERTIES
      LABELS "integration;musikcore;plugins;audio;contract" TIMEOUT 45 RESOURCE_LOCK isolated_pipewire)
    if(TARGET musikcore_linux_output_runtime_test)
      foreach(driver IN ITEMS pipewire pulse)
        if(TARGET ${driver}out)
          if(driver STREQUAL "pipewire")
            set(driver_name "PipeWire")
          else()
            set(driver_name "PulseAudio")
          endif()
          add_test(NAME musikcore.integration.plugins.${driver}_output_runtime
            COMMAND "${MUSIKCORE_DBUS_RUN_SESSION}" -- "${MUSIKCORE_TEST_NODE}"
              "${PROJECT_SOURCE_DIR}/tests/scripts/run-pipewire-output.js" "${PROJECT_BINARY_DIR}"
              "$<TARGET_FILE:musikcore_linux_output_runtime_test>" "$<TARGET_FILE:${driver}out>" "${driver_name}")
          set_tests_properties(musikcore.integration.plugins.${driver}_output_runtime PROPERTIES
            LABELS "integration;musikcore;plugins;audio;contract" TIMEOUT 45 RESOURCE_LOCK isolated_pipewire)
        endif()
      endforeach()
    endif()
  endif()
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND TARGET mpris)
  find_program(MUSIKCORE_DBUS_RUN_SESSION dbus-run-session)
  if(MUSIKCORE_DBUS_RUN_SESSION)
    if(USE_ELOGIND)
      set(mpris_test_library "libelogind >= 239.3")
    elseif(USE_BASU)
      set(mpris_test_library basu)
    else()
      set(mpris_test_library libsystemd)
    endif()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(MUSIKCORE_TEST_SDBUS REQUIRED IMPORTED_TARGET ${mpris_test_library})
    add_library(musikcore_mpris_stall SHARED
      "${PROJECT_SOURCE_DIR}/src/plugins/mpris/mpris.cpp"
      "${PROJECT_SOURCE_DIR}/src/plugins/mpris/dbus.cpp"
      "${PROJECT_SOURCE_DIR}/src/plugins/mpris/playback.cpp"
      "${PROJECT_SOURCE_DIR}/src/plugins/mpris/plugin.cpp"
      plugins/mpris-stall.cpp)
    target_include_directories(musikcore_mpris_stall PRIVATE "${PROJECT_SOURCE_DIR}/src/plugins/mpris")
    target_compile_definitions(musikcore_mpris_stall PRIVATE "$<TARGET_PROPERTY:mpris,INTERFACE_COMPILE_DEFINITIONS>")
    target_compile_features(musikcore_mpris_stall PRIVATE cxx_std_20)
    target_link_libraries(musikcore_mpris_stall PRIVATE musikcore PkgConfig::MUSIKCORE_TEST_SDBUS Threads::Threads)
    musikcube_configure_c_plugin(musikcore_mpris_stall)
    set_target_properties(musikcore_mpris_stall PROPERTIES
      LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin/fixtures/mpris-stall"
      BUILD_RPATH "$<TARGET_FILE_DIR:musikcore>")
    add_executable(musikcore_mpris_test ../integration/plugins/mpris.cpp)
    target_include_directories(musikcore_mpris_test PRIVATE "${PROJECT_SOURCE_DIR}/include")
    target_compile_definitions(musikcore_mpris_test PRIVATE "$<TARGET_PROPERTY:mpris,INTERFACE_COMPILE_DEFINITIONS>")
    target_compile_features(musikcore_mpris_test PRIVATE cxx_std_20)
    target_link_libraries(musikcore_mpris_test PRIVATE PkgConfig::MUSIKCORE_TEST_SDBUS ${CMAKE_DL_LIBS} Threads::Threads)
    add_dependencies(musikcore_mpris_test mpris musikcore_mpris_stall)
    add_dependencies(musikcube_tests musikcore_mpris_test)
    add_dependencies(musikcube_integration_tests musikcore_mpris_test)
    add_test(NAME musikcore.integration.plugins.mpris
      COMMAND "${MUSIKCORE_DBUS_RUN_SESSION}" -- "$<TARGET_FILE:musikcore_mpris_test>" "$<TARGET_FILE:mpris>")
    set_tests_properties(musikcore.integration.plugins.mpris PROPERTIES
      LABELS "integration;musikcore;plugins;contract" TIMEOUT 20)
    add_test(NAME musikcore.integration.plugins.mpris_artwork_stall
      COMMAND "${CMAKE_COMMAND}" -E env "LD_LIBRARY_PATH=$<TARGET_FILE_DIR:musikcore>"
        "${MUSIKCORE_DBUS_RUN_SESSION}" -- "$<TARGET_FILE:musikcore_mpris_test>"
        "$<TARGET_FILE:musikcore_mpris_stall>" stall)
    set_tests_properties(musikcore.integration.plugins.mpris_artwork_stall PROPERTIES
      LABELS "integration;musikcore;plugins;contract" TIMEOUT 20)
    add_executable(musikcore_mpris_runtime_test ../integration/plugins/mpris-runtime.cpp host-wakeup.cpp)
    target_include_directories(musikcore_mpris_runtime_test PRIVATE "${PROJECT_SOURCE_DIR}/include")
    target_compile_definitions(musikcore_mpris_runtime_test PRIVATE "$<TARGET_PROPERTY:mpris,INTERFACE_COMPILE_DEFINITIONS>")
    target_compile_features(musikcore_mpris_runtime_test PRIVATE cxx_std_20)
    target_link_libraries(musikcore_mpris_runtime_test PRIVATE musikcore PkgConfig::MUSIKCORE_TEST_SDBUS Threads::Threads)
    add_dependencies(musikcore_mpris_runtime_test mpris nullout ffmpegdecoder taglibreader musikcube_seed_library)
    add_dependencies(musikcube_tests musikcore_mpris_runtime_test)
    add_dependencies(musikcube_integration_tests musikcore_mpris_runtime_test)
    add_test(NAME musikcore.integration.plugins.mpris_runtime
      COMMAND "${MUSIKCORE_DBUS_RUN_SESSION}" -- "${CMAKE_COMMAND}"
        "-DNODE=${MUSIKCORE_TEST_NODE}" "-DSEED_SCRIPT=${PROJECT_SOURCE_DIR}/tests/scripts/seed-test-library.js"
        "-DISOLATED_RUNNER=${PROJECT_SOURCE_DIR}/tests/scripts/run-isolated-test.js"
        "-DBUILD_DIR=${PROJECT_BINARY_DIR}" "-DINDEXER=$<TARGET_FILE:musikcube_seed_library>"
        "-DNULL_OUTPUT=$<TARGET_FILE:nullout>" "-DMPRIS_PLUGIN=$<TARGET_FILE:mpris>"
        "-DMPRIS_RUNTIME_TEST=$<TARGET_FILE:musikcore_mpris_runtime_test>"
        -P "${PROJECT_SOURCE_DIR}/tests/scripts/check-indexed-library.cmake")
    set_tests_properties(musikcore.integration.plugins.mpris_runtime PROPERTIES
      LABELS "integration;musikcore;plugins;contract" TIMEOUT 90)
  endif()
endif()

if(UNIX AND TARGET sndioout)
  add_executable(musikcore_sndio_no_flush_test ../unit/musikcore/sndio-no-flush.cpp
    "${PROJECT_SOURCE_DIR}/src/plugins/sndioout/SndioOut.cpp")
  target_include_directories(musikcore_sndio_no_flush_test PRIVATE "${PROJECT_SOURCE_DIR}/src/plugins")
  target_compile_features(musikcore_sndio_no_flush_test PRIVATE cxx_std_20)
  target_compile_definitions(musikcore_sndio_no_flush_test PRIVATE MUSIKCUBE_SNDIO_HAS_FLUSH=0)
  target_link_libraries(musikcore_sndio_no_flush_test PRIVATE "${LIBSNDIO}" Threads::Threads)
  set_target_properties(musikcore_sndio_no_flush_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin")
  add_dependencies(musikcube_unit_tests musikcore_sndio_no_flush_test)
  add_dependencies(musikcube_tests musikcore_sndio_no_flush_test)
  add_test(NAME musikcore.unit.plugins.sndio_no_flush COMMAND musikcore_sndio_no_flush_test)
  set_tests_properties(musikcore.unit.plugins.sndio_no_flush PROPERTIES
    LABELS "unit;musikcore;plugins;audio;contract" TIMEOUT 20)
  add_executable(musikcore_sndio_devices_test ../integration/plugins/sndio-devices.cpp)
  target_include_directories(musikcore_sndio_devices_test PRIVATE "${PROJECT_SOURCE_DIR}/include")
  target_link_libraries(musikcore_sndio_devices_test PRIVATE ${CMAKE_DL_LIBS})
  set_target_properties(musikcore_sndio_devices_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin")
  add_dependencies(musikcore_sndio_devices_test sndioout)
  add_dependencies(musikcube_integration_tests musikcore_sndio_devices_test)
  add_dependencies(musikcube_tests musikcore_sndio_devices_test)
  add_test(NAME musikcore.integration.plugins.sndio_devices
    COMMAND musikcore_sndio_devices_test "$<TARGET_FILE:sndioout>")
  set_tests_properties(musikcore.integration.plugins.sndio_devices PROPERTIES
    LABELS "integration;musikcore;plugins;audio;contract" TIMEOUT 20)
endif()

if((CMAKE_SYSTEM_NAME STREQUAL "Linux" OR APPLE) AND TARGET projectm)
  add_executable(musikcore_projectm_test ../integration/plugins/projectm.cpp)
  target_include_directories(musikcore_projectm_test PRIVATE "${PROJECT_SOURCE_DIR}/include")
  target_link_libraries(musikcore_projectm_test PRIVATE ${CMAKE_DL_LIBS})
  target_compile_features(musikcore_projectm_test PRIVATE cxx_std_17)
  set_target_properties(musikcore_projectm_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin")
  add_dependencies(musikcore_projectm_test projectm)
  add_dependencies(musikcube_tests musikcore_projectm_test)
  add_dependencies(musikcube_integration_tests musikcore_projectm_test)
  add_test(NAME musikcore.integration.plugins.projectm
    COMMAND musikcore_projectm_test "$<TARGET_FILE:projectm>")
  set_tests_properties(musikcore.integration.plugins.projectm PROPERTIES
    LABELS "integration;musikcore;plugins;visualizer" TIMEOUT 30)
  if(NOT APPLE)
    set_tests_properties(musikcore.integration.plugins.projectm PROPERTIES ENVIRONMENT "SDL_VIDEODRIVER=dummy")
  endif()
endif()
