cmake_minimum_required(VERSION 2.6)
project( core )

SET (CMAKE_BUILD_TYPE DEBUG)

set(SOURCES 
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
  Server.cpp
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
)

set (WINDOWS_EXTRAS

)

if(CMAKE_SYSTEM_NAME MATCHES "Windows")
  add_definitions(-DWIN32)
	if(NOT DEFINED MINGW)
        	SET(SOURCES ${SOURCES} ${WINDOWS_EXTRAS})
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

add_library( core SHARED ${SOURCES} )

