package io.casey.musikcube.remote.data.impl.remote

import io.casey.musikcube.remote.data.ITrack
import io.casey.musikcube.remote.playback.Metadata
import org.json.JSONObject

class RemoteTrack(val json: JSONObject) : ITrack {
    override val id: Long
        get() = json.optLong(Metadata.Track.ID, -1)
    override val externalId: String
        get() = json.optString(Metadata.Track.EXTERNAL_ID, "")
    override val uri: String
        get() = json.optString(Metadata.Track.URI, "")
    override val title: String
        get() = json.optString(Metadata.Track.TITLE, "<no name>")
    override val album: String
        get() = json.optString(Metadata.Track.ALBUM, "<no album>")
    override val albumId: Long
        get() = json.optLong(Metadata.Track.ALBUM_ID, -1)
    override val albumArtist: String
        get() = json.optString(Metadata.Track.ALBUM_ARTIST, "<no album artist>")
    override val albumArtistId: Long
        get() = json.optLong(Metadata.Track.ALBUM_ARTIST_ID, -1)
    override val genre: String
        get() = json.optString(Metadata.Track.GENRE, "<no genre>")
    override val trackNum: Int
        get() = json.optInt(Metadata.Track.TRACK_NUM, 0)
    override val genreId: Long
        get() = json.optLong(Metadata.Track.GENRE_ID, -1)
    override val artist: String
        get() = json.optString(Metadata.Track.ARTIST, "<no artist>")
    override val artistId: Long
        get() = json.optLong(Metadata.Track.ARTIST_ID, -1)

    override fun getString(key: String, default: String): String {
        return json.optString(key, default)
    }

    override fun getLong(key: String, default: Long): Long {
        return json.optLong(key, default)
    }

    override fun toJson(): JSONObject {
        return JSONObject(json.toString())
    }
}
