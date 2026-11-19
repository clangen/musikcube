set(MUSIKCORE_ABI0_OUTPUT_DIR "${PROJECT_BINARY_DIR}/bin/abi0" PARENT_SCOPE)
set(output_dir "${PROJECT_BINARY_DIR}/bin/abi0")

add_library(musikcore_abi0_sqlite STATIC
  "${PROJECT_SOURCE_DIR}/src/3rdparty/src/sqlite/sqlite3.c"
  "${PROJECT_SOURCE_DIR}/src/3rdparty/src/sqlean/unicode/extension.c"
)
target_include_directories(musikcore_abi0_sqlite PRIVATE "${PROJECT_SOURCE_DIR}/src/3rdparty/include")
target_include_directories(musikcore_abi0_sqlite PUBLIC "${PROJECT_SOURCE_DIR}/src/3rdparty/include/sqlite")
target_compile_definitions(musikcore_abi0_sqlite PRIVATE SQLITE_OMIT_LOAD_EXTENSION)
target_link_libraries(musikcore_abi0_sqlite PRIVATE Threads::Threads)
set_target_properties(musikcore_abi0_sqlite PROPERTIES
  POSITION_INDEPENDENT_CODE ON
  C_VISIBILITY_PRESET hidden
  ARCHIVE_OUTPUT_DIRECTORY "${output_dir}"
)

add_library(musikcore_abi0 SHARED ../db/SqliteExtensions.cpp)
musikcore_add_consumer_api(musikcore_abi0)
target_include_directories(musikcore_abi0 PRIVATE "${PROJECT_SOURCE_DIR}/src" "${PROJECT_SOURCE_DIR}/src/3rdparty/include")
target_link_libraries(musikcore_abi0 PRIVATE musikcore_abi0_sqlite)
add_library(musikcore_cpp_abi0 INTERFACE)
target_link_libraries(musikcore_cpp_abi0 INTERFACE musikcore_abi0)
target_compile_features(musikcore_cpp_abi0 INTERFACE cxx_std_17)
set_target_properties(musikcore_abi0 PROPERTIES
  OUTPUT_NAME musikcore-abi0
  ARCHIVE_OUTPUT_DIRECTORY "${output_dir}"
  LIBRARY_OUTPUT_DIRECTORY "${output_dir}"
  RUNTIME_OUTPUT_DIRECTORY "${output_dir}"
  CXX_STANDARD 11
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
  CXX_VISIBILITY_PRESET hidden
  VISIBILITY_INLINES_HIDDEN ON
  WINDOWS_EXPORT_ALL_SYMBOLS OFF
)

set(allowlist "${CMAKE_CURRENT_SOURCE_DIR}/exports.txt")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${allowlist}")
file(STRINGS "${allowlist}" exports)
if(APPLE)
  string(REPLACE ";" "\n_" symbols "_${exports}")
  set(export_file "${CMAKE_CURRENT_BINARY_DIR}/exports.list")
  file(GENERATE OUTPUT "${export_file}" CONTENT "${symbols}\n")
  target_link_libraries(musikcore_abi0 PRIVATE "-Wl,-exported_symbols_list,${export_file}")
elseif(NOT WIN32)
  string(REPLACE ";" ";\n    " symbols "${exports}")
  set(export_file "${CMAKE_CURRENT_BINARY_DIR}/exports.map")
  file(GENERATE OUTPUT "${export_file}" CONTENT "{\n  global:\n    ${symbols};\n  local:\n    *;\n};\n")
  target_link_libraries(musikcore_abi0 PRIVATE "-Wl,--version-script=${export_file}")
endif()
if(export_file)
  set_property(TARGET musikcore_abi0 APPEND PROPERTY LINK_DEPENDS "${export_file}")
endif()
