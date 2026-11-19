add_executable(musikcube_seed_library seed-library.cpp)
target_include_directories(musikcube_seed_library PRIVATE "${PROJECT_SOURCE_DIR}/src")
set_target_properties(musikcube_seed_library PROPERTIES DISABLE_PRECOMPILE_HEADERS ON)
target_compile_features(musikcube_seed_library PRIVATE cxx_std_11)
target_link_libraries(musikcube_seed_library PRIVATE musikcore Threads::Threads)

if(WIN32)
  target_link_libraries(musikcube_seed_library PRIVATE musikcube_windows pdh psapi ws2_32 wldap32 comctl32)
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
  find_program(MUSIKCUBE_TEST_NODE NAMES node)
  if(MUSIKCUBE_TEST_NODE)
    add_dependencies(musikcube_tests musikcube_seed_library)
    add_test(NAME musikcore.abi.indexed_query COMMAND "${CMAKE_COMMAND}"
      "-DNODE=${MUSIKCUBE_TEST_NODE}"
      "-DSEED_SCRIPT=${PROJECT_SOURCE_DIR}/script/seed-test-library.js"
      "-DBUILD_DIR=${PROJECT_BINARY_DIR}"
      "-DINDEXER=$<TARGET_FILE:musikcube_seed_library>"
      "-DQUERY_TEST=$<TARGET_FILE:musikcore_abi_query_test>"
      "-DCPP_TEST=$<TARGET_FILE:musikcore_abi_cpp_test>"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/check-indexed-library.cmake")
    set_tests_properties(musikcore.abi.indexed_query PROPERTIES LABELS "integration;abi" TIMEOUT 90)
  endif()
endif()
