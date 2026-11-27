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

#ifdef _MSC_VER
#pragma warning(push, 0)
#endif
#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/client.hpp>
#include <nlohmann/json.hpp>
#ifdef _MSC_VER
#pragma warning(pop)
#endif

#include <musikcore/config.h>
#include <musikcore/i18n/Locale.h>
#include <musikcore/audio/Buffer.h>
#include <musikcore/audio/Crossfader.h>
#include <musikcore/audio/CrossfadeTransport.h>
#include <musikcore/audio/GaplessTransport.h>
#include <musikcore/audio/ITransport.h>
#include <musikcore/audio/MasterTransport.h>
#include <musikcore/audio/Outputs.h>
#include <musikcore/audio/PlaybackService.h>
#include <musikcore/audio/Player.h>
#include <musikcore/audio/Stream.h>
#include <musikcore/audio/Dsp.h>
#include <musikcore/audio/Visualizer.h>
#include <musikcore/db/Connection.h>
#include <musikcore/db/ScopedTransaction.h>
#include <musikcore/db/SqliteExtensions.h>
#include <musikcore/db/Statement.h>
#include <musikcore/debug.h>
#include <musikcore/library/LibraryBackend.h>
#include <musikcore/library/Indexer.h>
#include <musikcore/library/IQuery.h>
#include <musikcore/library/LibraryBackends.h>
#include <musikcore/library/LocalLibraryBackend.h>
#include <musikcore/library/LocalLibraryConstants.h>
#include <musikcore/plugin/PluginLibraryService.h>
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
#include <musikcore/library/query/util/Serialization.h>
#include <musikcore/library/query/util/TrackQueryFragments.h>
#include <musikcore/library/query/util/TrackSort.h>
#include <musikcore/library/QueryBase.h>
#include <musikcore/library/QueryRegistry.h>
#include <musikcore/library/RemoteLibraryBackend.h>
#include <musikcore/library/track/IndexerTrack.h>
#include <musikcore/library/track/LibraryTrack.h>
#include <musikcore/library/track/Track.h>
#include <musikcore/library/track/TrackList.h>
#include <musikcore/net/RawWebSocketClient.h>
#include <musikcore/net/WebSocketClient.h>
#include <musikcore/plugin/Plugins.h>
#include <common/runtime/IMessage.h>
#include <common/runtime/IMessageQueue.h>
#include <common/runtime/IMessageTarget.h>
#include <common/runtime/Message.h>
#include <common/runtime/MessageQueue.h>
#include <musikcore/audio/PlaybackTypes.h>
#include <musikcore/library/IndexingTypes.h>
#include <common/Filesystem.h>
#include <musikcore/plugin/metadata.h>
#include <common/String.h>
#include <musikcore/support/Auddio.h>
#include <musikcore/support/Common.h>
#include <common/DeleteDefaults.h>
#include <musikcore/support/LastFm.h>
#include <musikcore/support/Messages.h>
#include <common/NarrowCast.h>
#include <musikcore/support/Playback.h>
#include <musikcore/support/SettingKeys.h>
#include <musikcore/support/Settings.h>
#include <common/Utf8.h>
#include <common/version.h>
