set (taglibreader_SOURCES
  stdafx.cpp
  taglib_plugin.cpp
  TaglibMetadataReader.cpp
)

add_library(taglibreader SHARED ${taglibreader_SOURCES})

target_link_libraries(taglibreader ${BOOST_LINK_LIBS} tag.a z)
