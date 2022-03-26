set(CORE_SOURCES
  ./c_context.cpp
  ./c_interface_wrappers.cpp
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
  ./db/SqliteExtensions.cpp
  ./db/Statement.cpp
  ./i18n/Locale.cpp
  ./io/DataStreamFactory.cpp
  ./io/LocalFileStream.cpp
  ./library/Indexer.cpp
  ./library/LibraryFactory.cpp
  ./library/LocalLibrary.cpp
  ./library/LocalMetadataProxy.cpp
  ./library/MasterLibrary.cpp
  ./library/QueryRegistry.cpp
  ./library/RemoteLibrary.cpp
  ./library/query/AlbumListQuery.cpp
  ./library/query/AllCategoriesQuery.cpp
  ./library/query/AppendPlaylistQuery.cpp
  ./library/query/GetPlaylistQuery.cpp
  ./library/query/CategoryListQuery.cpp
  ./library/query/CategoryTrackListQuery.cpp
  ./library/query/DeletePlaylistQuery.cpp
  ./library/query/DirectoryTrackListQuery.cpp
  ./library/query/LyricsQuery.cpp
  ./library/query/MarkTrackPlayedQuery.cpp
  ./library/query/NowPlayingTrackListQuery.cpp
  ./library/query/PersistedPlayQueueQuery.cpp
  ./library/query/SavePlaylistQuery.cpp
  ./library/query/SearchTrackListQuery.cpp
  ./library/query/SetTrackRatingQuery.cpp
  ./library/query/TrackMetadataBatchQuery.cpp
  ./library/query/TrackMetadataQuery.cpp
  ./library/query/util/CategoryQueryUtil.cpp
  ./library/query/util/Serialization.cpp
  ./library/metadata/MetadataMap.cpp
  ./library/metadata/MetadataMapList.cpp
  ./library/track/IndexerTrack.cpp
  ./library/track/LibraryTrack.cpp
  ./library/track/Track.cpp
  ./library/track/TrackList.cpp
  ./net/PiggyWebSocketClient.cpp
  ./net/RawWebSocketClient.cpp
  ./net/WebSocketClient.cpp
  ./plugin/PluginFactory.cpp
  ./plugin/Plugins.cpp
  ./runtime/Message.cpp
  ./runtime/MessageQueue.cpp
  ./support/Auddio.cpp
  ./support/Duration.cpp
  ./support/Common.cpp
  ./support/LastFm.cpp
  ./support/PiggyDebugBackend.cpp
  ./support/Playback.cpp
  ./support/Preferences.cpp
  ./support/PreferenceKeys.cpp
  ../3rdparty/src/sqlite/sqlite3.c
  ../3rdparty/src/kiss_fft.c
  ../3rdparty/src/kiss_fftr.c
  ../3rdparty/src/md5.c
)

include_directories(
  .
  ..
  ../3rdparty/include
  ../3rdparty/include/md5
  ../3rdparty/include/sqlite
)

add_definitions(-DMCSDK_DEFINE_EXPORTS)
add_library(musikcore SHARED ${CORE_SOURCES})

set_target_properties(musikcore PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${musikcube_SOURCE_DIR}/bin)
target_link_libraries(musikcore ${musikcube_LINK_LIBS})
target_include_directories(musikcore BEFORE PRIVATE ${VENDOR_INCLUDE_DIRECTORIES})

if (ENABLE_PCH MATCHES "true")
    message(STATUS "[musikcore] enabling precompiled headers")
    include(./pch.cmake)
endif()
