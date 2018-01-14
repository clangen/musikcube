package io.casey.musikcube.remote.service.websocket.model.impl.remote

import io.casey.musikcube.remote.service.playback.impl.remote.Metadata
import io.casey.musikcube.remote.service.websocket.model.IDevice
import io.casey.musikcube.remote.service.websocket.model.IOutput
import org.json.JSONArray
import org.json.JSONObject

class RemoteOutput(val json: JSONObject): IOutput {
    private val deviceList: List<IDevice>

    init {
        val all = mutableListOf<IDevice>()
        val arr = json.optJSONArray(Metadata.Output.DEVICES) ?: JSONArray()
        for (i in 0 until arr.length()) {
            all.add(RemoteDevice(arr.getJSONObject(i)))
        }
        deviceList = all
    }

    override val name: String
        get() = json.optString(Metadata.Output.DRIVER_NAME, "")

    override val devices: List<IDevice>
        get() = deviceList
}