package io.casey.musikcube.remote.service.websocket.model.impl.remote

import io.casey.musikcube.remote.service.websocket.Messages
import io.casey.musikcube.remote.service.websocket.model.IGainSettings
import io.casey.musikcube.remote.service.websocket.model.ReplayGainMode
import org.json.JSONObject

class RemoteGainSettings(val json: JSONObject): IGainSettings {
    override val replayGainMode: ReplayGainMode
        get() = ReplayGainMode.find(json.optString(Messages.Key.REPLAYGAIN_MODE, "disabled"))
    override val preampGain: Float
        get() = json.optDouble(Messages.Key.PREAMP_GAIN, 0.0).toFloat()
}