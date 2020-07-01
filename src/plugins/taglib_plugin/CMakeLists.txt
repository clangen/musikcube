set (taglibreader_SOURCES
  stdafx.cpp
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

if (${USE_BUNDLED_TAGLIB} MATCHES "true")
  message(STATUS "[taglibmetadatareader] using ${musikcube_SOURCE_DIR}/src/plugins/taglib_plugin/taglib-1.11/stage/lib/libtag.a")
  target_link_libraries(taglibreader ${BOOST_LINK_LIBS} "${musikcube_SOURCE_DIR}/src/plugins/taglib_plugin/taglib-1.11/stage/lib/libtag.a" z)
else()
  target_link_libraries(taglibreader ${BOOST_LINK_LIBS} tag z)
endif()
