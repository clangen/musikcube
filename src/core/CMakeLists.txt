set(CORE_SOURCES 
  pch.cpp
  pch.hpp
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
  Server.h
)

SOURCE_GROUP(audio FILES
	audio/Buffer.cpp
	audio/Player.cpp
	audio/Stream.cpp
	audio/Transport.cpp)
SOURCE_GROUP(db FILES 
	db/CachedStatement.cpp
	db/Connection.cpp
	db/ScopedTransaction.cpp
	db/Statement.cpp)
SOURCE_GROUP(filestreams FILES 
	filestreams/Factory.cpp
	filestreams/LocalFileStream.cpp)
SOURCE_GROUP(http FILES 
	http/RequestParser.cpp
	http/Responder.cpp
	http/Server.cpp
	http/TrackSender.cpp)
SOURCE_GROUP(library FILES 
	Library/Base.cpp
	Library/LocalDB.cpp
	Library/Remote.cpp)
SOURCE_GROUP(query FILES 
	Query/Base.cpp
	Query/Factory.cpp
	Query/ListBase.cpp
	Query/ListSelection.cpp
	Query/SortTracks.cpp
	Query/SortTracksWithData.cpp
	Query/TrackMetadata.cpp)
SOURCE_GROUP(server FILES 
	server/Connection.cpp
	server/User.cpp
	server/UserSession.cpp)
SOURCE_GROUP(tracklist FILES 
	tracklist/Base.cpp
	tracklist/LibraryList.cpp
	tracklist/MultiLibraryList.cpp)
SOURCE_GROUP(xml FILES 
	xml/Node.cpp
	xml/Parser.cpp
	xml/ParserNode.cpp
	xml/Socket.cpp
	xml/Writer.cpp
	xml/WriterNode.cpp)


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

add_library( musik SHARED ${CORE_SOURCES} )
target_link_libraries( musik ${musikCube_LINK_LIBS})


