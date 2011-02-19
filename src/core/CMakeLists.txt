set(CORE_SOURCES 
  pch.cpp
  PlaybackQueue.cpp
  Common.cpp
  Crypt.cpp
  Image.cpp
  MessageQueue.cpp
  Preferences.cpp
  ThreadHelper.cpp
  tracklist/Base.cpp
  tracklist/LibraryList.cpp
  tracklist/MultiLibraryList.cpp
  Indexer.cpp
  LibraryFactory.cpp
  Query/Base.cpp
  Query/Factory.cpp
  Query/ListBase.cpp
  Query/ListSelection.cpp
  Query/SortTracks.cpp
  Query/SortTracksWithData.cpp
  Query/TrackMetadata.cpp
  Library/Base.cpp
  Library/LocalDB.cpp
  Library/Remote.cpp
  db/CachedStatement.cpp
  db/Connection.cpp
  db/ScopedTransaction.cpp
  db/Statement.cpp
  MetadataValue.cpp
  MetaKey.cpp
  NonLibraryTrackHelper.cpp
  PluginFactory.cpp
  audio/Buffer.cpp
  audio/Player.cpp
  audio/Stream.cpp
  audio/Transport.cpp
  server/Connection.cpp
  server/User.cpp
  server/UserSession.cpp
  xml/Node.cpp
  xml/Parser.cpp
  xml/ParserNode.cpp
  xml/Socket.cpp
  xml/Writer.cpp
  xml/WriterNode.cpp
  http/RequestParser.cpp
  http/Responder.cpp
  http/Server.cpp
  http/TrackSender.cpp
  filestreams/Factory.cpp
  filestreams/LocalFileStream.cpp
  GenericTrack.cpp
  IndexerTrack.cpp
  LibraryTrack.cpp
  Track.cpp
  TrackFactory.cpp
  Server.cpp
)

set (CORE_HEADERS
	Common.h
	config.h
	config_filesystem.h
	config_format.h
	Crypt.h
	Image.h
	Indexer.h
	IndexerTrack.h
	IPlugin.h
	ITrack.h
	LibraryFactory.h
	MessageQueue.h
	MetadataValue.h
	MetaKey.h
	NonLibraryTrackHelper.h
	pch.hpp
	PlaybackQueue.h
	PluginFactory.h
	Preferences.h
	Server.h
	ThreadHelper.h
	Track.h
	TrackFactory.h
	Version.h
	audio/Buffer.h
	audio/IAnalyzer.h
	audio/IBuffer.h
	audio/IDecoder.h
	audio/IDecoderFactory.h
	audio/IDSP.h
	audio/IOutput.h
	audio/IPlayer.h
	audio/Player.h
	audio/Stream.h
	audio/Transport.h
	db/CachedStatement.h
	db/Connection.h
	db/dbconfig.h
	db/ScopedTransaction.h
	db/Statement.h
	filestreams/Factory.h
	filestreams/IFileStream.h
	filestreams/IFileStreamFactory.h
	filestreams/LocalFileStream.h
	http/IRequestParser.h
	http/IRequestPlugin.h
	http/IResponder.h
	http/RequestParser.h
	http/Responder.h
	http/Server.h
	http/TrackSender.h
	Library/Base.h
	Library/LocalDB.h
	Library/Remote.h
	Plugin/IMetaDataReader.h
	Query/Base.h
	Query/Factory.h
	Query/ListBase.h
	Query/ListSelection.h
	Query/PlaylistLoad.h
	Query/Playlists.h
	Query/PlaylistSave.h
	Query/SortTracks.h
	Query/SortTracksWithData.h
	Query/TrackMetadata.h
	server/Connection.h
	server/User.h
	server/UserSession.h
	tracklist/Base.h
	tracklist/LibraryList.h
	tracklist/MultiLibraryList.h
	xml/IReadSupplier.h
	xml/IWriteSupplier.h
	xml/Node.h
	xml/Parser.h
	xml/ParserNode.h
	xml/Socket.h
	xml/Writer.h
	xml/WriterNode.h
)

SOURCE_GROUP(audio FILES
	audio/Buffer.cpp
	audio/Player.cpp
	audio/Stream.cpp
	audio/Transport.cpp
	audio/IAnalyzer.h
	audio/IBuffer.h
	audio/IDecoder.h
	audio/IDecoderFactory.h
	audio/IDSP.h
	audio/IOutput.h
	audio/IPlayer.h
	audio/Player.h
	audio/Stream.h
	audio/Transport.h)
SOURCE_GROUP(db FILES 
	db/CachedStatement.cpp
	db/Connection.cpp
	db/ScopedTransaction.cpp
	db/Statement.cpp
	db/CachedStatement.h
	db/Connection.h
	db/dbconfig.h
	db/ScopedTransaction.h
	db/Statement.h)
SOURCE_GROUP(filestreams FILES 
	filestreams/Factory.cpp
	filestreams/LocalFileStream.cpp
	filestreams/Factory.h
	filestreams/IFileStream.h
	filestreams/IFileStreamFactory.h
	filestreams/LocalFileStream.h)
SOURCE_GROUP(http FILES 
	http/RequestParser.cpp
	http/Responder.cpp
	http/Server.cpp
	http/TrackSender.cpp
	http/IRequestParser.h
	http/IRequestPlugin.h
	http/IResponder.h
	http/RequestParser.h
	http/Responder.h
	http/Server.h
	http/TrackSender.h)
SOURCE_GROUP(library FILES 
	Library/Base.cpp
	Library/LocalDB.cpp
	Library/Remote.cpp
	Library/Base.h
	Library/LocalDB.h
	Library/Remote.h)
SOURCE_GROUP(plugin FILES
	Plugin/IMetaDataReader.h)
SOURCE_GROUP(query FILES 
	Query/Base.cpp
	Query/Factory.cpp
	Query/ListBase.cpp
	Query/ListSelection.cpp
	Query/SortTracks.cpp
	Query/SortTracksWithData.cpp
	Query/TrackMetadata.cpp
	Query/Base.h
	Query/Factory.h
	Query/ListBase.h
	Query/ListSelection.h
	Query/PlaylistLoad.h
	Query/Playlists.h
	Query/PlaylistSave.h
	Query/SortTracks.h
	Query/SortTracksWithData.h
	Query/TrackMetadata.h)
SOURCE_GROUP(server FILES 
	server/Connection.cpp
	server/User.cpp
	server/UserSession.cpp
	server/Connection.h
	server/User.h
	server/UserSession.h)
SOURCE_GROUP(tracklist FILES 
	tracklist/Base.cpp
	tracklist/LibraryList.cpp
	tracklist/MultiLibraryList.cpp
	tracklist/Base.h
	tracklist/LibraryList.h
	tracklist/MultiLibraryList.h)
SOURCE_GROUP(xml FILES 
	xml/Node.cpp
	xml/Parser.cpp
	xml/ParserNode.cpp
	xml/Socket.cpp
	xml/Writer.cpp
	xml/WriterNode.cpp
	xml/IReadSupplier.h
	xml/IWriteSupplier.h
	xml/Node.h
	xml/Parser.h
	xml/ParserNode.h
	xml/Socket.h
	xml/Writer.h
	xml/WriterNode.h)


if(CMAKE_SYSTEM_NAME MATCHES "Windows")
  add_definitions(-DWIN32)
	if(NOT DEFINED MINGW)
        	
	endif(NOT DEFINED MINGW)
else(CMAKE_SYSTEM_NAME MATCHES "Windows")
  set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} -fpermissive)
endif(CMAKE_SYSTEM_NAME MATCHES "Windows")

include_directories(
	..
	../3rdparty/include
	../3rdparty/include/jpeg-6b
)

add_definitions( 
  -DXML_STATIC
  -D_CRT_SECURE_NO_DEPRECATE
  -D_DEBUG
)

add_library( musik SHARED ${CORE_SOURCES} ${CORE_HEADERS})
target_link_libraries( musik ${musikCube_LINK_LIBS})


