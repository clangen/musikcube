function(musikcore_add_consumer_api target remote_session_source)
  set(ffi "${PROJECT_SOURCE_DIR}/src/musikcore/ffi")
  set(sources
    "${ffi}/common/version.cpp"
    "${ffi}/common/string.cpp"
    "${ffi}/common/error.cpp"
    "${ffi}/common/subscriptions.cpp"
    "${ffi}/plugin_abi/ProviderHost.cpp"
    "${ffi}/consumer/context.cpp"
    "${ffi}/consumer/library.cpp"
    "${remote_session_source}"
    "${ffi}/consumer/playback.cpp"
    "${ffi}/consumer/player.cpp"
    "${PROJECT_SOURCE_DIR}/src/musikcore/library/query/TrackReadQueries.cpp"
    "${PROJECT_SOURCE_DIR}/src/musikcore/library/query/TrackReadBackend.cpp"
  )
  if(target STREQUAL "musikcore" OR target STREQUAL "musikcore_objects")
    list(APPEND sources "${ffi}/consumer/library-query-backends.cpp")
  else()
    list(APPEND sources "${ffi}/consumer/audio-abi-only.cpp")
  endif()
  target_sources(${target} PRIVATE ${sources})
  set_source_files_properties(${sources} PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
  target_include_directories(${target} PUBLIC "${PROJECT_SOURCE_DIR}/include")
  target_include_directories(${target} PRIVATE "${PROJECT_SOURCE_DIR}/src/3rdparty/include/sqlite")
  target_link_libraries(${target} PRIVATE Threads::Threads ${CMAKE_DL_LIBS})

  get_target_property(kind ${target} TYPE)
  if(kind STREQUAL "SHARED_LIBRARY" OR target STREQUAL "musikcore_objects")
    target_compile_definitions(${target} PRIVATE MC_BUILDING_SHARED INTERFACE MC_USING_SHARED)
    # Hide C++ implementation symbols at compile time.
    if(CMAKE_CXX_COMPILER_ID MATCHES "^(GNU|Clang|AppleClang)$")
      set_property(SOURCE ${sources} APPEND PROPERTY COMPILE_OPTIONS
        -fvisibility=hidden -fvisibility-inlines-hidden)
    endif()
  endif()
endfunction()
