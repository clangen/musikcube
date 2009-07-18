set ( bpm_analyzer_SOURCES
	bpm_analyzer_plugin.cpp
	BPMAnalyzer.cpp
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

add_library( bpm_analyzer SHARED ${bpm_analyzer_SOURCES} )
target_link_libraries( bpm_analyzer ${musikCube_LINK_LIBS})


