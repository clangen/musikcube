set (taglib_plugin_SOURCES
  stdafx.cpp
  taglib_plugin.cpp
  TaglibMetadataReader.cpp
)

add_definitions(
  -D_DEBUG
  -DTAGLIB_NO_CONFIG
)

add_library(taglib_plugin SHARED ${taglib_plugin_SOURCES})
target_link_libraries(taglib_plugin ${BOOST_LINK_LIBS} tag)
