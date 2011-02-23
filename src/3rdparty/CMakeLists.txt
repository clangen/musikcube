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

FILE(GLOB_RECURSE EXPAT_HEADERS include/expat/*.h)
FILE(GLOB_RECURSE EXPAT_SRC src/expat/*.c)
set (3rdParty_EXPAT_SOURCES
	${EXPAT_HEADERS}
	${EXPAT_SRC}
)

set (ZLIB_INCLUDE_DIR ${musikCube_SOURCE_DIRECTORY}/src/3rdparty/include)
set (ZLIB_LIBRARY ${musikCube_SOURCE_DIRECTORY}/src/3rdparty/lib)

include_directories ( include/md5 )
add_library( md5 STATIC ${3rdParty_MD5_SOURCES} )

include_directories ( include/sqlite )
add_library( sqlite3 STATIC ${3rdParty_SQLITE_SOURCES} )

include_directories ( include/expat )
if (WIN32)
add_definitions(
	-DHAVE_MEMMOVE
)
#Must find a better way of solving this
endif (WIN32)
add_library( expat STATIC ${3rdParty_EXPAT_SOURCES} )
