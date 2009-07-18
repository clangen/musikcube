set ( alsaout_SOURCES
	alsaout_plugin.cpp
	AlsaOut.cpp
	AlsaOutBuffer.cpp
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

#include_directories( ../../core )
include_directories(/usr/include)

add_library( alsaout SHARED ${alsaout_SOURCES} )
target_link_libraries( alsaout ${musikCube_LINK_LIBS} asound)


