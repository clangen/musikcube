set(CORE_SOURCES
  ./debug.cpp
  ./pch.cpp
  ./audio/Buffer.cpp
  ./audio/Player.cpp
  ./audio/Stream.cpp
  ./db/CachedStatement.cpp
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
  ./playback/NonLibraryTrackHelper.cpp
  ./playback/Transport.cpp
  ./plugin/PluginFactory.cpp
  ./support/Common.cpp
  ./support/Preferences.cpp
  ./support/ThreadHelper.cpp
  ./support/Version.cpp
)

set (CORE_HEADERS
  ./config.h
  ./debug.h
  ./pch.hpp
  ./audio/Buffer.h
  ./audio/Player.h
  ./audio/Stream.h
  ./db/CachedStatement.h
  ./db/Connection.h
  ./db/ScopedTransaction.h
  ./db/Statement.h
  ./db/dbconfig.h
  ./io/DataStreamFactory.h
  ./io/LocalFileStream.h
  ./library/IIndexer.h
  ./library/ILibrary.h
  ./library/IQuery.h
  ./library/Indexer.h
  ./library/LibraryFactory.h
  ./library/LocalLibrary.h
  ./library/LocalLibraryConstants.h
  ./library/metadata/MetadataKeyValue.h
  ./library/metadata/MetadataValue.h
  ./library/query/QueryBase.h
  ./library/track/IndexerTrack.h
  ./library/track/LibraryTrack.h
  ./library/track/Track.h
  ./playback/NonLibraryTrackHelper.h
  ./playback/Transport.h
  ./plugin/PluginFactory.h
  ./sdk/IAnalyzer.h
  ./sdk/IBuffer.h
  ./sdk/IBufferProvider.h
  ./sdk/IDSP.h
  ./sdk/IDataStream.h
  ./sdk/IDataStreamFactory.h
  ./sdk/IDecoder.h
  ./sdk/IDecoderFactory.h
  ./sdk/IMetadataReader.h
  ./sdk/IMetadataWriter.h
  ./sdk/IOutput.h
  ./sdk/IPlugin.h
  ./sdk/config.h
  ./support/Common.h
  ./support/Preferences.h
  ./support/ThreadHelper.h
  ./support/Version.h
)

set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} -fpermissive)

include_directories(
  .
  ..
  ../3rdparty/include
)

add_definitions(
  -D_CRT_SECURE_NO_DEPRECATE
  -D_DEBUG
)

add_library(musikcore STATIC ${CORE_SOURCES} ${CORE_HEADERS})
target_link_libraries(musikcore ${musikbox_LINK_LIBS})
