package io.casey.musikcube.remote.service.playback.impl.remote

object Metadata {
    object Track {
        const val ID = "id"
        const val EXTERNAL_ID = "external_id"
        const val URI = "uri"
        const val TITLE = "title"
        const val ALBUM = "album"
        const val ALBUM_ID = "album_id"
        const val ALBUM_ARTIST = "album_artist"
        const val ALBUM_ARTIST_ID = "album_artist_id"
        const val GENRE = "genre"
        const val TRACK_NUM = "track_num"
        const val GENRE_ID = "genre_id"
        const val ARTIST = "artist"
        const val ARTIST_ID = "artist_id"
        const val THUMBNAIL_ID = "thumbnail_id"
    }

    object Album {
        const val ID = "id"
        const val TITLE = "title"
        const val ALBUM_ARTIST = "album_artist"
        const val ALBUM_ARTIST_ID = "album_artist_id"
        const val ARTIST = "artist"
        const val ARTIST_ID = "artist_id"
        const val THUMBNAIL_ID = "thumbnail_id"
    }

    object Category {
        const val OFFLINE = "offline"
        const val ALBUM = "album"
        const val ARTIST = "artist"
        const val ALBUM_ARTIST = "album_artist"
        const val GENRE = "genre"
        const val TRACKS = "track"
        const val PLAYLISTS = "playlists"
    }

    object Output {
        const val DRIVER_NAME = "driver_name"
        const val DEVICES = "devices"
    }

    object Device {
        const val DEVICE_NAME = "device_name"
        const val DEVICE_ID = "device_id"
    }
}
