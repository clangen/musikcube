package io.casey.musikcube.remote.service.websocket.model.impl.remote

import io.casey.musikcube.remote.service.playback.impl.remote.Metadata
import io.casey.musikcube.remote.service.websocket.model.IEnvironment
import org.json.JSONObject

class RemoteEnvironment(val json: JSONObject = JSONObject()): IEnvironment {
    override val apiVersion: Int
        get() = json.optInt(Metadata.Environment.API_VERSION, -1)
    override val sdkVersion: Int
        get() = json.optInt(Metadata.Environment.SDK_VERSION, -1)
    override val serverVersion: String
        get() = json.optString(Metadata.Environment.APP_VERSION, "0.0.0")
}