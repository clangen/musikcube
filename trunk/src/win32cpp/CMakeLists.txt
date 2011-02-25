FILE (GLOB WIN32CPP_SOURCES *.cpp)
FILE (GLOB WIN32CPP_HEADERS *.hpp)

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

include_directories( ${musikCube_SOURCE_DIR}/src/win32cpp )

if (WIN32)
	#Just until I can figure out how to get rid of the last linker errors when exporting dll functions
	add_library( win32cpp STATIC ${WIN32CPP_SOURCES} ${WIN32CPP_HEADERS})
else (WIN32)
	add_library( win32cpp SHARED ${WIN32CPP_SOURCES} ${WIN32CPP_HEADERS})
endif (WIN32)
#target_link_libraries( win32cpp comctl32 )


