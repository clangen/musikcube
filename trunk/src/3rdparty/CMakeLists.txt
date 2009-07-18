if (UNIX AND NOT WIN32)
	if (CMAKE_SIZEOF_VOID_P MATCHES "8")
	        add_definitions(-fPIC)
	endif (CMAKE_SIZEOF_VOID_P MATCHES "8")
endif (UNIX AND NOT WIN32)

set (3rdParty_MD5_SOURCES
	include/md5/md5.h
	src/md5/md5.c
)

include_directories ( include/md5 )
add_library( md5 STATIC ${3rdParty_MD5_SOURCES} )
