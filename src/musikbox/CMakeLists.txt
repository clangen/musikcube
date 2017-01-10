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
	./app/overlay/PlaybackOverlays.cpp
	./app/overlay/VisualizerOverlay.cpp
	./app/util/Hotkeys.cpp
	./app/util/GlobalHotkeys.cpp
	./app/util/PreferenceKeys.cpp
	./app/util/Playback.cpp
	./app/window/CategoryListView.cpp
	./app/window/EntryWithHeader.cpp
	./app/window/LogWindow.cpp
	./app/window/TrackListView.cpp
	./app/window/TransportWindow.cpp
	./cursespp/App.cpp
	./cursespp/Checkbox.cpp
	./cursespp/Colors.cpp
	./cursespp/DialogOverlay.cpp
	./cursespp/LayoutBase.cpp
	./cursespp/ListOverlay.cpp
	./cursespp/ListWindow.cpp
	./cursespp/MultiLineEntry.cpp
	./cursespp/OverlayStack.cpp
	./cursespp/ShortcutsWindow.cpp
	./cursespp/Screen.cpp
	./cursespp/ScrollableWindow.cpp
	./cursespp/ScrollAdapterBase.cpp
	./cursespp/SimpleScrollAdapter.cpp
	./cursespp/SingleLineEntry.cpp
	./cursespp/Text.cpp
	./cursespp/TextInput.cpp
	./cursespp/TextLabel.cpp
	./cursespp/Window.cpp
)

set(musikbox_INSTALL_DIR ${HOMEBREW_PREFIX})

if (NOT DEFINED musikbox_INSTALL_DIR)
	set(musikbox_INSTALL_DIR ${CMAKE_INSTALL_PREFIX})
endif()

configure_file("musikbox.in" "musikbox" @ONLY)

add_executable(musikbox ${BOX_SRCS})

if (CMAKE_SYSTEM_NAME MATCHES "Linux")
	target_link_libraries(musikbox ${musikbox_LINK_LIBS} ncursesw panelw musikcore musikglue)
else (CMAKE_SYSTEM_NAME MATCHES "Linux")
	target_link_libraries(musikbox ${musikbox_LINK_LIBS} curses panel musikcore musikglue)
endif (CMAKE_SYSTEM_NAME MATCHES "Linux")
