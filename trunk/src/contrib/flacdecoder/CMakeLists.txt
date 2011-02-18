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

include_directories( ${musikCube_SOURCE_DIR}/src/contrib/flacdecoder/include )
find_package(flac)
if (flac_found)
	message(STATUS "FLAC found. Using system lib")
	link_directories ( flac_link_dirs )
else (flac_found)
	message(STATUS "FLAC not found")
	if (${MSVC90})
		message(STATUS "Using MSVC2008 (MSVC9.0) Prebuilt dll")
		link_directories ( ${musikCube_SOURCE_DIR}/src/contrib/flacdecoder/lib/VS2008/ )
	endif (${MSVC90})
endif (flac_found)
add_library( flacdecoder SHARED ${flacdecoder_SOURCES} )
target_link_libraries( flacdecoder ${musikCube_LINK_LIBS} FLAC FLAC++)


