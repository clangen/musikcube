package io.casey.musikcube.remote.service.websocket.model.impl.remote

import io.casey.musikcube.remote.service.playback.impl.remote.Metadata
import io.casey.musikcube.remote.service.websocket.model.ITrack
import org.json.JSONObject

class RemoteTrack(val json: JSONObject) : ITrack {
    override val id: Long
        get() = json.optLong(Metadata.Track.ID, -1)
    override val externalId: String
        get() = json.optString(Metadata.Track.EXTERNAL_ID, "")
    override val uri: String
        get() = json.optString(Metadata.Track.URI, "")
    override val title: String
        get() = json.optString(Metadata.Track.TITLE, "")
    override val album: String
        get() = json.optString(Metadata.Track.ALBUM, "")
    override val albumId: Long
        get() = json.optLong(Metadata.Track.ALBUM_ID, -1)
    override val albumArtist: String
        get() = json.optString(Metadata.Track.ALBUM_ARTIST, "")
    override val albumArtistId: Long
        get() = json.optLong(Metadata.Track.ALBUM_ARTIST_ID, -1)
    override val genre: String
        get() = json.optString(Metadata.Track.GENRE, "")
    override val trackNum: Int
        get() = json.optInt(Metadata.Track.TRACK_NUM, 0)
    override val genreId: Long
        get() = json.optLong(Metadata.Track.GENRE_ID, -1)
    override val artist: String
        get() = json.optString(Metadata.Track.ARTIST, "")
    override val artistId: Long
        get() = json.optLong(Metadata.Track.ARTIST_ID, -1)
    override val thumbnailId: Long
        get() = json.optLong(Metadata.Track.THUMBNAIL_ID, -1)

    override fun getCategoryId(categoryType: String): Long {
        val idKey = CATEGORY_NAME_TO_ID[categoryType]
        if (idKey != null && idKey.isNotEmpty()) {
            return json.optLong(idKey, -1L)
        }
        return -1L
    }

    override fun toJson(): JSONObject = JSONObject(json.toString())

    companion object {
        private val CATEGORY_NAME_TO_ID: Map<String, String> = mapOf(
            Metadata.Category.ALBUM_ARTIST to Metadata.Track.ALBUM_ARTIST_ID,
            Metadata.Category.GENRE to Metadata.Track.GENRE_ID,
            Metadata.Category.ARTIST to Metadata.Track.ARTIST_ID,
            Metadata.Category.ALBUM to Metadata.Track.ALBUM_ID,
            Metadata.Category.PLAYLISTS to Metadata.Track.ALBUM_ID)
    }
}
