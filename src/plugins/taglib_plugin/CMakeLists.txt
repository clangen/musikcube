set (taglibreader_SOURCES
  taglib_plugin.cpp
  TaglibMetadataReader.cpp
)

if (${ENABLE_FFMPEG} MATCHES "false")
  message(STATUS "[taglibmetadatareader] *not* defining ENABLE_FFMPEG")
else()
  message(STATUS "[taglibmetadatareader] defining ENABLE_FFMPEG")
  add_definitions(-DENABLE_FFMPEG)
endif()

add_library(taglibreader SHARED ${taglibreader_SOURCES})
  
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
  target_link_libraries(taglibreader ${BOOST_LINK_LIBS} "${musikcube_SOURCE_DIR}/src/plugins/taglib_plugin/taglib-1.11/stage/lib/libtag.a" z)
  message(STATUS "[taglibmetadatareader] using ${musikcube_SOURCE_DIR}/src/plugins/taglib_plugin/taglib-1.11/stage/lib/libtag.a")
else()
  message(STATUS "[taglibmetadatareader] using bundled taglib = false")
  target_link_libraries(taglibreader ${BOOST_LINK_LIBS} tag z)
endif()
