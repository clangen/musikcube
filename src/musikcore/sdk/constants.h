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

/** @file constants.h @brief Defines shared constants, enums, and metadata keys used throughout the SDK. */
#pragma once

#include "version.h"
#include <stdint.h>
#include <stddef.h>

/** @namespace musik::core::sdk @brief Core SDK interfaces shared between the musikcube application and its plugins. */
namespace musik {
    namespace core {
        namespace sdk {
            /** @brief The playback state of the current track. */
            enum class PlaybackState: int {
                Stopped = 1,
                Paused = 2,
                Prepared = 3,
                Playing = 4,
            };

            /** @brief The buffering and playback state of a stream. */
            enum class StreamState: int {
                Buffering = 1,
                Buffered = 2,
                Playing = 3,
                AlmostDone = 4,
                Finished = 5,
                Stopped = 6,
                Destroyed = 7,
                OpenFailed = -1
            };

            /** @brief The repeat mode used by the playback engine. */
            enum class RepeatMode: int {
                None = 0,
                Track = 1,
                List = 2
            };

            /** @brief Return values reported by output plugins when writing buffers. */
            enum class OutputState: int {
                FormatError = -4,
                InvalidState = -3,
                BufferFull = -2,
                BufferWritten = -1
            };

            /** @brief How a time change request should be applied by the playback engine. */
            enum class TimeChangeMode: int {
                Seek = 0,
                Scrub = 1
            };

            /** @brief Identifies well-known filesystem paths the application can provide. */
            enum class PathType: int {
                UserHome = 0,
                Data = 1,
                Application = 2,
                Plugins = 3,
                Library = 4
            };

            /** @brief Optional capabilities that streams or outputs may advertise. */
            enum class Capability: int {
                Prebuffer = 0x01
            };

            /** @brief The result of an indexer scan pass. */
            enum ScanResult {
                ScanCommit = 1,
                ScanRollback = 2
            };

            /** @brief The ReplayGain mode used when normalizing playback volume. */
            enum class ReplayGainMode: int {
                Disabled = 0,
                Track = 1,
                Album = 2
            };

            /** @brief The transport implementation used for gapless or crossfaded playback. */
            enum class TransportType: int {
                Gapless = 0,
                Crossfade = 1
            };

            /** @brief Access flags used when opening streams or files. */
            enum OpenFlags {
                None = 0,
                Read = 1,
                Write = 2
            };

            /** @brief Flags that modify stream processing behavior. */
            enum class StreamFlags: int {
                None = 0,
                NoDSP = 1
            };

            /** @brief The loading state of a track's metadata. */
            enum class MetadataState: int {
                NotLoaded = 0,
                Loading = 1,
                Loaded = 2,
                Missing = 3
            };

            /** @brief The number of bands in the built-in equalizer. */
            static const size_t EqualizerBandCount = 18;

            /** @brief The center frequencies, in Hz, of the equalizer's bands. */
            static const size_t EqualizerBands[] = {
                65, 92, 131, 185, 262, 370, 523, 740, 1047, 1480,
                2093, 2960, 4186, 5920, 8372, 11840, 16744, 22000
            };

            /** @brief Well-known category types used when querying the metadata index. */
            namespace category {
                /** @brief The album category type. */
                static const char* Album = "album";
                /** @brief The artist category type. */
                static const char* Artist = "artist";
                /** @brief The album artist category type. */
                static const char* AlbumArtist = "album_artist";
                /** @brief The genre category type. */
                static const char* Genre = "genre";
                /** @brief The playlist category type. */
                static const char* Playlist = "playlists";
            }

            /** @brief Well-known metadata keys associated with tracks. */
            namespace track {
                /** @brief The unique database id of the track. */
                static const char* Id = "id";
                /** @brief The track number on its disc. */
                static const char* TrackNum = "track";
                /** @brief The disc number within its album. */
                static const char* DiscNum = "disc";
                /** @brief The beats per minute of the track. */
                static const char* Bpm = "bpm";
                /** @brief The duration of the track, in seconds. */
                static const char* Duration = "duration";
                /** @brief The size of the underlying file, in bytes. */
                static const char* Filesize = "filesize";
                /** @brief The release year of the track. */
                static const char* Year = "year";
                /** @brief The track title. */
                static const char* Title = "title";
                /** @brief The filename of the underlying audio file. */
                static const char* Filename = "filename";
                /** @brief The thumbnail image id associated with the track. */
                static const char* ThumbnailId = "thumbnail_id";
                /** @brief The album name. */
                static const char* Album = "album";
                /** @brief The album artist name. */
                static const char* AlbumArtist = "album_artist";
                /** @brief The genre name. */
                static const char* Genre = "genre";
                /** @brief The artist name. */
                static const char* Artist = "artist";
                /** @brief The last-modified timestamp of the underlying file. */
                static const char* Filetime = "filetime";
                /** @brief The visual genre id used for display purposes. */
                static const char* GenreId = "visual_genre_id";
                /** @brief The visual artist id used for display purposes. */
                static const char* ArtistId = "visual_artist_id";
                /** @brief The visual album artist id used for display purposes. */
                static const char* AlbumArtistId = "album_artist_id";
                /** @brief The visual album id used for display purposes. */
                static const char* AlbumId = "album_id";
                /** @brief The indexer source id that produced the track. */
                static const char* SourceId = "source_id";
                /** @brief The external id used to correlate the track with its source. */
                static const char* ExternalId = "external_id";
            }

            /** @brief The current SDK version supported by this build of the application. */
            static const int SdkVersion = 21;
} } }
