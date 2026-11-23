if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_SIZEOF_VOID_P EQUAL 4)
  # The file source selects the 64-bit libc interfaces before including system headers.
  set_source_files_properties(io/LocalFileSource.cpp PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
endif()

include_directories(
  .
  ..
  ../3rdparty/include
  ../3rdparty/include/md5
  ../3rdparty/include/sqlite
)

add_library(musikcore_objects OBJECT ${musikcore_sources}
  ../3rdparty/src/sqlean/unicode/extension.c
  ../3rdparty/src/sqlite/sqlite3.c
  ../3rdparty/src/kiss_fft.c
  ../3rdparty/src/kiss_fftr.c
  ../3rdparty/src/md5.c)
set_target_properties(musikcore_objects PROPERTIES POSITION_INDEPENDENT_CODE ON)
musikcore_add_consumer_api(musikcore_objects "${CMAKE_CURRENT_SOURCE_DIR}/ffi/consumer/remote-session.cpp")
target_link_libraries(musikcore_objects PRIVATE ${musikcube_LINK_LIBS})
target_include_directories(musikcore_objects BEFORE PRIVATE ${VENDOR_INCLUDE_DIRECTORIES})
find_header(asio.hpp)

add_library(musikcore SHARED $<TARGET_OBJECTS:musikcore_objects>)
set_target_properties(musikcore PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${MUSIKCUBE_RUNTIME_OUTPUT_DIRECTORY}")
target_include_directories(musikcore PUBLIC "${PROJECT_SOURCE_DIR}/include")
target_compile_definitions(musikcore INTERFACE MC_USING_SHARED)
target_link_libraries(musikcore PRIVATE ${musikcube_LINK_LIBS} Threads::Threads)
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/ffi/exports.txt" public_exports)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/ffi/exports.txt")
musikcube_restrict_exports(musikcore ${public_exports})

if(BUILD_TESTING)
    # internal tests link these same objects without widening the installed ABI.
    add_library(musikcore_internal STATIC $<TARGET_OBJECTS:musikcore_objects>)
    target_include_directories(musikcore_internal PUBLIC "${PROJECT_SOURCE_DIR}/include")
    target_link_libraries(musikcore_internal PUBLIC ${musikcube_LINK_LIBS} Threads::Threads)
endif()

if (ENABLE_PCH MATCHES "true")
    message(STATUS "[musikcore] enabling precompiled headers")
    include(./pch.cmake)
endif()
