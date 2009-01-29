cmake_minimum_required(VERSION 2.4)
PROJECT(mc2)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")

find_package(EXPAT REQUIRED)
include_directories(${EXPAT_INCLUDE_DIRS})
set(LIBS ${LIBS} ${EXPAT_LIBRARIES})
find_package(Boost REQUIRED)
include_directories(${Boost_INCLUDE_DIRS})
set(LIBS ${LIBS} ${Boost_LIBRARIES})
set(SQLITE_MIN_VERSION 3.5)
find_package(SQLITE REQUIRED)
set(LIBS ${LIBS} ${SQLITE_LIBRARIES})

include_directories(${PROJECT_SOURCE_DIR}/src)
include_directories(${PROJECT_SOURCE_DIR}/src/3rdparty/include)
add_subdirectory(src)
