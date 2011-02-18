set ( mpg123decoder_SOURCES
	mp3decoder_plugin.cpp
	MP3Decoder.cpp
	MP3SourceSupplier.cpp
	stdafx.cpp
	)

if(CMAKE_SYSTEM_NAME MATCHES "Windows")
  add_definitions(-DWIN32)
  if (${MSVC90})
	message(STATUS "VC 2008 found. Including libmpg123.dll")
	include_directories( 
		${musikCube_SOURCE_DIR}/src/contrib/mpg123decoder/mpg123/src
		${musikCube_SOURCE_DIR}/src/contrib/mpg123decoder/mpg123/src/libmpg123
		${musikCube_SOURCE_DIR}/src/contrib/mpg123decoder/mpg123/ports/MSVC++
		${musikCube_SOURCE_DIR}/src/contrib/mpg123decoder/VC2008 )
	link_directories( ${musikCube_SOURCE_DIR}/src/contrib/mpg123decoder/VC2008 )
  else (${MSVC90})
    message(STATUS "VC 2008 not found")
  endif (${MSVC90})
else(CMAKE_SYSTEM_NAME MATCHES "Windows")
  set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} -fpermissive)
endif(CMAKE_SYSTEM_NAME MATCHES "Windows")

add_definitions( 
  -DXML_STATIC
  -D_CRT_SECURE_NO_DEPRECATE
  -D_DEBUG
)

add_library( mpg123decoder SHARED ${mpg123decoder_SOURCES} )
target_link_libraries( mpg123decoder ${musikCube_LINK_LIBS} mpg123)