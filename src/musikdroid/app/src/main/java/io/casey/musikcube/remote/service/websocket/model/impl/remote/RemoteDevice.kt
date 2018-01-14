package io.casey.musikcube.remote.service.websocket.model.impl.remote

import io.casey.musikcube.remote.service.playback.impl.remote.Metadata
import io.casey.musikcube.remote.service.websocket.model.IDevice
import org.json.JSONObject

class RemoteDevice(val json: JSONObject): IDevice {
    override val name: String
        get() = json.optString(Metadata.Device.DEVICE_NAME, "")
    override val id: String
        get() = json.optString(Metadata.Device.DEVICE_ID, "")
}