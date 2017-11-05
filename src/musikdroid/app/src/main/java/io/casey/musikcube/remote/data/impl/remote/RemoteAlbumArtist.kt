package io.casey.musikcube.remote.data.impl.remote

import io.casey.musikcube.remote.data.IAlbumArtist
import io.casey.musikcube.remote.websocket.Messages
import org.json.JSONObject

class RemoteAlbumArtist(private val json: JSONObject) : IAlbumArtist {
    override val id: Long
        get() = json.optLong(Messages.Key.ID, -1)
    override val value: String
        get() = json.optString(Messages.Key.VALUE, "")
    override val type: String
        get() = Messages.Category.ALBUM_ARTIST
}