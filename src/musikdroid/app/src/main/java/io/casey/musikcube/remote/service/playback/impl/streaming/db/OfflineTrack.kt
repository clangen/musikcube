package io.casey.musikcube.remote.service.playback.impl.streaming.db

import androidx.room.Entity
import androidx.room.PrimaryKey
import io.casey.musikcube.remote.service.playback.impl.remote.Metadata
import org.json.JSONException
import org.json.JSONObject

@Entity
class OfflineTrack {
    @PrimaryKey
    var externalId: String = ""

    var uri: String = ""
    var title: String = ""
    var album: String = ""
    var artist: String = ""
    var albumArtist: String = ""
    var genre: String = ""
    var albumId: Long = 0
    var artistId: Long = 0
    var albumArtistId: Long = 0
    var genreId: Long = 0
    var trackNum: Int = 0

    fun fromJSONObject(uri: String, from: JSONObject): Boolean {
        if (uri.isEmpty()) {
            throw IllegalArgumentException("uri cannot be empty")
        }

        val externalId = from.optString(Metadata.Track.EXTERNAL_ID, "")
        if (externalId.isEmpty()) {
            return false
        }

        this.externalId = externalId
        this.uri = uri
        this.title = from.optString(Metadata.Track.TITLE, "")
        this.album = from.optString(Metadata.Track.ALBUM, "")
        this.artist = from.optString(Metadata.Track.ARTIST, "")
        this.albumArtist = from.optString(Metadata.Track.ALBUM_ARTIST, "")
        this.genre = from.optString(Metadata.Track.GENRE, "")
        this.albumId = from.optLong(Metadata.Track.ALBUM_ID, -1L)
        this.artistId = from.optLong(Metadata.Track.ARTIST_ID, -1L)
        this.albumArtistId = from.optLong(Metadata.Track.ALBUM_ARTIST_ID, -1L)
        this.genreId = from.optLong(Metadata.Track.GENRE_ID, -1L)
        this.trackNum = from.optInt(Metadata.Track.TRACK_NUM, 0)

        return true
    }

    internal fun toJSONObject(): JSONObject {
        try {
            val json = JSONObject()
            json.put(Metadata.Track.TITLE, title)
            json.put(Metadata.Track.ALBUM, album)
            json.put(Metadata.Track.ARTIST, artist)
            json.put(Metadata.Track.ALBUM_ARTIST, albumArtist)
            json.put(Metadata.Track.GENRE, genre)
            json.put(Metadata.Track.ALBUM_ID, albumId)
            json.put(Metadata.Track.ARTIST_ID, artistId)
            json.put(Metadata.Track.ALBUM_ARTIST_ID, albumArtistId)
            json.put(Metadata.Track.GENRE_ID, genreId)
            json.put(Metadata.Track.TRACK_NUM, trackNum)
            json.put(Metadata.Track.EXTERNAL_ID, externalId)
            json.put(Metadata.Track.URI, uri)
            return json
        }
        catch (ex: JSONException) {
            throw RuntimeException("json serialization error")
        }
    }
}
