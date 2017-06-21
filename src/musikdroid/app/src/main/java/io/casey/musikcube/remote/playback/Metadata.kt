package io.casey.musikcube.remote.playback

object Metadata {
    interface Track {
        companion object {
            val ID = "id"
            val EXTERNAL_ID = "external_id"
            val URI = "uri"
            val TITLE = "title"
            val ALBUM = "album"
            val ALBUM_ID = "album_id"
            val ALBUM_ARTIST = "album_artist"
            val ALBUM_ARTIST_ID = "album_artist_id"
            val GENRE = "genre"
            val TRACK_NUM = "track_num"
            val GENRE_ID = "visual_genre_id"
            val ARTIST = "artist"
            val ARTIST_ID = "visual_artist_id"
        }
    }

    interface Album {
        companion object {
            val TITLE = "title"
            val ALBUM_ARTIST = "album_artist"
        }
    }
}
