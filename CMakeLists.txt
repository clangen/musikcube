cmake_minimum_required(VERSION 2.6)
project( musikCube )

set ( musikCube_VERSION_MAJOR 2)
set ( musikCube_VERSION_MINOR 0)


set (BOOST_LIBS
system
date_time
filesystem
iostreams
thread
regex
)

set (Boost_ADDITIONAL_VERSIONS "1.37.0" )
if(WIN32 AND NOT UNIX)
	find_package(Boost 1.36.0 COMPONENTS ${BOOST_LIBS} )
	if (Boost_FOUND)
		add_definitions (-DHAVE_BOOST)
	endif (Boost_FOUND)
else(WIN32 AND NOT UNIX)
	find_package(Boost 1.36.0 REQUIRED ${BOOST_LIBS}) 
	if(Boost_FOUND)
		add_definitions (-DHAVE_BOOST
				-D_FILE_OFFSET_BITS=64)
		set (BOOST_LINK_LIBS 
			${Boost_LIBRARIES})
	#	message(STATUS "Libs: " ${BOOST_LINK_LIBS})
	#	message(STATUS "Includes: " ${Boost_INCLUDE_DIRS})
	else(Boost_FOUND)
		set (BOOST_LINK_LIBS
			boost_system
			boost_iostreams
			boost_filesystem
			boost_thread
			boost_regex
			boost_date_time
			)
	endif(Boost_FOUND)
	find_package(Qt4 REQUIRED)
	#add_definitions(${QT_DEFINITIONS})
	#include_directories(${QT_INCLUDE_DIR})
	set (linux_LINK_LIBS
		dl
		expat
	)
	set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}
		${CMAKE_CURRENT_SOURCE_DIR}/Modules )
	#include(UseDoxygen)
	set(DOXYFILE_OUTPUT_DIRECTORY docs)
endif(WIN32 AND NOT UNIX)

SET (CMAKE_BUILD_TYPE DEBUG)
set (LIBRARY_OUTPUT_PATH ${musikCube_SOURCE_DIR}/lib)
set (EXECUTABLE_OUTPUT_PATH ${musikCube_SOURCE_DIR}/bin)

link_directories (${musikCube_SOURCE_DIR}/lib)

set (musikCube_LINK_LIBS
	${linux_LINK_LIBS}
	${BOOST_LINK_LIBS}
	sqlite3
	md5
)

include_directories (	"/usr/include/taglib"
			"${musikCube_SOURCE_DIR}/src"
			"${musikCube_SOURCE_DIR}/src/core"
			"${musikCube_SOURCE_DIR}/src/3rdparty/include")

add_subdirectory(src/3rdparty)
add_subdirectory(src/core)
add_subdirectory(src/contrib)
#add_subdirectory(src/server)
add_subdirectory(src/square)
#add_subdirectory(src/cube)
