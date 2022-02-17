set (taglibreader_SOURCES
  taglib_plugin.cpp
  TaglibMetadataReader.cpp
)

add_library(taglibreader SHARED ${taglibreader_SOURCES})
include_directories(${Boost_INCLUDE_DIRS})

find_library(LIBZ z)

if (NOT ${ENABLE_BUNDLED_TAGLIB} MATCHES "false")
  message(STATUS "[taglibmetadatareader] using bundled taglib = true")
  message(STATUS "[taglibmetadatareader] CMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}")

  include(ExternalProject)

  ExternalProject_Add(taglib
    SOURCE_DIR
      "${musikcube_SOURCE_DIR}/src/plugins/taglib_plugin/taglib-1.11"
    BUILD_IN_SOURCE 0
    DOWNLOAD_COMMAND ""
    UPDATE_COMMAND ""
    CMAKE_ARGS
      "-DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}"
      "-DCMAKE_INSTALL_PREFIX=${musikcube_SOURCE_DIR}/src/plugins/taglib_plugin/taglib-1.11/stage/"
    INSTALL_COMMAND
      make install)

  include_directories("${musikcube_SOURCE_DIR}/src/plugins/taglib_plugin/taglib-1.11/stage/include")
  add_dependencies(taglibreader taglib)
  set(LIBTAG "${musikcube_SOURCE_DIR}/src/plugins/taglib_plugin/taglib-1.11/stage/lib/libtag.a")
else()
  find_library(LIBTAG tag)
endif()

message(STATUS "[taglibmetadatareader] using ${LIBTAG}")
target_link_libraries(taglibreader ${Boost_LIBRARIES} ${LIBTAG} ${LIBZ})
