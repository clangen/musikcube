set(CMAKE_INCLUDE_CURRENT_DIR on)

add_subdirectory(Library)
add_subdirectory(Query)
add_subdirectory(audio)
add_subdirectory(db)
add_subdirectory(filestreams)
add_subdirectory(http)
add_subdirectory(server)
add_subdirectory(tracklist)
add_subdirectory(xml)

set(Library_SRCS
	Library/Base.cpp
	Library/LocalDB.cpp
	Library/Remote.cpp
)

set(Query_SRCS
	Query/Base.cpp
	Query/Factory.cpp
	Query/ListBase.cpp
	Query/ListSelection.cpp
	#Query/PlaylistLoad.cpp
	#Query/PlaylistSave.cpp
	#Query/Playlists.cpp
	Query/SortTracks.cpp
	Query/TrackMetadata.cpp
)

set(audio_SRCS
	audio/AudioPacketizer.cpp
	audio/AudioStream.cpp
	audio/Transport.cpp
)

set(db_SRCS
	db/CachedStatement.cpp
	db/Connection.cpp
	db/ScopedTransaction.cpp
	db/Statement.cpp
)

set(filestreams_SRCS
	filestreams/Factory.cpp
	filestreams/LocalFileStream.cpp
)

set(http_SRCS
	http/RequestParser.cpp
	http/Responder.cpp
	http/Server.cpp
	http/TrackSender.cpp
)

set(server_SRCS
	server/Connection.cpp
	server/User.cpp
	server/UserSession.cpp
)

set(tracklist_SRCS
	tracklist/Playlist.cpp
	tracklist/Standard.cpp
)

set(xml_SRCS
	xml/Node.cpp
	xml/Parser.cpp
	xml/ParserNode.cpp
	xml/Writer.cpp
	xml/WriterNode.cpp
)

set(core_SRCS
	${Library_SRCS}
	${Query_SRCS}
	${audio_SRCS}
	${db_SRCS}
	${filestreams_SRCS}
	${http_SRCS}
	${server_SRCS}
	${tracklist_SRCS}
	${xml_SRCS}
	Common.cpp
	Crypt.cpp
	Indexer.cpp
	LibraryFactory.cpp
	MessageQueue.cpp
	MetaKey.cpp
	MetadataValue.cpp
	PlaybackQueue.cpp
	PluginFactory.cpp
	Preferences.cpp
	Server.cpp
	ThreadHelper.cpp
	Track.cpp
	TrackMeta.cpp
	#Version.cpp
	pch.cpp
)

add_library(core SHARED ${core_SRCS})

