set ( oggdecoder_SOURCES
	oggdecoder_plugin.cpp
	OGGDecoder.cpp
	OggSourceSupplier.cpp
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

add_library( oggdecoder SHARED ${oggdecoder_SOURCES} )
target_link_libraries( oggdecoder ${musikCube_LINK_LIBS})


