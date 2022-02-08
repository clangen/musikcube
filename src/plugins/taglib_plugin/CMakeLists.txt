set (taglibreader_SOURCES
  taglib_plugin.cpp
  TaglibMetadataReader.cpp
)

add_library(taglibreader SHARED ${taglibreader_SOURCES})
include_directories(${Boost_INCLUDE_DIRS})

if (NOT ${ENABLE_BUNDLED_TAGLIB} MATCHES "false")
  message(STATUS "[taglibmetadatareader] using bundled taglib = true")

  include(ExternalProject)

  ExternalProject_Add(taglib
    SOURCE_DIR "${musikcube_SOURCE_DIR}/src/plugins/taglib_plugin/taglib-1.11"
    BUILD_IN_SOURCE 0
    DOWNLOAD_COMMAND ""
    UPDATE_COMMAND ""
    CMAKE_ARGS "-DCMAKE_INSTALL_PREFIX:PATH=${musikcube_SOURCE_DIR}/src/plugins/taglib_plugin/taglib-1.11/stage/"
    INSTALL_COMMAND make install)

  include_directories("${musikcube_SOURCE_DIR}/src/plugins/taglib_plugin/taglib-1.11/stage/include")
  add_dependencies(taglibreader taglib)
  target_link_libraries(taglibreader ${Boost_LIBRARIES} "${musikcube_SOURCE_DIR}/src/plugins/taglib_plugin/taglib-1.11/stage/lib/libtag.a" z)
  message(STATUS "[taglibmetadatareader] using ${musikcube_SOURCE_DIR}/src/plugins/taglib_plugin/taglib-1.11/stage/lib/libtag.a")
else()
  message(STATUS "[taglibmetadatareader] using bundled taglib = false")
  ensure_library_exists(tag)
  target_link_libraries(taglibreader ${Boost_LIBRARIES} tag z)
endif()
