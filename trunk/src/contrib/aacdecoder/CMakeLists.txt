set ( aacdecoder_SOURCES
	aacdecoder_plugin.cpp
	AACSourceSupplier.cpp
	stdafx.cpp
	drms.c
	m4aAudioSource.cpp
	mp4atom.c
	mp4ff.c
	mp4meta.c
	mp4sample.c
	mp4tagupdate.c
	mp4util.c
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

add_library( aacdecoder SHARED ${aacdecoder_SOURCES} )
target_link_libraries( aacdecoder ${musikCube_LINK_LIBS})


