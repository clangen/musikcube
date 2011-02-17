if (UNIX AND NOT WIN32)
	if (CMAKE_SIZEOF_VOID_P MATCHES "8")
	        add_definitions(-fPIC)
	endif (CMAKE_SIZEOF_VOID_P MATCHES "8")
endif (UNIX AND NOT WIN32)

set (3rdParty_MD5_SOURCES
	include/md5/md5.h
	src/md5/md5.c
)

set (3rdParty_SQLITE_SOURCES
	include/sqlite/sqlite3.h
	include/sqlite/sqlite3ext.h
	src/sqlite/sqlite3.c
)

set (ZLIB_INCLUDE_DIR ${musikCube_SOURCE_DIRECTORY}/src/3rdparty/include)
set (ZLIB_LIBRARY ${musikCube_SOURCE_DIRECTORY}/src/3rdparty/lib)

include_directories ( include/md5 )
add_library( md5 STATIC ${3rdParty_MD5_SOURCES} )
include_directories ( include/sqlite )

add_library( sqlite3 STATIC ${3rdParty_SQLITE_SOURCES} )
