set ( dsp_example_echo_SOURCES
	dsp_echo_plugin.cpp
	DSPEcho.cpp
	pch.cpp
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

add_library( dsp_example_echo SHARED ${dsp_example_echo_SOURCES} )
target_link_libraries( dsp_example_echo ${musikCube_LINK_LIBS})


