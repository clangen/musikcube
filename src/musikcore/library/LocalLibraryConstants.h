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

/** @file LocalLibraryConstants.h
 *  @brief Column and table name constants for the local library database schema.
 *  @details Groups constant strings describing the SQLite schema used by the
 *      local library: table names and their column names for tracks, genres,
 *      artists, albums, paths, thumbnails, playlists and normalized metadata. */

/** @namespace musik::core::library::constants
 *  @brief Constant names for the local library database schema. */
namespace musik { namespace core { namespace library { namespace constants {

    /** @brief Constants for the tracks table and its columns. */
    namespace Track {
        /* DB fields */
        static const char* TABLE_NAME = "tracks";      /**< Table name. */
        static const char* ID = "id";                  /**< Primary key. */
        static const char* TRACK_NUM = "track";        /**< Track number. */
        static const char* DISC_NUM = "disc";          /**< Disc number. */
        static const char* BPM = "bpm";                /**< Beats per minute. */
        static const char* DURATION = "duration";      /**< Duration, in seconds. */
        static const char* FILESIZE = "filesize";      /**< File size, in bytes. */
        static const char* YEAR = "year";              /**< Release year. */
        static const char* TITLE = "title";            /**< Track title. */
        static const char* FILENAME = "filename";      /**< File name on disk. */
        static const char* FILETIME = "filetime";      /**< File modification time. */
        static const char* THUMBNAIL_ID = "thumbnail_id"; /**< Foreign key to thumbnails. */
        static const char* GENRE_ID = "visual_genre_id"; /**< Denormalized genre id. */
        static const char* ARTIST_ID = "visual_artist_id"; /**< Denormalized artist id. */
        static const char* ALBUM_ARTIST_ID = "album_artist_id"; /**< Denormalized album artist id. */
        static const char* ALBUM_ID = "album_id";      /**< Denormalized album id. */
        static const char* PATH_ID = "path_id";        /**< Foreign key to paths. */
        static const char* SOURCE_ID = "source_id";    /**< Id of the indexer source. */
        static const char* EXTERNAL_ID = "external_id";/**< Id assigned by the source. */
        static const char* RATING = "rating";          /**< User rating (1-5). */
        static const char* LAST_PLAYED = "last_played";/**< Timestamp of last play. */
        static const char* PLAY_COUNT = "play_count";  /**< Number of plays. */
        static const char* DATE_ADDED = "date_added";  /**< Timestamp of insertion. */
        static const char* DATE_UPDATED = "date_updated"; /**< Timestamp of last update. */

        /* used in Track instances where foreign key IDs have been
        replaced with actual values... */
        static const char* GENRE = "genre";         /**< Genre name (denormalized). */
        static const char* ARTIST = "artist";       /**< Artist name (denormalized). */
        static const char* ALBUM = "album";         /**< Album name (denormalized). */
        static const char* ALBUM_ARTIST = "album_artist"; /**< Album artist name (denormalized). */
        static const char* DIRECTORY = "directory"; /**< Containing directory path. */
    }

    /** @brief Constants for the genres table. */
    namespace Genres {
        static const char* TABLE_NAME = "genres";   /**< Table name. */
        static const char* ID = "id";               /**< Primary key. */
        static const char* NAME = "name";           /**< Genre name. */
        static const char* AGGREGATED = "aggregated"; /**< Whether the row is a summary. */
        static const char* SORT_ORDER = "sort_order"; /**< Display sort order. */
    }

    /** @brief Constants for the track_genres association table. */
    namespace TrackGenres {
        static const char* TABLE_NAME = "track_genres"; /**< Table name. */
        static const char* ID = "id";               /**< Primary key. */
        static const char* TRACK_ID = "track_id";   /**< Foreign key to tracks. */
        static const char* GENRE_ID = "genre_id";   /**< Foreign key to genres. */
    }

    /** @brief Constants for the artists table. */
    namespace Artists {
        static const char* TABLE_NAME = "artists";  /**< Table name. */
        static const char* ID = "id";               /**< Primary key. */
        static const char* NAME = "name";           /**< Artist name. */
        static const char* AGGREGATED = "aggregated"; /**< Whether the row is a summary. */
        static const char* SORT_ORDER = "sort_order"; /**< Display sort order. */
    }

    /** @brief Constants for the artist_genres association table. */
    namespace ArtistGenres {
        static const char* TABLE_NAME = "artist_genres"; /**< Table name. */
        static const char* ID = "id";               /**< Primary key. */
        static const char* TRACK_ID = "track_id";   /**< Foreign key to tracks. */
        static const char* GENRE_ID = "artist_id";  /**< Foreign key to artists. */
    }

    /** @brief Constants for the albums table. */
    namespace Albums {
        static const char* TABLE_NAME = "albums";   /**< Table name. */
        static const char* ID = "id";               /**< Primary key. */
        static const char* NAME = "name";           /**< Album name. */
        static const char* THUMBNAIL_ID = "thumbnail_id"; /**< Foreign key to thumbnails. */
        static const char* SORT_ORDER = "sort_order"; /**< Display sort order. */
    }

    /** @brief Constants for the normalized metadata keys table. */
    namespace NormalizedKeys {
        static const char* TABLE_NAME = "meta_keys";/**< Table name. */
        static const char* ID = "id";               /**< Primary key. */
        static const char* NAME = "name";           /**< Metadata key name. */
    }

    /** @brief Constants for the normalized metadata values table. */
    namespace NormalizedValues {
        static const char* TABLE_NAME = "meta_values"; /**< Table name. */
        static const char* ID = "id";               /**< Primary key. */
        static const char* KEY_ID = "meta_key_id";  /**< Foreign key to meta_keys. */
        static const char* SORT_ORDER = "sort_order"; /**< Display sort order. */
        static const char* CONTENT = "content";     /**< Metadata value text. */
    }

    /** @brief Constants for the track_meta association table. */
    namespace ExtendedTrackMetadata {
        static const char* TABLE_NAME = "track_meta"; /**< Table name. */
        static const char* ID = "id";               /**< Primary key. */
        static const char* TRACK_ID = "track_id";   /**< Foreign key to tracks. */
        static const char* VALUE_ID = "meta_value_id"; /**< Foreign key to meta_values. */
    }

    /** @brief Constants for the paths table. */
    namespace Paths {
        static const char* TABLE_NAME = "paths";    /**< Table name. */
        static const char* ID = "id";               /**< Primary key. */
        static const char* PATH = "path";           /**< Directory path. */
    }

    /** @brief Constants for the thumbnails table. */
    namespace Thumbnails {
        static const char* TABLE_NAME = "thumbnails"; /**< Table name. */
        static const char* ID = "id";               /**< Primary key. */
        static const char* FILENAME = "filename";   /**< Thumbnail file name. */
        static const char* FILESIZE = "filesize";   /**< Thumbnail file size. */
        static const char* CHECKSUM = "checksum";   /**< Content checksum. */
    }

    /** @brief Constants for the playlists table. */
    namespace Playlists {
        static const char* TABLE_NAME = "playlists";/**< Table name. */
        static const char* ID = "id";               /**< Primary key. */
        static const char* NAME = "name";           /**< Playlist name. */
    }

    /** @brief Constants for the playlist_tracks association table. */
    namespace PlaylistTracks {
        static const char* TABLE_NAME = "playlist_tracks"; /**< Table name. */
        static const char* TRACK_ID = "track_id";   /**< Foreign key to tracks. */
        static const char* PLAYLIST_ID = "playlist_id"; /**< Foreign key to playlists. */
        static const char* SORT_ORDER = "sort_order"; /**< Position within the playlist. */
    }

} } } }
