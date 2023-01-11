//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004-2021 musikcube team
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

#include <string>

namespace musik { namespace core { namespace prefs {

    namespace components {
        EXPORT extern const std::string Settings;
        EXPORT extern const std::string Libraries;
        EXPORT extern const std::string Playback;
        EXPORT extern const std::string Plugins;
        EXPORT extern const std::string Session;
    }

    namespace keys {
        EXPORT extern const std::string AutoSyncIntervalMillis;
        EXPORT extern const std::string RemoveMissingFiles;
        EXPORT extern const std::string SyncOnStartup;
        EXPORT extern const std::string ResumePlaybackOnStartup;
        EXPORT extern const std::string Volume;
        EXPORT extern const std::string RepeatMode;
        EXPORT extern const std::string TimeChangeMode;
        EXPORT extern const std::string OutputPlugin;
        EXPORT extern const std::string Transport;
        EXPORT extern const std::string Locale;
        EXPORT extern const std::string IndexerLogEnabled;
        EXPORT extern const std::string IndexerThreadCount;
        EXPORT extern const std::string IndexerTransactionInterval;
        EXPORT extern const std::string ReplayGainMode;
        EXPORT extern const std::string PreampDecibels;
        EXPORT extern const std::string SaveSessionOnExit;
        EXPORT extern const std::string LastPlayQueueIndex;
        EXPORT extern const std::string LastPlayQueueTime;
        EXPORT extern const std::string LastFmToken;
        EXPORT extern const std::string LastFmSessionId;
        EXPORT extern const std::string LastFmUsername;
        EXPORT extern const std::string DisableAlbumArtistFallback;
        EXPORT extern const std::string AuddioApiToken;
        EXPORT extern const std::string LibraryType;
        EXPORT extern const std::string PlaybackTrackQueryTimeoutMs;
        EXPORT extern const std::string RemoteLibraryHostname;
        EXPORT extern const std::string RemoteLibraryWssPort;
        EXPORT extern const std::string RemoteLibraryHttpPort;
        EXPORT extern const std::string RemoteLibraryPassword;
        EXPORT extern const std::string RemoteLibraryViewed;
        EXPORT extern const std::string RemoteLibraryLatencyTimeoutMs;
        EXPORT extern const std::string RemoteLibraryWssTls;
        EXPORT extern const std::string RemoteLibraryHttpTls;
        EXPORT extern const std::string RemoteLibraryTlsWarningSuppressed;
        EXPORT extern const std::string RemoteLibraryTranscoderEnabled;
        EXPORT extern const std::string RemoteLibraryTranscoderFormat;
        EXPORT extern const std::string RemoteLibraryTranscoderBitrate;
        EXPORT extern const std::string RemoteLibraryIgnoreVersionMismatch;
        EXPORT extern const std::string AsyncTrackListQueries;
        EXPORT extern const std::string PiggyEnabled;
        EXPORT extern const std::string PiggyHostname;
    }

} } }

