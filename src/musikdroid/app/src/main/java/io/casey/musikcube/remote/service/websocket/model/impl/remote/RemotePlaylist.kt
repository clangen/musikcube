package io.casey.musikcube.remote.model.impl.remote

import io.casey.musikcube.remote.service.websocket.model.IPlaylist
import io.casey.musikcube.remote.service.websocket.Messages
import org.json.JSONObject

class RemotePlaylist(private val json: JSONObject) : IPlaylist {
    override val id: Long
        get() = json.optLong(Messages.Key.ID, -1)
    override val value: String
        get() = json.optString(Messages.Key.VALUE, "")
    override val type: String
        get() = Messages.Category.PLAYLISTS
}