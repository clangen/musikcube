set(GLUE_SOURCES
  ./audio/MasterTransport.cpp
  ./util/Duration.cpp
  ./util/Playback.cpp
)

include_directories(
  .
  ..
  ../3rdparty/include
)

add_library(musikglue STATIC ${GLUE_SOURCES})
target_link_libraries(musikglue ${musikcube_LINK_LIBS} musikcore)
