set (SQUARE_SRCS
	stdafx.cpp
	player.cpp
	DummyAudioEventHandler.cpp
	ConsoleUI.cpp
)

add_executable(square ${SQUARE_SRCS})
target_link_libraries(square ${musikCube_LINK_LIBS} musik)