package io.casey.musikcube.remote.data.impl.remote

import io.casey.musikcube.remote.data.IAlbum
import io.casey.musikcube.remote.playback.Metadata
import io.casey.musikcube.remote.websocket.Messages
import org.json.JSONObject

class RemoteAlbum(val json: JSONObject) : IAlbum {
    override val id: Long get() = json.optLong(Metadata.Album.ID, -1)
    override val value: String get() = json.optString(Metadata.Album.TITLE, "")
    override val albumArtist: String get() = json.optString(Metadata.Album.ALBUM_ARTIST, "")
    override val albumArtistId: Long get() = json.optLong(Metadata.Album.ALBUM_ARTIST_ID, -1)
    override val type: String get() = Messages.Category.ALBUM
}