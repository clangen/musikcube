set(MUSIKCORE_ABI0_OUTPUT_DIR "${PROJECT_BINARY_DIR}/bin/abi0" PARENT_SCOPE)
set(output_dir "${PROJECT_BINARY_DIR}/bin/abi0")
file(MAKE_DIRECTORY "${output_dir}/plugins")

add_library(musikcore_abi0_sqlite STATIC
  "${PROJECT_SOURCE_DIR}/src/3rdparty/src/sqlite/sqlite3.c"
  "${PROJECT_SOURCE_DIR}/src/3rdparty/src/sqlean/unicode/extension.c"
)
target_include_directories(musikcore_abi0_sqlite PRIVATE "${PROJECT_SOURCE_DIR}/src/3rdparty/include")
target_include_directories(musikcore_abi0_sqlite PUBLIC "${PROJECT_SOURCE_DIR}/src/3rdparty/include/sqlite")
target_compile_definitions(musikcore_abi0_sqlite PRIVATE SQLITE_OMIT_LOAD_EXTENSION)
if(WIN32)
  set_source_files_properties("${PROJECT_SOURCE_DIR}/src/3rdparty/src/sqlean/unicode/extension.c"
    PROPERTIES COMPILE_DEFINITIONS SQLITE_CORE)
endif()
target_link_libraries(musikcore_abi0_sqlite PRIVATE Threads::Threads)
set_target_properties(musikcore_abi0_sqlite PROPERTIES
  POSITION_INDEPENDENT_CODE ON
  C_VISIBILITY_PRESET hidden
  ARCHIVE_OUTPUT_DIRECTORY "${output_dir}"
)

# the restricted ABI test runtime includes these read-only category and SQLite
# sources so explicit catalog handles can exercise the same typed queries.
add_library(musikcore_abi0 SHARED
  "${PROJECT_SOURCE_DIR}/src/musikcore/db/SqliteExtensions.cpp"
  "${PROJECT_SOURCE_DIR}/src/musikcore/db/Connection.cpp"
  "${PROJECT_SOURCE_DIR}/src/musikcore/db/Statement.cpp"
  "${PROJECT_SOURCE_DIR}/src/musikcore/library/query/AllCategoriesQuery.cpp"
  "${PROJECT_SOURCE_DIR}/src/musikcore/library/query/CategoryListQuery.cpp"
  "${PROJECT_SOURCE_DIR}/src/musikcore/library/query/util/CategoryQueryUtil.cpp"
  "${PROJECT_SOURCE_DIR}/src/musikcore/library/query/util/CategorySerialization.cpp"
)
musikcore_add_consumer_api(musikcore_abi0 "${PROJECT_SOURCE_DIR}/src/musikcore/ffi/consumer/remote-session-abi-only.cpp")
target_include_directories(musikcore_abi0 PRIVATE "${PROJECT_SOURCE_DIR}/src/musikcore" "${PROJECT_SOURCE_DIR}/src" "${PROJECT_SOURCE_DIR}/src/3rdparty/include")
target_compile_definitions(musikcore_abi0 PRIVATE ASIO_STANDALONE _WEBSOCKETPP_CPP11_TYPE_TRAITS_ _WEBSOCKETPP_CPP11_RANDOM_DEVICE_)
target_include_directories(musikcore_abi0 BEFORE PRIVATE ${VENDOR_INCLUDE_DIRECTORIES} "${PROJECT_SOURCE_DIR}/src/3rdparty/asio/asio/include")
target_link_libraries(musikcore_abi0 PRIVATE musikcore_abi0_sqlite)
if(WIN32 AND NOT DEFINED MUSIKCORE_USE_ASIO_WINTLS)
  option(MUSIKCORE_USE_ASIO_WINTLS "Use Windows TLS for the WebSocket client" ON)
endif()
if(WIN32 AND MUSIKCORE_USE_ASIO_WINTLS)
  target_compile_definitions(musikcore_abi0 PRIVATE MUSIKCORE_USE_ASIO_WINTLS UNICODE _UNICODE)
  target_include_directories(musikcore_abi0 PRIVATE "${PROJECT_SOURCE_DIR}/src/3rdparty/asio-wintls/include")
endif()
add_library(musikcore_cpp_abi0 INTERFACE)
target_link_libraries(musikcore_cpp_abi0 INTERFACE musikcore_abi0)
target_compile_features(musikcore_cpp_abi0 INTERFACE cxx_std_17)
set_target_properties(musikcore_abi0 PROPERTIES
  OUTPUT_NAME musikcore-abi0
  ARCHIVE_OUTPUT_DIRECTORY "${output_dir}"
  LIBRARY_OUTPUT_DIRECTORY "${output_dir}"
  RUNTIME_OUTPUT_DIRECTORY "${output_dir}"
  CXX_STANDARD 20
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
  CXX_VISIBILITY_PRESET hidden
  VISIBILITY_INLINES_HIDDEN ON
  WINDOWS_EXPORT_ALL_SYMBOLS OFF
)

set(allowlist "${PROJECT_SOURCE_DIR}/src/musikcore/ffi/exports.txt")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${allowlist}")
file(STRINGS "${allowlist}" exports)
musikcube_restrict_exports(musikcore_abi0 ${exports})
