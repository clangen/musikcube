set ( mpg123decoder_SOURCES
	mp3decoder_plugin.cpp
	MP3Decoder.cpp
	MP3SourceSupplier.cpp
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

add_library( mpg123decoder SHARED ${mpg123decoder_SOURCES} )
target_link_libraries( mpg123decoder ${musikCube_LINK_LIBS})


