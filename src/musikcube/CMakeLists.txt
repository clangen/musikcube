set (CUBE_SRCS
    ./Main.cpp
    ./stdafx.cpp
    ./app/layout/BrowseLayout.cpp
    ./app/layout/CategorySearchLayout.cpp
    ./app/layout/ConsoleLayout.cpp
    ./app/layout/DirectoryLayout.cpp
    ./app/layout/HotkeysLayout.cpp
    ./app/layout/LibraryLayout.cpp
    ./app/layout/LibraryNotConnectedLayout.cpp
    ./app/layout/LocalLibrarySettingsLayout.cpp
    ./app/layout/LyricsLayout.cpp
    ./app/layout/MainLayout.cpp
    ./app/layout/NowPlayingLayout.cpp
    ./app/layout/RemoteLibrarySettingsLayout.cpp
    ./app/layout/SettingsLayout.cpp
    ./app/layout/TrackSearchLayout.cpp
    ./app/model/DirectoryAdapter.cpp
    ./app/model/HotkeysAdapter.cpp
    ./app/overlay/BrowseOverlays.cpp
    ./app/overlay/ColorThemeOverlay.cpp
    ./app/overlay/EqualizerOverlay.cpp
    ./app/overlay/LastFmOverlay.cpp
    ./app/overlay/PlaybackOverlays.cpp
    ./app/overlay/PlayQueueOverlays.cpp
    ./app/overlay/PluginOverlay.cpp
    ./app/overlay/PreampOverlay.cpp
    ./app/overlay/ReassignHotkeyOverlay.cpp
    ./app/overlay/ServerOverlay.cpp
    ./app/overlay/SettingsOverlays.cpp
    ./app/overlay/TrackOverlays.cpp
    ./app/overlay/VisualizerOverlay.cpp
    ./app/util/ConsoleLogger.cpp
    ./app/util/GlobalHotkeys.cpp
    ./app/util/Hotkeys.cpp
    ./app/util/MagicConstants.cpp
    ./app/util/PreferenceKeys.cpp
    ./app/util/Playback.cpp
    ./app/util/Rating.cpp
    ./app/util/TrackRowRenderers.cpp
    ./app/util/UpdateCheck.cpp
    ./app/window/CategoryListView.cpp
    ./app/window/TrackListView.cpp
    ./app/window/TransportWindow.cpp
    ./cursespp/App.cpp
    ./cursespp/AppLayout.cpp
    ./cursespp/Checkbox.cpp
    ./cursespp/Colors.cpp
    ./cursespp/DialogOverlay.cpp
    ./cursespp/InputOverlay.cpp
    ./cursespp/IMouseHandler.cpp
    ./cursespp/LayoutBase.cpp
    ./cursespp/ListOverlay.cpp
    ./cursespp/ListWindow.cpp
    ./cursespp/MultiLineEntry.cpp
    ./cursespp/OverlayStack.cpp
    ./cursespp/SchemaOverlay.cpp
    ./cursespp/Screen.cpp
    ./cursespp/ScrollableWindow.cpp
    ./cursespp/ScrollAdapterBase.cpp
    ./cursespp/Scrollbar.cpp
    ./cursespp/ShortcutsWindow.cpp
    ./cursespp/SimpleScrollAdapter.cpp
    ./cursespp/SingleLineEntry.cpp
    ./cursespp/Text.cpp
    ./cursespp/TextInput.cpp
    ./cursespp/TextLabel.cpp
    ./cursespp/ToastOverlay.cpp
    ./cursespp/Window.cpp)

set(musikcube_INSTALL_DIR ${HOMEBREW_PREFIX})
if (NOT DEFINED musikcube_INSTALL_DIR)
    set(musikcube_INSTALL_DIR ${CMAKE_INSTALL_PREFIX})
endif()

configure_file("musikcube.in" "musikcube" @ONLY)

add_executable(musikcube ${CUBE_SRCS})
add_definitions(-DNCURSES_WIDECHAR)

target_include_directories(musikcube BEFORE PRIVATE ${VENDOR_INCLUDE_DIRECTORIES})

# figure out if we have a "w" suffix or not...
if (${NCURSES_DISABLE_LIB_SUFFIXES} MATCHES "true")
    message(STATUS "[ncurses] using library names *WITHOUT* 'w' prefix")
    set(CURSES_LIBRARY_NAME ncurses)
    set(PANEL_LIBRARY_NAME panel)
else()
    message(STATUS "[ncurses] using library names with 'w' prefix")
    set(CURSES_LIBRARY_NAME ncursesw)
    set(PANEL_LIBRARY_NAME panelw)
endif()

find_library(LIBNCURSES NAMES ${CURSES_LIBRARY_NAME} PATHS ${VENDOR_LINK_DIRECTORIES})
find_library(LIBPANEL NAMES ${PANEL_LIBRARY_NAME} PATHS ${VENDOR_LINK_DIRECTORIES})

if (NOT CMAKE_SYSTEM_NAME MATCHES "Darwin")
    find_library(LIBTINFO NAMES tinfo)
endif()

message(STATUS "[musikcube] using libncurses at: " ${LIBNCURSES})
message(STATUS "[musikcube] using libpanel at: " ${LIBPANEL})
message(STATUS "[musikcube] using libpanel at: " ${LIBTINFO})
target_link_libraries(musikcube ${musikcube_LINK_LIBS} ${LIBNCURSES} ${LIBPANEL} ${LIBTINFO} musikcore)

if (ENABLE_PCH MATCHES "true")
    message(STATUS "[musikcube] enabling precompiled headers")
    include(./pch.cmake)
endif()

