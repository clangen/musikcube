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

namespace musik { namespace core { namespace library { namespace constants {

    namespace Track {
        /* DB fields */
        static const char* TABLE_NAME = "tracks";
        static const char* ID = "id";
        static const char* TRACK_NUM = "track";
        static const char* DISC_NUM = "disc";
        static const char* BPM = "bpm";
        static const char* DURATION = "duration";
        static const char* FILESIZE = "filesize";
        static const char* YEAR = "year";
        static const char* TITLE = "title";
        static const char* FILENAME = "filename";
        static const char* FILETIME = "filetime";
        static const char* THUMBNAIL_ID = "thumbnail_id";
        static const char* GENRE_ID = "visual_genre_id";
        static const char* ARTIST_ID = "visual_artist_id";
        static const char* ALBUM_ARTIST_ID = "album_artist_id";
        static const char* ALBUM_ID = "album_id";
        static const char* PATH_ID = "path_id";
        static const char* SOURCE_ID = "source_id";
        static const char* EXTERNAL_ID = "external_id";
        static const char* RATING = "rating";
        static const char* LAST_PLAYED = "last_played";
        static const char* PLAY_COUNT = "play_count";
        static const char* DATE_ADDED = "date_added";
        static const char* DATE_UPDATED = "date_updated";

        /* used in Track instances where foreign key IDs have been
        replaced with actual values... */
        static const char* GENRE = "genre";
        static const char* ARTIST = "artist";
        static const char* ALBUM = "album";
        static const char* ALBUM_ARTIST = "album_artist";
        static const char* DIRECTORY = "directory";
    }

    namespace Genres {
        static const char* TABLE_NAME = "genres";
        static const char* ID = "id";
        static const char* NAME = "name";
        static const char* AGGREGATED = "aggregated";
        static const char* SORT_ORDER = "sort_order";
    }

    namespace TrackGenres {
        static const char* TABLE_NAME = "track_genres";
        static const char* ID = "id";
        static const char* TRACK_ID = "track_id";
        static const char* GENRE_ID = "genre_id";
    }

    namespace Artists {
        static const char* TABLE_NAME = "artists";
        static const char* ID = "id";
        static const char* NAME = "name";
        static const char* AGGREGATED = "aggregated";
        static const char* SORT_ORDER = "sort_order";
    }

    namespace ArtistGenres {
        static const char* TABLE_NAME = "artist_genres";
        static const char* ID = "id";
        static const char* TRACK_ID = "track_id";
        static const char* GENRE_ID = "artist_id";
    }

    namespace Albums {
        static const char* TABLE_NAME = "albums";
        static const char* ID = "id";
        static const char* NAME = "name";
        static const char* THUMBNAIL_ID = "thumbnail_id";
        static const char* SORT_ORDER = "sort_order";
    }

    namespace NormalizedKeys {
        static const char* TABLE_NAME = "meta_keys";
        static const char* ID = "id";
        static const char* NAME = "name";
    }

    namespace NormalizedValues {
        static const char* TABLE_NAME = "meta_values";
        static const char* ID = "id";
        static const char* KEY_ID = "meta_key_id";
        static const char* SORT_ORDER = "sort_order";
        static const char* CONTENT = "content";
    }

    namespace ExtendedTrackMetadata {
        static const char* TABLE_NAME = "track_meta";
        static const char* ID = "id";
        static const char* TRACK_ID = "track_id";
        static const char* VALUE_ID = "meta_value_id";
    }

    namespace Paths {
        static const char* TABLE_NAME = "paths";
        static const char* ID = "id";
        static const char* PATH = "path";
    }

    namespace Thumbnails {
        static const char* TABLE_NAME = "thumbnails";
        static const char* ID = "id";
        static const char* FILENAME = "filename";
        static const char* FILESIZE = "filesize";
        static const char* CHECKSUM = "checksum";
    }

    namespace Playlists {
        static const char* TABLE_NAME = "playlists";
        static const char* ID = "id";
        static const char* NAME = "name";
    }

    namespace PlaylistTracks {
        static const char* TABLE_NAME = "playlist_tracks";
        static const char* TRACK_ID = "track_id";
        static const char* PLAYLIST_ID = "playlist_id";
        static const char* SORT_ORDER = "sort_order";
    }

} } } }
