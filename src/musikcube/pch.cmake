target_precompile_headers(musikcube
  PRIVATE
    "$<$<COMPILE_LANGUAGE:CXX>:<set$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<vector$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<map$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<string$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<algorithm$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<unordered_map$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<thread$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<cmath$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<string$<ANGLE-R>>"

  PUBLIC
    "$<$<COMPILE_LANGUAGE:CXX>:cursespp/cursespp/App.h>"
    "$<$<COMPILE_LANGUAGE:CXX>:cursespp/cursespp/AppLayout.h>"
    "$<$<COMPILE_LANGUAGE:CXX>:cursespp/cursespp/Checkbox.h>"
    "$<$<COMPILE_LANGUAGE:CXX>:cursespp/cursespp/Colors.h>"
    "$<$<COMPILE_LANGUAGE:CXX>:cursespp/cursespp/curses_config.h>"
    "$<$<COMPILE_LANGUAGE:CXX>:cursespp/cursespp/DialogOverlay.h>"
    "$<$<COMPILE_LANGUAGE:CXX>:cursespp/cursespp/IDisplayable.h>"
    "$<$<COMPILE_LANGUAGE:CXX>:cursespp/cursespp/IInput.h>"
    "$<$<COMPILE_LANGUAGE:CXX>:cursespp/cursespp/IKeyHandler.h>"
    "$<$<COMPILE_LANGUAGE:CXX>:cursespp/cursespp/ILayout.h>"
    "$<$<COMPILE_LANGUAGE:CXX>:cursespp/cursespp/IMouseHandler.h>"
    "$<$<COMPILE_LANGUAGE:CXX>:cursespp/cursespp/INavigationKeys.h>"
    "$<$<COMPILE_LANGUAGE:CXX>:cursespp/cursespp/InputOverlay.h>"
    "$<$<COMPILE_LANGUAGE:CXX>:cursespp/cursespp/IOrderable.h>"
    "$<$<COMPILE_LANGUAGE:CXX>:cursespp/cursespp/IOverlay.h>"
    "$<$<COMPILE_LANGUAGE:CXX>:cursespp/cursespp/IScrollable.h>"
    "$<$<COMPILE_LANGUAGE:CXX>:cursespp/cursespp/IScrollAdapter.h>"
    "$<$<COMPILE_LANGUAGE:CXX>:cursespp/cursespp/ITopLevelLayout.h>"
    "$<$<COMPILE_LANGUAGE:CXX>:cursespp/cursespp/IWindow.h>"
    "$<$<COMPILE_LANGUAGE:CXX>:cursespp/cursespp/IWindowGroup.h>"
    "$<$<COMPILE_LANGUAGE:CXX>:cursespp/cursespp/LayoutBase.h>"
    "$<$<COMPILE_LANGUAGE:CXX>:cursespp/cursespp/ListOverlay.h>"
    "$<$<COMPILE_LANGUAGE:CXX>:cursespp/cursespp/ListWindow.h>"
    "$<$<COMPILE_LANGUAGE:CXX>:cursespp/cursespp/MultiLineEntry.h>"
    "$<$<COMPILE_LANGUAGE:CXX>:cursespp/cursespp/NumberValidator.h>"
    "$<$<COMPILE_LANGUAGE:CXX>:cursespp/cursespp/OverlayBase.h>"
    "$<$<COMPILE_LANGUAGE:CXX>:cursespp/cursespp/OverlayStack.h>"
    "$<$<COMPILE_LANGUAGE:CXX>:cursespp/cursespp/SchemaOverlay.h>"
    "$<$<COMPILE_LANGUAGE:CXX>:cursespp/cursespp/Screen.h>"
    "$<$<COMPILE_LANGUAGE:CXX>:cursespp/cursespp/ScrollableWindow.h>"
    "$<$<COMPILE_LANGUAGE:CXX>:cursespp/cursespp/ScrollAdapterBase.h>"
    "$<$<COMPILE_LANGUAGE:CXX>:cursespp/cursespp/Scrollbar.h>"
    "$<$<COMPILE_LANGUAGE:CXX>:cursespp/cursespp/ShortcutsWindow.h>"
    "$<$<COMPILE_LANGUAGE:CXX>:cursespp/cursespp/SimpleScrollAdapter.h>"
    "$<$<COMPILE_LANGUAGE:CXX>:cursespp/cursespp/SingleLineEntry.h>"
    "$<$<COMPILE_LANGUAGE:CXX>:cursespp/cursespp/Text.h>"
    "$<$<COMPILE_LANGUAGE:CXX>:cursespp/cursespp/TextInput.h>"
    "$<$<COMPILE_LANGUAGE:CXX>:cursespp/cursespp/TextLabel.h>"
    "$<$<COMPILE_LANGUAGE:CXX>:cursespp/cursespp/ToastOverlay.h>"
    "$<$<COMPILE_LANGUAGE:CXX>:cursespp/cursespp/Win32Util.h>"
    "$<$<COMPILE_LANGUAGE:CXX>:cursespp/cursespp/Window.h>"   
    "$<$<COMPILE_LANGUAGE:CXX>:app/layout/BrowseLayout.h>"
    "$<$<COMPILE_LANGUAGE:CXX>:app/layout/CategorySearchLayout.h>"
    "$<$<COMPILE_LANGUAGE:CXX>:app/layout/ConsoleLayout.h>"
    "$<$<COMPILE_LANGUAGE:CXX>:app/layout/DirectoryLayout.h>"
    "$<$<COMPILE_LANGUAGE:CXX>:app/layout/HotkeysLayout.h>"
    "$<$<COMPILE_LANGUAGE:CXX>:app/layout/LibraryLayout.h>"
    "$<$<COMPILE_LANGUAGE:CXX>:app/layout/LibraryNotConnectedLayout.h>"
    "$<$<COMPILE_LANGUAGE:CXX>:app/layout/LocalLibrarySettingsLayout.h>"
    "$<$<COMPILE_LANGUAGE:CXX>:app/layout/LyricsLayout.h>"
    "$<$<COMPILE_LANGUAGE:CXX>:app/layout/MainLayout.h>"
    "$<$<COMPILE_LANGUAGE:CXX>:app/layout/NowPlayingLayout.h>"
    "$<$<COMPILE_LANGUAGE:CXX>:app/layout/RemoteLibrarySettingsLayout.h>"
    "$<$<COMPILE_LANGUAGE:CXX>:app/layout/SettingsLayout.h>"
    "$<$<COMPILE_LANGUAGE:CXX>:app/layout/TrackSearchLayout.h>"
    "$<$<COMPILE_LANGUAGE:CXX>:app/model/DirectoryAdapter.h>"
    "$<$<COMPILE_LANGUAGE:CXX>:app/model/HotkeysAdapter.h>"
    "$<$<COMPILE_LANGUAGE:CXX>:app/overlay/BrowseOverlays.h>"
    "$<$<COMPILE_LANGUAGE:CXX>:app/overlay/ColorThemeOverlay.h>"
    "$<$<COMPILE_LANGUAGE:CXX>:app/overlay/EqualizerOverlay.h>"
    "$<$<COMPILE_LANGUAGE:CXX>:app/overlay/LastFmOverlay.h>"
    "$<$<COMPILE_LANGUAGE:CXX>:app/overlay/PlaybackOverlays.h>"
    "$<$<COMPILE_LANGUAGE:CXX>:app/overlay/PlayQueueOverlays.h>"
    "$<$<COMPILE_LANGUAGE:CXX>:app/overlay/PluginOverlay.h>"
    "$<$<COMPILE_LANGUAGE:CXX>:app/overlay/PreampOverlay.h>"
    "$<$<COMPILE_LANGUAGE:CXX>:app/overlay/ReassignHotkeyOverlay.h>"
    "$<$<COMPILE_LANGUAGE:CXX>:app/overlay/ServerOverlay.h>"
    "$<$<COMPILE_LANGUAGE:CXX>:app/overlay/SettingsOverlays.h>"
    "$<$<COMPILE_LANGUAGE:CXX>:app/overlay/TrackOverlays.h>"
    "$<$<COMPILE_LANGUAGE:CXX>:app/overlay/VisualizerOverlay.h>"
    "$<$<COMPILE_LANGUAGE:CXX>:app/util/ConsoleLogger.h>"
    "$<$<COMPILE_LANGUAGE:CXX>:app/util/GlobalHotkeys.h>"
    "$<$<COMPILE_LANGUAGE:CXX>:app/util/Hotkeys.h>"
    "$<$<COMPILE_LANGUAGE:CXX>:app/util/MagicConstants.h>"
    "$<$<COMPILE_LANGUAGE:CXX>:app/util/Messages.h>"
    "$<$<COMPILE_LANGUAGE:CXX>:app/util/Playback.h>"
    "$<$<COMPILE_LANGUAGE:CXX>:app/util/PreferenceKeys.h>"
    "$<$<COMPILE_LANGUAGE:CXX>:app/util/Rating.h>"
    "$<$<COMPILE_LANGUAGE:CXX>:app/util/TrackRowRenderers.h>"
    "$<$<COMPILE_LANGUAGE:CXX>:app/util/UpdateCheck.h>"
    "$<$<COMPILE_LANGUAGE:CXX>:app/window/CategoryListView.h>"
    "$<$<COMPILE_LANGUAGE:CXX>:app/window/TrackListView.h>"
    "$<$<COMPILE_LANGUAGE:CXX>:app/window/TransportWindow.h>"
)
