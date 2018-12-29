package io.casey.musikcube.remote.service.websocket.model.impl.remote

import io.casey.musikcube.remote.service.websocket.Messages
import io.casey.musikcube.remote.service.websocket.model.IEqualizerSettings
import org.json.JSONObject

class RemoteEqualizerSettings(private val json: JSONObject): IEqualizerSettings {
    private val bandToFreqMap: Map<Int, Double>

    init {
        bandToFreqMap = mutableMapOf()

        json.optJSONObject(Messages.Key.BANDS)?.let { bands ->
            bands.keys().forEach { freq ->
                try {
                    bandToFreqMap[freq.toInt()] = bands.getDouble(freq)
                }
                catch (ex: NumberFormatException) {
                    /* ugh... */
                }
            }
        }
    }

    override val enabled: Boolean
        get() = json.optBoolean(Messages.Key.ENABLED, false)

    override val bands: MutableMap<Int, Double>
        get() = mutableMapOf<Int, Double>().apply { putAll(bandToFreqMap ) }
}
