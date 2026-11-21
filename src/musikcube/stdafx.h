//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004-2023 musikcube team
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//    * Redistributions of source code must retain the above copyright notice,
//      this list of conditions and the following disclaimer.
//
//    * Redistributions in binary form must reproduce the above copyright
//      notice, this list of conditions and the following disclaimer in the
//      documentation and/or other materials provided with the distribution.
//
//    * Neither the name of the author nor the names of other contributors may
//      be used to endorse or promote products derived from this software
//      without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
//////////////////////////////////////////////////////////////////////////////

#pragma once

#ifdef WIN32
    #ifndef WINVER
        #define WINVER 0x0601
    #endif

    #ifndef _WIN32_WINNT
        #define _WIN32_WINNT 0x0601
    #endif

    #define WIN32_LEAN_AND_MEAN
    #define NOMINMAX

    #include <windows.h>
#endif

/* curses defines a timeout() method that causes issues with
asio's streambuf. include it up front so work around this */
#pragma warning(push, 0)
#include <asio/basic_socket_streambuf.hpp>
#pragma warning(pop)

#include <set>
#include <vector>
#include <map>
#include <string>
#include <algorithm>
#include <unordered_map>
#include <thread>
#include <filesystem>

#include <cmath>
#include <string>

#pragma warning(push, 0)
#include <nlohmann/json.hpp>
#include <sigslot/sigslot.h>
#pragma warning(pop)

#include <musikcore/config.h>
#include <app/util/Locale.h>
#include <musikcore/audio/Buffer.h>
#include <musikcore/audio/Crossfader.h>
#include <musikcore/audio/CrossfadeTransport.h>
#include <musikcore/audio/GaplessTransport.h>
#include <musikcore/audio/IStream.h>
#include <musikcore/audio/Player.h>
#include <musikcore/audio/Stream.h>
#include <musikcore/audio/Streams.h>
#include <musikcore/db/Connection.h>
#include <musikcore/db/ScopedTransaction.h>
#include <musikcore/db/SqliteExtensions.h>
#include <musikcore/db/Statement.h>
#include <musikcore/debug.h>
#include <musikcore/io/DataStreamFactory.h>
#include <musikcore/io/LocalFileStream.h>
#include <musikcore/net/RawWebSocketClient.h>
#include <musikcore/net/WebSocketClient.h>
#include <musikcore/plugin/Plugins.h>
#include <common/runtime/IMessage.h>
#include <common/runtime/IMessageQueue.h>
#include <common/runtime/IMessageTarget.h>
#include <common/runtime/Message.h>
#include <common/runtime/MessageQueue.h>
#include <musikcore/sdk/constants.h>
#include <musikcore/sdk/DataBuffer.h>
#include <musikcore/sdk/Filesystem.h>
#include <musikcore/sdk/IAllocator.h>
#include <musikcore/sdk/IAnalyzer.h>
#include <musikcore/sdk/IBlockingEncoder.h>
#include <musikcore/sdk/IBuffer.h>
#include <musikcore/sdk/IBufferProvider.h>
#include <musikcore/sdk/IDataStream.h>
#include <musikcore/sdk/IDataStreamFactory.h>
#include <musikcore/sdk/IDebug.h>
#include <musikcore/sdk/IDecoder.h>
#include <musikcore/sdk/IDecoderFactory.h>
#include <musikcore/sdk/IDSP.h>
#include <musikcore/sdk/IEncoder.h>
#include <musikcore/sdk/IEncoderFactory.h>
#include <musikcore/sdk/IEnvironment.h>
#include <musikcore/sdk/IIndexerNotifier.h>
#include <musikcore/sdk/IIndexerSource.h>
#include <musikcore/sdk/IIndexerWriter.h>
#include <musikcore/sdk/IMap.h>
#include <musikcore/sdk/IMapList.h>
#include <musikcore/sdk/IMetadataProxy.h>
#include <musikcore/sdk/IPcmVisualizer.h>
#include <musikcore/sdk/IPlaybackRemote.h>
#include <musikcore/sdk/IResource.h>
#include <musikcore/sdk/ISpectrumVisualizer.h>
#include <musikcore/sdk/IStreamingEncoder.h>
#include <musikcore/sdk/ITagReader.h>
#include <musikcore/sdk/ITagStore.h>
#include <musikcore/sdk/ITrack.h>
#include <musikcore/sdk/ITrackList.h>
#include <musikcore/sdk/ITrackListEditor.h>
#include <musikcore/sdk/IValue.h>
#include <musikcore/sdk/IValueList.h>
#include <musikcore/sdk/IVisualizer.h>
#include <musikcore/sdk/ReplayGain.h>
#include <musikcore/sdk/String.h>
#include <musikcore/support/Auddio.h>
#include <musikcore/support/Common.h>
#include <musikcore/support/DeleteDefaults.h>
#include <musikcore/support/Duration.h>
#include <musikcore/support/LastFm.h>
#include <musikcore/support/NarrowCast.h>
#include <musikcore/support/Playback.h>
#include <musikcore/utfutil.h>
#include <musikcore/sdk/version.h>

#include <cursespp/cursespp/App.h>
#include <cursespp/cursespp/AppLayout.h>
#include <cursespp/cursespp/Checkbox.h>
#include <cursespp/cursespp/Colors.h>
#include <cursespp/cursespp/curses_config.h>
#include <cursespp/cursespp/DialogOverlay.h>
#include <cursespp/cursespp/IDisplayable.h>
#include <cursespp/cursespp/IInput.h>
#include <cursespp/cursespp/IKeyHandler.h>
#include <cursespp/cursespp/ILayout.h>
#include <cursespp/cursespp/IMouseHandler.h>
#include <cursespp/cursespp/INavigationKeys.h>
#include <cursespp/cursespp/InputOverlay.h>
#include <cursespp/cursespp/IOrderable.h>
#include <cursespp/cursespp/IOverlay.h>
#include <cursespp/cursespp/IScrollable.h>
#include <cursespp/cursespp/IScrollAdapter.h>
#include <cursespp/cursespp/ITopLevelLayout.h>
#include <cursespp/cursespp/IWindow.h>
#include <cursespp/cursespp/IWindowGroup.h>
#include <cursespp/cursespp/LayoutBase.h>
#include <cursespp/cursespp/ListOverlay.h>
#include <cursespp/cursespp/ListWindow.h>
#include <cursespp/cursespp/MultiLineEntry.h>
#include <cursespp/cursespp/NumberValidator.h>
#include <cursespp/cursespp/OverlayBase.h>
#include <cursespp/cursespp/OverlayStack.h>
#include <cursespp/cursespp/SchemaOverlay.h>
#include <cursespp/cursespp/Screen.h>
#include <cursespp/cursespp/ScrollableWindow.h>
#include <cursespp/cursespp/ScrollAdapterBase.h>
#include <cursespp/cursespp/Scrollbar.h>
#include <cursespp/cursespp/ShortcutsWindow.h>
#include <cursespp/cursespp/SimpleScrollAdapter.h>
#include <cursespp/cursespp/SingleLineEntry.h>
#include <cursespp/cursespp/Text.h>
#include <cursespp/cursespp/TextInput.h>
#include <cursespp/cursespp/TextLabel.h>
#include <cursespp/cursespp/ToastOverlay.h>
#include <cursespp/cursespp/Win32Util.h>
#include <cursespp/cursespp/Window.h>

#include <app/layout/BrowseLayout.h>
#include <app/layout/CategorySearchLayout.h>
#include <app/layout/ConsoleLayout.h>
#include <app/layout/DirectoryLayout.h>
#include <app/layout/HotkeysLayout.h>
#include <app/layout/LibraryLayout.h>
#include <app/layout/LibraryNotConnectedLayout.h>
#include <app/layout/LocalLibrarySettingsLayout.h>
#include <app/layout/LyricsLayout.h>
#include <app/layout/MainLayout.h>
#include <app/layout/NowPlayingLayout.h>
#include <app/layout/RemoteLibrarySettingsLayout.h>
#include <app/layout/SettingsLayout.h>
#include <app/layout/TrackSearchLayout.h>
#include <app/model/DirectoryAdapter.h>
#include <app/model/HotkeysAdapter.h>
#include <app/overlay/BrowseOverlays.h>
#include <app/overlay/ColorThemeOverlay.h>
#include <app/overlay/EqualizerOverlay.h>
#include <app/overlay/LastFmOverlay.h>
#include <app/overlay/PlaybackOverlays.h>
#include <app/overlay/PlayQueueOverlays.h>
#include <app/overlay/PluginOverlay.h>
#include <app/overlay/PreampOverlay.h>
#include <app/overlay/ReassignHotkeyOverlay.h>
#include <app/overlay/ServerOverlay.h>
#include <app/overlay/SettingsOverlays.h>
#include <app/overlay/TrackOverlays.h>
#include <app/overlay/VisualizerOverlay.h>
#include <app/util/ConsoleLogger.h>
#include <app/util/GlobalHotkeys.h>
#include <app/util/Hotkeys.h>
#include <app/util/MagicConstants.h>
#include <app/util/Messages.h>
#include <app/util/PreferenceKeys.h>
#include <app/util/Rating.h>
#include <app/util/TrackRowRenderers.h>
#include <app/util/UpdateCheck.h>
#include <app/window/CategoryListView.h>
#include <app/window/TrackListView.h>
#include <app/window/TransportWindow.h>
