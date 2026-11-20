add_executable(musikcube_seed_library seed-library.cpp)
target_include_directories(musikcube_seed_library PRIVATE "${PROJECT_SOURCE_DIR}/src" ${VENDOR_INCLUDE_DIRECTORIES})
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
  add_executable(musikcore_remote_query_transport_test remote-query-transport.cpp)
  target_include_directories(musikcore_remote_query_transport_test PRIVATE
    "${PROJECT_SOURCE_DIR}/src" ${VENDOR_INCLUDE_DIRECTORIES})
  target_link_libraries(musikcore_remote_query_transport_test PRIVATE musikcore Threads::Threads)
  set_target_properties(musikcore_remote_query_transport_test PROPERTIES
    DISABLE_PRECOMPILE_HEADERS ON)
  if(WIN32)
    target_link_libraries(musikcore_remote_query_transport_test PRIVATE
      musikcube_windows vendor::ssl pdh psapi ws2_32 wldap32 comctl32)
  endif()
  add_dependencies(musikcube_tests musikcore_remote_query_transport_test)
  add_test(NAME musikcore.abi.remote_query_transport COMMAND musikcore_remote_query_transport_test)
  set_tests_properties(musikcore.abi.remote_query_transport PROPERTIES
    LABELS "integration;abi"
    TIMEOUT 20
    ENVIRONMENT "XDG_CONFIG_HOME=${CMAKE_CURRENT_BINARY_DIR}/bin/remote-fixture;APPDATA=${CMAKE_CURRENT_BINARY_DIR}/bin/remote-fixture")

  find_program(MUSIKCUBE_TEST_NODE NAMES node)
  if(MUSIKCUBE_TEST_NODE)
    add_executable(musikcore_production_api_test
      ../abi/cpp-wrapper.cpp ../abi/query-support.cpp)
    target_compile_definitions(musikcore_production_api_test PRIVATE MUSIKCORE_TEST_APP_BINDING)
    target_include_directories(musikcore_production_api_test PRIVATE
      "${PROJECT_SOURCE_DIR}/src" ${VENDOR_INCLUDE_DIRECTORIES})
    set_target_properties(musikcore_production_api_test PROPERTIES DISABLE_PRECOMPILE_HEADERS ON)
    target_compile_features(musikcore_production_api_test PRIVATE cxx_std_17)
    target_link_libraries(musikcore_production_api_test PRIVATE musikcore Threads::Threads)
    if(WIN32)
      target_link_libraries(musikcore_production_api_test PRIVATE musikcube_windows vendor::ssl pdh psapi ws2_32 wldap32 comctl32)
    endif()
    add_dependencies(musikcube_tests musikcube_seed_library musikcore_production_api_test)
    add_test(NAME musikcore.abi.indexed_query COMMAND "${CMAKE_COMMAND}"
      "-DNODE=${MUSIKCUBE_TEST_NODE}"
      "-DSEED_SCRIPT=${PROJECT_SOURCE_DIR}/script/seed-test-library.js"
      "-DBUILD_DIR=${PROJECT_BINARY_DIR}"
      "-DINDEXER=$<TARGET_FILE:musikcube_seed_library>"
      "-DQUERY_TEST=$<TARGET_FILE:musikcore_abi_query_test>"
      "-DCPP_TEST=$<TARGET_FILE:musikcore_abi_cpp_test>"
      "-DPRODUCTION_TEST=$<TARGET_FILE:musikcore_production_api_test>"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/check-indexed-library.cmake")
    set_tests_properties(musikcore.abi.indexed_query PROPERTIES LABELS "integration;abi" TIMEOUT 90)
  endif()
endif()
