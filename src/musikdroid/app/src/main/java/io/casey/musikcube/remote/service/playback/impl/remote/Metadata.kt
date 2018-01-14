package io.casey.musikcube.remote.service.playback.impl.remote

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
            val GENRE_ID = "genre_id"
            val ARTIST = "artist"
            val ARTIST_ID = "artist_id"
            val THUMBNAIL_ID = "thumbnail_id"
        }
    }

    interface Album {
        companion object {
            val ID = "id"
            val TITLE = "title"
            val ALBUM_ARTIST = "album_artist"
            val ALBUM_ARTIST_ID = "album_artist_id"
            val ARTIST = "artist"
            val ARTIST_ID = "artist_id"
            val THUMBNAIL_ID = "thumbnail_id"
        }
    }

    interface Output {
        companion object {
            val DRIVER_NAME = "driver_name"
            val DEVICES = "devices"
        }
    }

    interface Device {
        companion object {
            val DEVICE_NAME = "device_name"
            val DEVICE_ID = "device_id"
        }
    }
}
