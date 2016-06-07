set (taglibreader_SOURCES
  stdafx.cpp
  taglib_plugin.cpp
  TaglibMetadataReader.cpp
)

add_library(taglibreader SHARED ${taglibreader_SOURCES})

if (CMAKE_SYSTEM_NAME MATCHES "Linux")
  target_link_libraries(taglibreader ${BOOST_LINK_LIBS} tag.a)
else (CMAKE_SYSTEM_NAME MATCHES "Linux")
  target_link_libraries(taglibreader ${BOOST_LINK_LIBS} tag)
endif (CMAKE_SYSTEM_NAME MATCHES "Linux")