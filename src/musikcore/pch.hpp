//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004-2020 musikcube team
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

#include <set>
#include <vector>
#include <map>
#include <string>
#include <algorithm>
#include <unordered_map>
#include <thread>

#include <cmath>
#include <string>

#pragma warning(push, 0)
#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/config/asio_client.hpp>
#include <websocketpp/client.hpp>
#include <boost/filesystem.hpp>
#include <boost/algorithm/string/case_conv.hpp>
#include <boost/algorithm/string.hpp>
#include <nlohmann/json.hpp>
#include <sigslot/sigslot.h>
#pragma warning(pop)

#include <musikcore/config.h>
#include <musikcore/i18n/Locale.h>
#include <musikcore/audio/Buffer.h>
#include <musikcore/audio/Crossfader.h>
#include <musikcore/audio/CrossfadeTransport.h>
#include <musikcore/audio/GaplessTransport.h>
#include <musikcore/audio/IStream.h>
#include <musikcore/audio/ITransport.h>
#include <musikcore/audio/MasterTransport.h>
#include <musikcore/audio/Outputs.h>
#include <musikcore/audio/PlaybackService.h>
#include <musikcore/audio/Player.h>
#include <musikcore/audio/Stream.h>
#include <musikcore/audio/Streams.h>
#include <musikcore/audio/Visualizer.h>
#include <musikcore/db/Connection.h>
#include <musikcore/db/ScopedTransaction.h>
#include <musikcore/db/SqliteExtensions.h>
#include <musikcore/db/Statement.h>
#include <musikcore/debug.h>
#include <musikcore/io/DataStreamFactory.h>
#include <musikcore/io/LocalFileStream.h>
#include <musikcore/library/IIndexer.h>
#include <musikcore/library/ILibrary.h>
#include <musikcore/library/Indexer.h>
#include <musikcore/library/IQuery.h>
#include <musikcore/library/LibraryFactory.h>
#include <musikcore/library/LocalLibrary.h>
#include <musikcore/library/LocalLibraryConstants.h>
#include <musikcore/library/LocalMetadataProxy.h>
#include <musikcore/library/MasterLibrary.h>
#include <musikcore/library/metadata/MetadataMap.h>
#include <musikcore/library/metadata/MetadataMapList.h>
#include <musikcore/library/query/AlbumListQuery.h>
#include <musikcore/library/query/AllCategoriesQuery.h>
#include <musikcore/library/query/AppendPlaylistQuery.h>
#include <musikcore/library/query/CategoryListQuery.h>
#include <musikcore/library/query/CategoryTrackListQuery.h>
#include <musikcore/library/query/DeletePlaylistQuery.h>
#include <musikcore/library/query/DirectoryTrackListQuery.h>
#include <musikcore/library/query/GetPlaylistQuery.h>
#include <musikcore/library/query/LyricsQuery.h>
#include <musikcore/library/query/MarkTrackPlayedQuery.h>
#include <musikcore/library/query/NowPlayingTrackListQuery.h>
#include <musikcore/library/query/PersistedPlayQueueQuery.h>
#include <musikcore/library/query/SavePlaylistQuery.h>
#include <musikcore/library/query/SearchTrackListQuery.h>
#include <musikcore/library/query/SetTrackRatingQuery.h>
#include <musikcore/library/query/TrackListQueryBase.h>
#include <musikcore/library/query/TrackMetadataBatchQuery.h>
#include <musikcore/library/query/TrackMetadataQuery.h>
#include <musikcore/library/query/util/CategoryQueryUtil.h>
#include <musikcore/library/query/util/SdkWrappers.h>
#include <musikcore/library/query/util/Serialization.h>
#include <musikcore/library/query/util/TrackQueryFragments.h>
#include <musikcore/library/query/util/TrackSort.h>
#include <musikcore/library/QueryBase.h>
#include <musikcore/library/QueryRegistry.h>
#include <musikcore/library/RemoteLibrary.h>
#include <musikcore/library/track/IndexerTrack.h>
#include <musikcore/library/track/LibraryTrack.h>
#include <musikcore/library/track/Track.h>
#include <musikcore/library/track/TrackList.h>
#include <musikcore/net/RawWebSocketClient.h>
#include <musikcore/net/WebSocketClient.h>
#include <musikcore/plugin/PluginFactory.h>
#include <musikcore/plugin/Plugins.h>
#include <musikcore/runtime/IMessage.h>
#include <musikcore/runtime/IMessageQueue.h>
#include <musikcore/runtime/IMessageTarget.h>
#include <musikcore/runtime/Message.h>
#include <musikcore/runtime/MessageQueue.h>
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
#include <musikcore/sdk/IDevice.h>
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
#include <musikcore/sdk/IOutput.h>
#include <musikcore/sdk/IPcmVisualizer.h>
#include <musikcore/sdk/IPlaybackRemote.h>
#include <musikcore/sdk/IPlaybackService.h>
#include <musikcore/sdk/IPlugin.h>
#include <musikcore/sdk/IPreferences.h>
#include <musikcore/sdk/IResource.h>
#include <musikcore/sdk/ISchema.h>
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
#include <musikcore/support/Messages.h>
#include <musikcore/support/NarrowCast.h>
#include <musikcore/support/Playback.h>
#include <musikcore/support/PreferenceKeys.h>
#include <musikcore/support/Preferences.h>
#include <musikcore/utfutil.h>
#include <musikcore/version.h>