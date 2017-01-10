set(GLUE_SOURCES
  ./audio/MasterTransport.cpp
  ./query/CategoryListQuery.cpp
  ./query/CategoryTrackListQuery.cpp
  ./query/NowPlayingTrackListQuery.cpp
  ./query/SearchTrackListQuery.cpp
  ./util/Duration.cpp
  ./util/Playback.cpp
)

include_directories(
  .
  ..
  ../3rdparty/include
)

add_library(musikglue STATIC ${GLUE_SOURCES})
target_link_libraries(musikglue ${musikbox_LINK_LIBS} musikcore)
