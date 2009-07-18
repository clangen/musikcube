set ( httpstream_SOURCES
	httpstream_plugin.cpp
	HTTPRequest.cpp
	HTTPStream.cpp
	HTTPStreamFactory.cpp
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
)

add_library( httpstream SHARED ${httpstream_SOURCES} )
target_link_libraries( httpstream ${musikCube_LINK_LIBS})


