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

/** @file PreferenceKeys.h
 *  @brief Well-known preference component and key names.
 *  @details Names of the preference components (files) and the keys used within
 *      them. Centralizing these avoids string literals scattered across the code
 *      base. */

#include <string>

/** @namespace musik::core::prefs
 *  @brief Preference component and key name constants. */
namespace musik { namespace core { namespace prefs {

    /** @brief Names of the preference components (files). */
    namespace components {
        extern const std::string Settings;   /**< General application settings. */
        extern const std::string Libraries;  /**< Library configuration. */
        extern const std::string Playback;   /**< Playback settings. */
        extern const std::string Plugins;    /**< Plugin settings. */
        extern const std::string Session;    /**< Session-scoped (transient) settings. */
    }

    /** @brief Preference key names within a component. */
    namespace keys {
        extern const std::string AutoSyncIntervalMillis;     /**< Indexer auto-sync interval. */
        extern const std::string RemoveMissingFiles;         /**< Remove missing files during sync. */
        extern const std::string SyncOnStartup;              /**< Sync the library on startup. */
        extern const std::string ResumePlaybackOnStartup;    /**< Resume the last queue on startup. */
        extern const std::string Volume;                     /**< Last volume level. */
        extern const std::string RepeatMode;                 /**< Last repeat mode. */
        extern const std::string TimeChangeMode;             /**< Last time-change mode. */
        extern const std::string OutputPlugin;               /**< Selected output plugin. */
        extern const std::string Transport;                  /**< Selected transport type. */
        extern const std::string Locale;                     /**< Selected locale. */
        extern const std::string IndexerLogEnabled;          /**< Whether indexer logging is on. */
        extern const std::string IndexerThreadCount;         /**< Indexer worker thread count. */
        extern const std::string IndexerTransactionInterval; /**< Indexer transaction batch interval. */
        extern const std::string ReplayGainMode;             /**< ReplayGain processing mode. */
        extern const std::string PreampDecibels;             /**< ReplayGain preamp in decibels. */
        extern const std::string SaveSessionOnExit;          /**< Persist session on exit. */
        extern const std::string LastPlayQueueIndex;         /**< Last queue index (resume). */
        extern const std::string LastPlayQueueTime;          /**< Last queue position (resume). */
        extern const std::string LastFmToken;                /**< Last.fm auth token. */
        extern const std::string LastFmSessionId;            /**< Last.fm session id. */
        extern const std::string LastFmUsername;             /**< Last.fm username. */
        extern const std::string DisableAlbumArtistFallback; /**< Disable album-artist fallback. */
        extern const std::string AuddioApiToken;             /**< Auddio API token. */
        extern const std::string LibraryType;                /**< Primary library type. */
        extern const std::string PlaybackTrackQueryTimeoutMs;/**< Track query timeout for playback. */
        extern const std::string RemoteLibraryHostname;      /**< Remote library host. */
        extern const std::string RemoteLibraryWssPort;       /**< Remote library WSS port. */
        extern const std::string RemoteLibraryHttpPort;      /**< Remote library HTTP port. */
        extern const std::string RemoteLibraryPassword;      /**< Remote library password. */
        extern const std::string RemoteLibraryViewed;        /**< Whether the remote library was viewed. */
        extern const std::string RemoteLibraryLatencyTimeoutMs; /**< Remote query latency timeout. */
        extern const std::string RemoteLibraryWssTls;        /**< Use TLS for the WSS connection. */
        extern const std::string RemoteLibraryHttpTls;       /**< Use TLS for the HTTP connection. */
        extern const std::string RemoteLibraryTlsWarningSuppressed; /**< Whether TLS warnings are suppressed. */
        extern const std::string RemoteLibraryTranscoderEnabled;   /**< Enable server-side transcoding. */
        extern const std::string RemoteLibraryTranscoderFormat;    /**< Transcode output format. */
        extern const std::string RemoteLibraryTranscoderBitrate;   /**< Transcode bitrate. */
        extern const std::string RemoteLibraryIgnoreVersionMismatch; /**< Ignore server version mismatch. */
        extern const std::string AsyncTrackListQueries;      /**< Run track-list queries asynchronously. */
        extern const std::string PiggyEnabled;               /**< Whether the Piggy service is enabled. */
        extern const std::string PiggyHostname;              /**< Piggy service host. */
    }

} } }

