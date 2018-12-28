package io.casey.musikcube.remote.service.websocket.model

interface IEqualizerSettings {
    val enabled: Boolean
    val bands: Map<Int, Double>
}