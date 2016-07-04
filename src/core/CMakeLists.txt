set(CORE_SOURCES
  ./debug.cpp
  ./audio/Buffer.cpp
  ./audio/Player.cpp
  ./audio/Stream.cpp
  ./audio/GaplessTransport.cpp
  ./db/Connection.cpp
  ./db/ScopedTransaction.cpp
  ./db/Statement.cpp
  ./io/DataStreamFactory.cpp
  ./io/LocalFileStream.cpp
  ./library/Indexer.cpp
  ./library/LibraryFactory.cpp
  ./library/LocalLibrary.cpp
  ./library/metadata/MetadataKeyValue.cpp
  ./library/metadata/MetadataValue.cpp
  ./library/query/QueryBase.cpp
  ./library/track/IndexerTrack.cpp
  ./library/track/LibraryTrack.cpp
  ./library/track/Track.cpp
  ./plugin/PluginFactory.cpp
  ./support/Common.cpp
  ./support/NonLibraryTrackHelper.cpp
  ./support/Preferences.cpp
  ./support/ThreadHelper.cpp
  ./support/Version.cpp
  ../3rdparty/src/md5/md5.c
  ../3rdparty/src/sqlite/sqlite3.c
)

include_directories(
  .
  ..
  ../3rdparty/include
  ../3rdparty/include/md5
  ../3rdparty/include/sqlite
)

add_definitions(
  -D_DEBUG
)

add_library(musikcore STATIC ${CORE_SOURCES})
target_link_libraries(musikcore ${musikbox_LINK_LIBS})
