set ( flacdecoder_SOURCES
	FLACDecoder.cpp
	FLACSourceSupplier.cpp
	flacdecoder_plugin.cpp
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

add_library( flacdecoder SHARED ${flacdecoder_SOURCES} )
target_link_libraries( flacdecoder ${musikCube_LINK_LIBS} FLAC++)


