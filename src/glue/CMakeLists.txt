set(GLUE_SOURCES
  ./audio/MasterTransport.cpp
  ./audio/PlaybackService.cpp
  ./model/TrackList.cpp
  ./util/Duration.cpp
  ./util/Playback.cpp
)

include_directories(
  .
  ..
  ../3rdparty/include
)

add_library(musikglue STATIC ${GLUE_SOURCES})
target_link_libraries(musikglue ${musikbox_LINK_LIBS})
