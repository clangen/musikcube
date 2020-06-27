include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/../taglib
  ${CMAKE_CURRENT_SOURCE_DIR}/../taglib/toolkit
  ${CMAKE_CURRENT_SOURCE_DIR}/../taglib/ape
  ${CMAKE_CURRENT_SOURCE_DIR}/../taglib/mpeg
  ${CMAKE_CURRENT_SOURCE_DIR}/../taglib/mpeg/id3v1
  ${CMAKE_CURRENT_SOURCE_DIR}/../taglib/mpeg/id3v2
  ${CMAKE_CURRENT_SOURCE_DIR}/../taglib/mpeg/id3v2/frames
  ${CMAKE_CURRENT_SOURCE_DIR}/../bindings/c/
)

if(NOT BUILD_SHARED_LIBS)
  add_definitions(-DTAGLIB_STATIC)
endif()

########### next target ###############

add_executable(tagreader tagreader.cpp)
target_link_libraries(tagreader tag)

########### next target ###############

add_executable(tagreader_c tagreader_c.c)
target_link_libraries(tagreader_c tag_c)

########### next target ###############

add_executable(tagwriter tagwriter.cpp)
target_link_libraries(tagwriter tag)

########### next target ###############

add_executable(framelist framelist.cpp)
target_link_libraries(framelist tag)

########### next target ###############

add_executable(strip-id3v1 strip-id3v1.cpp)
target_link_libraries(strip-id3v1 tag)

