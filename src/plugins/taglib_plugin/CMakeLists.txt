set (taglibreader_SOURCES
  taglib_plugin.cpp
  TaglibMetadataReader.cpp
)

if (${FFMPEG_ENABLED} MATCHES "false")
  message(STATUS "[taglibmetadatareader] *not* defining FFMPEG_ENABLED")
else()
  message(STATUS "[taglibmetadatareader] defining FFMPEG_ENABLED")
  add_definitions(-DFFMPEG_ENABLED)
endif()

add_library(taglibreader SHARED ${taglibreader_SOURCES})

message(STATUS "[taglibmetadatareader] using bundled taglib = ${USE_BUNDLED_TAGLIB}")

if (${USE_BUNDLED_TAGLIB} MATCHES "true")
  if (${USE_BUNDLED_TAGLIB} MATCHES "true")
    include(ExternalProject)

    ExternalProject_Add(taglib
      SOURCE_DIR "${musikcube_SOURCE_DIR}/src/plugins/taglib_plugin/taglib-1.11"
      BUILD_IN_SOURCE 0
      DOWNLOAD_COMMAND ""
      UPDATE_COMMAND ""
      CMAKE_ARGS "-DCMAKE_INSTALL_PREFIX:PATH=${musikcube_SOURCE_DIR}/src/plugins/taglib_plugin/taglib-1.11/stage/"
      INSTALL_COMMAND make install)

    include_directories("${musikcube_SOURCE_DIR}/src/plugins/taglib_plugin/taglib-1.11/stage/include")
  endif()
  add_dependencies(taglibreader taglib)
  target_link_libraries(taglibreader ${BOOST_LINK_LIBS} "${musikcube_SOURCE_DIR}/src/plugins/taglib_plugin/taglib-1.11/stage/lib/libtag.a" z)
  message(STATUS "[taglibmetadatareader] using ${musikcube_SOURCE_DIR}/src/plugins/taglib_plugin/taglib-1.11/stage/lib/libtag.a")
else()
  target_link_libraries(taglibreader ${BOOST_LINK_LIBS} tag z)
endif()
