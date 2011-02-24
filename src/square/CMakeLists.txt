set (SQUARE_SRCS
	stdafx.cpp
	player.cpp
	DummyAudioEventHandler.cpp
	ConsoleUI.cpp
)

set (SQUARE_HEADERS
	config.h
	ConsoleUI.h
	DummyAudioEventHandler.h
	memtrace.h
	stdafx.h
)

SOURCE_GROUP ("Source Files" FILES ${SQUARE_SRCS})
SOURCE_GROUP ("Header Files" FILES ${SQUARE_HEADERS})

add_executable(square ${SQUARE_SRCS} ${SQUARE_HEADERS})
target_link_libraries(square ${musikCube_LINK_LIBS} musik)