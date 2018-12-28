set (taglibreader_SOURCES
  stdafx.cpp
  taglib_plugin.cpp
  TaglibMetadataReader.cpp
)

if (${FFMPEG_DECODER} MATCHES "false")
  message(STATUS "[taglibmetadatareader] *not* defining FFMPEG_DECODER")
else()
  message(STATUS "[taglibmetadatareader] defining FFMPEG_DECODER")
  add_definitions(-DFFMPEG_DECODER)
endif()

add_library(taglibreader SHARED ${taglibreader_SOURCES})

if (${USE_BUNDLED_TAGLIB} MATCHES "true")
  target_link_libraries(taglibreader ${BOOST_LINK_LIBS} tag.a z)
else()
  target_link_libraries(taglibreader ${BOOST_LINK_LIBS} tag z)
endif()
