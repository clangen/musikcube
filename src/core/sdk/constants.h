//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004-2019 musikcube team
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

#include <stdint.h>
#include <stddef.h>

namespace musik {
    namespace core {
        namespace sdk {
            enum PlaybackState {
                PlaybackStopped = 1,
                PlaybackPaused = 2,
                PlaybackPrepared = 3,
                PlaybackPlaying = 4,
            };

            enum StreamEventType {
                StreamScheduled = 1,
                StreamPrepared = 2,
                StreamPlaying = 3,
                StreamAlmostDone = 4,
                StreamFinished = 5,
                StreamStopped = 6,
                StreamError = -1
            };

            enum RepeatMode {
                RepeatNone = 0,
                RepeatTrack = 1,
                RepeatList = 2
            };

            enum OutputPlay {
                OutputFormatError = -4,
                OutputInvalidState = -3,
                OutputBufferFull = -2,
                OutputBufferWritten = -1
            };

            enum TimeChangeMode {
                TimeChangeSeek = 0,
                TimeChangeScrub = 1
            };

            enum PathType {
                PathUserHome = 0,
                PathData = 1,
                PathApplication = 2,
                PathPlugins = 3,
                PathLibrary = 4
            };

            enum class Capability : int {
                Prebuffer = 0x01
            };

            enum ScanResult {
                ScanCommit = 1,
                ScanRollback = 2
            };

            enum class ReplayGainMode : int {
                Disabled = 0,
                Track = 1,
                Album = 2
            };

            enum class TransportType : int {
                Gapless = 0,
                Crossfade = 1
            };

            enum OpenFlags {
                None = 0,
                Read = 1,
                Write = 2
            };

            enum class StreamFlags: int {
                None = 0,
                NoDSP = 1
            };

            static const size_t EqualizerBandCount = 18;

            static const size_t EqualizerBands[] = {
                65, 92, 131, 185, 262, 370, 523, 740, 1047, 1480,
                2093, 2960, 4186, 5920, 8372, 11840, 16744, 22000
            };

            namespace category {
                static const char* Album = "album";
                static const char* Artist = "artist";
                static const char* AlbumArtist = "album_artist";
                static const char* Genre = "genre";
                static const char* Playlist = "playlists";
            }

            namespace track {
                static const char* Id = "id";
                static const char* TrackNum = "track";
                static const char* DiscNum = "disc";
                static const char* Bpm = "bpm";
                static const char* Duration = "duration";
                static const char* Filesize = "filesize";
                static const char* Year = "year";
                static const char* Title = "title";
                static const char* Filename = "filename";
                static const char* ThumbnailId = "thumbnail_id";
                static const char* Album = "album";
                static const char* AlbumArtist = "album_artist";
                static const char* Genre = "genre";
                static const char* Artist = "artist";
                static const char* Filetime = "filetime";
                static const char* GenreId = "visual_genre_id";
                static const char* ArtistId = "visual_artist_id";
                static const char* AlbumArtistId = "album_artist_id";
                static const char* AlbumId = "album_id";
                static const char* SourceId = "source_id";
                static const char* ExternalId = "external_id";
            }

            static const int SdkVersion = 18;
} } }
