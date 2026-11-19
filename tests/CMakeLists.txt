add_executable(musikcore_abi_header_test
  abi/header.c
  abi/header.cpp
  abi/error.cpp
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
set_tests_properties(musikcore.abi.header PROPERTIES LABELS "contract;abi")

add_executable(musikcore_abi0_c_test abi/shared.c)
target_link_libraries(musikcore_abi0_c_test PRIVATE musikcore_abi0)
set_target_properties(musikcore_abi0_c_test PROPERTIES
  C_STANDARD 11
  C_STANDARD_REQUIRED ON
  C_EXTENSIONS OFF
  LINKER_LANGUAGE C
  RUNTIME_OUTPUT_DIRECTORY "${MUSIKCORE_ABI0_OUTPUT_DIR}"
)
add_test(NAME musikcore.abi.shared_c COMMAND musikcore_abi0_c_test)
set_tests_properties(musikcore.abi.shared_c PROPERTIES LABELS "contract;abi")

if(UNIX)
  add_test(NAME musikcore.abi.exports COMMAND "${CMAKE_COMMAND}"
    "-DNM=${CMAKE_NM}"
    "-DAPPLE=${APPLE}"
    "-DLIBRARY=$<TARGET_FILE:musikcore_abi0>"
    "-DALLOWLIST=${PROJECT_SOURCE_DIR}/src/musikcore/ffi/exports.txt"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/abi/check-exports.cmake")
  set_tests_properties(musikcore.abi.exports PROPERTIES LABELS "contract;abi")
endif()

add_custom_target(musikcube_tests DEPENDS musikcore_abi_header_test musikcore_abi0_c_test)
