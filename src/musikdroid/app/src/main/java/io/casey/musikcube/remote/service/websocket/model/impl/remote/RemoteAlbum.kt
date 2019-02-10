package io.casey.musikcube.remote.service.websocket.model.impl.remote

import io.casey.musikcube.remote.service.playback.impl.remote.Metadata
import io.casey.musikcube.remote.service.websocket.model.IAlbum
import org.json.JSONObject

class RemoteAlbum(val json: JSONObject) : IAlbum {
    override val id: Long get() = json.optLong(Metadata.Album.ID, -1)
    override val value: String get() = json.optString(Metadata.Album.TITLE, "")
    override val name: String get() = value
    override val artist: String get() = json.optString(Metadata.Album.ARTIST, "")
    override val artistId: Long get() =json.optLong(Metadata.Album.ARTIST_ID, -1)
    override val albumArtist: String get() = json.optString(Metadata.Album.ALBUM_ARTIST, "")
    override val albumArtistId: Long get() = json.optLong(Metadata.Album.ALBUM_ARTIST_ID, -1)
    override val thumbnailId: Long get() = json.optLong(Metadata.Album.THUMBNAIL_ID, -1)
    override val type: String get() = Metadata.Category.ALBUM
}