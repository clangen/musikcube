add_subdirectory(taglib-1.6.3)

set ( taglib_plugin_SOURCES
	taglib_plugin.cpp
	TagReaderTaglib.cpp
	stdafx.cpp
	)

if(CMAKE_SYSTEM_NAME MATCHES "Windows")
  add_definitions(-DWIN32)
	if(NOT DEFINED MINGW)
        	
	endif(NOT DEFINED MINGW)
else(CMAKE_SYSTEM_NAME MATCHES "Windows")
  set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} -fpermissive)
endif(CMAKE_SYSTEM_NAME MATCHES "Windows")

add_definitions( 
  -DXML_STATIC
  -D_CRT_SECURE_NO_DEPRECATE
  -D_DEBUG
  -DTAGLIB_NO_CONFIG
)

include_directories( ${musikCube_SOURCE_DIR}/src/contrib/taglib_plugin/taglib-1.6.3/ )
include_directories( ${musikCube_SOURCE_DIR}/src/contrib/taglib_plugin/taglib-1.6.3/taglib )
include_directories( ${musikCube_SOURCE_DIR}/src/contrib/taglib_plugin/taglib-1.6.3/taglib/toolkit )
include_directories( ${musikCube_SOURCE_DIR}/src/contrib/taglib_plugin/taglib-1.6.3/taglib/mpeg/id3v2 )
include_directories( ${musikCube_SOURCE_DIR}/src/contrib/taglib_plugin/taglib-1.6.3/taglib/mpeg/id3v2/frames )

add_library( taglib_plugin SHARED ${taglib_plugin_SOURCES} )
target_link_libraries( taglib_plugin ${musikCube_LINK_LIBS} tag)


