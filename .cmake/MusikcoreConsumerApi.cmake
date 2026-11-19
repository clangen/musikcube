function(musikcore_add_consumer_api target)
  set(ffi "${PROJECT_SOURCE_DIR}/src/musikcore/ffi")
  set(sources
    "${ffi}/common/version.cpp"
    "${ffi}/common/string.cpp"
    "${ffi}/common/error.cpp"
    "${ffi}/consumer/context.cpp"
    "${ffi}/consumer/library.cpp"
    "${PROJECT_SOURCE_DIR}/src/musikcore/library/query/TrackReadQueries.cpp"
  )
  target_sources(${target} PRIVATE ${sources})
  set_source_files_properties(${sources} PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
  target_include_directories(${target} PUBLIC "${PROJECT_SOURCE_DIR}/include")
  target_include_directories(${target} PRIVATE "${PROJECT_SOURCE_DIR}/src/3rdparty/include/sqlite")
  target_link_libraries(${target} PRIVATE Threads::Threads)

  get_target_property(kind ${target} TYPE)
  if(kind STREQUAL "SHARED_LIBRARY")
    target_compile_definitions(${target} PRIVATE MC_BUILDING_SHARED INTERFACE MC_USING_SHARED)
    # Keep façade implementation symbols private without changing legacy exports.
    if(CMAKE_CXX_COMPILER_ID MATCHES "^(GNU|Clang|AppleClang)$")
      set_property(SOURCE ${sources} APPEND PROPERTY COMPILE_OPTIONS
        -fvisibility=hidden -fvisibility-inlines-hidden)
    endif()
  endif()
endfunction()
