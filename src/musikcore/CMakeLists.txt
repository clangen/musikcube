set(CORE_SOURCES
  ./debug.cpp
  ./audio/Buffer.cpp
  ./audio/Crossfader.cpp
  ./audio/CrossfadeTransport.cpp
  ./audio/GaplessTransport.cpp
  ./audio/MasterTransport.cpp
  ./audio/Outputs.cpp
  ./audio/PlaybackService.cpp
  ./runtime/ManagedRuntime.cpp
  ./ffi/consumer/online.cpp
  ./audio/PlaybackHandle.cpp
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
  ../common/runtime/Message.cpp
  ../common/runtime/MessageQueue.cpp
  ./support/Auddio.cpp
  ./support/Common.cpp
  ./support/LastFm.cpp
  ./support/PiggyDebugBackend.cpp
  ./support/Playback.cpp
  ./support/Preferences.cpp
  ./support/PreferenceKeys.cpp
  ../3rdparty/src/sqlean/unicode/extension.c
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

add_library(musikcore SHARED ${CORE_SOURCES})
musikcore_add_consumer_api(musikcore "${CMAKE_CURRENT_SOURCE_DIR}/ffi/consumer/remote-session.cpp")

set_target_properties(musikcore PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${MUSIKCUBE_RUNTIME_OUTPUT_DIRECTORY}")
target_link_libraries(musikcore PUBLIC ${musikcube_LINK_LIBS})
target_include_directories(musikcore BEFORE PRIVATE ${VENDOR_INCLUDE_DIRECTORIES})
find_header(asio.hpp)

if (ENABLE_PCH MATCHES "true")
    message(STATUS "[musikcore] enabling precompiled headers")
    include(./pch.cmake)
endif()
