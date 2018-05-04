set(CORE_SOURCES
  ./debug.cpp
  ./audio/Buffer.cpp
  ./audio/Crossfader.cpp
  ./audio/CrossfadeTransport.cpp
  ./audio/GaplessTransport.cpp
  ./audio/MasterTransport.cpp
  ./audio/Outputs.cpp
  ./audio/PlaybackService.cpp
  ./audio/Player.cpp
  ./audio/Stream.cpp
  ./audio/Streams.cpp
  ./audio/Visualizer.cpp
  ./db/Connection.cpp
  ./db/ScopedTransaction.cpp
  ./db/Statement.cpp
  ./i18n/Locale.cpp
  ./io/DataStreamFactory.cpp
  ./io/LocalFileStream.cpp
  ./library/Indexer.cpp
  ./library/LibraryFactory.cpp
  ./library/LocalLibrary.cpp
  ./library/LocalSimpleDataProvider.cpp
  ./library/query/local/AlbumListQuery.cpp
  ./library/query/local/AllCategoriesQuery.cpp
  ./library/query/local/AppendPlaylistQuery.cpp
  ./library/query/local/GetPlaylistQuery.cpp
  ./library/query/local/CategoryListQuery.cpp
  ./library/query/local/CategoryTrackListQuery.cpp
  ./library/query/local/DeletePlaylistQuery.cpp
  ./library/query/local/DirectoryTrackListQuery.cpp
  ./library/query/local/NowPlayingTrackListQuery.cpp
  ./library/query/local/PersistedPlayQueueQuery.cpp
  ./library/query/local/ReplayGainQuery.cpp
  ./library/query/local/SavePlaylistQuery.cpp
  ./library/query/local/SearchTrackListQuery.cpp
  ./library/query/local/TrackMetadataQuery.cpp
  ./library/query/local/util/CategoryQueryUtil.cpp
  ./library/metadata/MetadataMap.cpp
  ./library/metadata/MetadataMapList.cpp
  ./library/track/IndexerTrack.cpp
  ./library/track/LibraryTrack.cpp
  ./library/track/Track.cpp
  ./library/track/TrackList.cpp
  ./plugin/PluginFactory.cpp
  ./plugin/Plugins.cpp
  ./runtime/Message.cpp
  ./runtime/MessageQueue.cpp
  ./support/Duration.cpp
  ./support/Common.cpp
  ./support/LastFm.cpp
  ./support/Playback.cpp
  ./support/Preferences.cpp
  ./support/PreferenceKeys.cpp
  ../3rdparty/src/sqlite/sqlite3.c
  ../3rdparty/src/kiss_fft.c
  ../3rdparty/src/kiss_fftr.c
)

include_directories(
  .
  ..
  ../3rdparty/include
  ../3rdparty/include/md5
  ../3rdparty/include/sqlite
)

add_library(musikcore SHARED ${CORE_SOURCES})

set_target_properties(musikcore PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY ${musikcube_SOURCE_DIR}/bin)

target_link_libraries(musikcore ${musikcube_LINK_LIBS})
