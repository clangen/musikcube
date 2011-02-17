set ( mp3decoder_SOURCES
	BaseDecoder.cpp
	BitStream.cpp
	CRC.cpp
	FrameSplitter.cpp
	Header.cpp
	Layer3Decoder.cpp
	mp3decoder_plugin.cpp
	MP3Decoder.cpp
	MP3SourceSupplier.cpp
	SideInfo.cpp
	stdafx.cpp
	)

if(CMAKE_SYSTEM_NAME MATCHES "Windows")
  add_definitions(-DWIN32)
else(CMAKE_SYSTEM_NAME MATCHES "Windows")
  set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} -fpermissive)
endif(CMAKE_SYSTEM_NAME MATCHES "Windows")

add_definitions( 
  -DXML_STATIC
  -D_CRT_SECURE_NO_DEPRECATE
  -D_DEBUG
)

add_library( mp3decoder SHARED ${mp3decoder_SOURCES} )
target_link_libraries( mp3decoder ${musikCube_LINK_LIBS} )


