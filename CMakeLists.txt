cmake_minimum_required(VERSION 2.6)
project( musikCube )

SET (CMAKE_BUILD_TYPE DEBUG)
set (LIBRARY_OUTPUT_PATH ${musikCube_SOURCE_DIR}/lib)
set (EXECUTABLE_OUTPUT_PATH ${musikCibe_SOURCE_DIR}/bin)

add_subdirectory(src/core)
