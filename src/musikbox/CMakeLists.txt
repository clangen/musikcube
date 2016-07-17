set (BOX_SRCS
	./Main.cpp
	./stdafx.cpp
	./app/layout/BrowseLayout.cpp
	./app/layout/ConsoleLayout.cpp
	./app/layout/SettingsLayout.cpp
	./app/layout/LibraryLayout.cpp
	./app/layout/MainLayout.cpp
	./app/layout/NowPlayingLayout.cpp
	./app/layout/SearchLayout.cpp
	./app/layout/TrackSearchLayout.cpp
	./app/model/DirectoryAdapter.cpp
	./app/model/TrackList.cpp
	./app/query/CategoryListViewQuery.cpp
	./app/query/CategoryTrackListQuery.cpp
	./app/query/NowPlayingTrackListQuery.cpp
	./app/query/SearchTrackListQuery.cpp
	./app/service/PlaybackService.cpp
	./app/util/Duration.cpp
	./app/util/Hotkeys.cpp
	./app/util/GlobalHotkeys.cpp
	./app/util/Playback.cpp
	./app/util/SystemInfo.cpp
	./app/window/CategoryListView.cpp
	./app/window/EntryWithHeader.cpp
	./app/window/LogWindow.cpp
	./app/window/OutputWindow.cpp
	./app/window/ResourcesWindow.cpp
	./app/window/ShortcutsWindow.cpp
	./app/window/TrackListView.cpp
	./app/window/TransportWindow.cpp
	./cursespp/App.cpp
	./cursespp/Checkbox.cpp
	./cursespp/Colors.cpp
	./cursespp/LayoutBase.cpp
	./cursespp/LayoutStack.cpp
	./cursespp/ListWindow.cpp
	./cursespp/Message.cpp
	./cursespp/MessageQueue.cpp
	./cursespp/MultiLineEntry.cpp
	./cursespp/Screen.cpp
	./cursespp/ScrollableWindow.cpp
	./cursespp/ScrollAdapterBase.cpp
	./cursespp/SimpleScrollAdapter.cpp
	./cursespp/SingleLineEntry.cpp
	./cursespp/Text.cpp
	./cursespp/TextInput.cpp
	./cursespp/TextLabel.cpp
	./cursespp/Window.cpp
	./cursespp/WindowLayout.cpp
)

add_executable(musikbox ${BOX_SRCS})

if (CMAKE_SYSTEM_NAME MATCHES "Linux")
	target_link_libraries(musikbox ${musikbox_LINK_LIBS} ncursesw panelw musikcore)
else (CMAKE_SYSTEM_NAME MATCHES "Linux")
	target_link_libraries(musikbox ${musikbox_LINK_LIBS} curses panel musikcore)
endif (CMAKE_SYSTEM_NAME MATCHES "Linux")
