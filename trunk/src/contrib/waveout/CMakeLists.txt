set ( waveout_SOURCES
	WaveOut.cpp
	WaveOutBuffer.cpp
	pch.cpp
	waveout_plugin.cpp
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

add_library( waveout SHARED ${waveout_SOURCES} )
target_link_libraries( waveout ${musikCube_LINK_LIBS})


