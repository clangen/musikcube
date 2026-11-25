#include <musikcore/preferences.h>

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

#include "pch.hpp"
#include "PreferenceKeys.h"

namespace musik { namespace core { namespace prefs {

    const std::string components::Settings = "settings";
    const std::string components::Libraries = "libraries";
    const std::string components::Playback = "playback";
    const std::string components::Plugins = "plugins";
    const std::string components::Session = "session";

    const std::string keys::AutoSyncIntervalMillis = "AutoSyncIntervalMillis";
    const std::string keys::RemoveMissingFiles = MC_PREFERENCE_KEY_REMOVE_MISSING_FILES;
    const std::string keys::SyncOnStartup = MC_PREFERENCE_KEY_SYNC_ON_STARTUP;
    const std::string keys::ResumePlaybackOnStartup = MC_PREFERENCE_KEY_RESUME_PLAYBACK_ON_STARTUP;
    const std::string keys::Volume = "Volume";
    const std::string keys::RepeatMode = "RepeatMode";
    const std::string keys::TimeChangeMode = "TimeChangeMode";
    const std::string keys::OutputPlugin = "OutputPlugin";
    const std::string keys::Transport = "Transport";
    const std::string keys::Locale = "Locale";
    const std::string keys::IndexerLogEnabled = MC_PREFERENCE_KEY_INDEXER_LOG_ENABLED;
    const std::string keys::IndexerThreadCount = MC_PREFERENCE_KEY_INDEXER_THREAD_COUNT;
    const std::string keys::IndexerTransactionInterval = MC_PREFERENCE_KEY_INDEXER_TRANSACTION_INTERVAL;
    const std::string keys::ReplayGainMode = "ReplayGainMode";
    const std::string keys::PreampDecibels = "PreampDecibels";
    const std::string keys::SaveSessionOnExit = MC_PREFERENCE_KEY_SAVE_SESSION_ON_EXIT;
    const std::string keys::LastPlayQueueIndex = "LastPlayQueueIndex";
    const std::string keys::LastPlayQueueTime = "LastPlayQueueTime";
    const std::string keys::LastFmToken = "LastFmToken";
    const std::string keys::LastFmSessionId = "LastFmSessionId";
    const std::string keys::LastFmUsername = "LastFmUsername";
    const std::string keys::DisableAlbumArtistFallback = "DisableAlbumArtistFallback";
    const std::string keys::AuddioApiToken = MC_PREFERENCE_KEY_AUDDIO_API_TOKEN;
    const std::string keys::LibraryType = "LibraryType";
    const std::string keys::PlaybackTrackQueryTimeoutMs = MC_PREFERENCE_KEY_PLAYBACK_TRACK_QUERY_TIMEOUT_MS;
    const std::string keys::RemoteLibraryHostname = MC_PREFERENCE_KEY_REMOTE_LIBRARY_HOSTNAME;
    const std::string keys::RemoteLibraryWssPort = MC_PREFERENCE_KEY_REMOTE_LIBRARY_WSS_PORT;
    const std::string keys::RemoteLibraryHttpPort = MC_PREFERENCE_KEY_REMOTE_LIBRARY_HTTP_PORT;
    const std::string keys::RemoteLibraryPassword = MC_PREFERENCE_KEY_REMOTE_LIBRARY_PASSWORD;
    const std::string keys::RemoteLibraryViewed = MC_PREFERENCE_KEY_REMOTE_LIBRARY_VIEWED;
    const std::string keys::RemoteLibraryLatencyTimeoutMs = MC_PREFERENCE_KEY_REMOTE_LIBRARY_LATENCY_TIMEOUT_MS;
    const std::string keys::RemoteLibraryWssTls = MC_PREFERENCE_KEY_REMOTE_LIBRARY_WSS_TLS;
    const std::string keys::RemoteLibraryHttpTls = MC_PREFERENCE_KEY_REMOTE_LIBRARY_HTTP_TLS;
    const std::string keys::RemoteLibraryHttpTlsAllowUntrustedCertificate =
        MC_PREFERENCE_KEY_REMOTE_LIBRARY_HTTP_TLS_ALLOW_UNTRUSTED_CERTIFICATE;
    const std::string keys::RemoteLibraryTlsWarningSuppressed = MC_PREFERENCE_KEY_REMOTE_LIBRARY_TLS_WARNING_SUPPRESSED;
    const std::string keys::RemoteLibraryTranscoderEnabled = MC_PREFERENCE_KEY_REMOTE_LIBRARY_TRANSCODER_ENABLED;
    const std::string keys::RemoteLibraryTranscoderFormat = MC_PREFERENCE_KEY_REMOTE_LIBRARY_TRANSCODER_FORMAT;
    const std::string keys::RemoteLibraryTranscoderBitrate = MC_PREFERENCE_KEY_REMOTE_LIBRARY_TRANSCODER_BITRATE;
    const std::string keys::RemoteLibraryIgnoreVersionMismatch = MC_PREFERENCE_KEY_REMOTE_LIBRARY_IGNORE_VERSION_MISMATCH;
    const std::string keys::AsyncTrackListQueries = MC_PREFERENCE_KEY_ASYNC_TRACK_LIST_QUERIES;

} } }

