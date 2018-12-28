package io.casey.musikcube.remote.service.websocket.model

interface IGainSettings {
    val replayGainMode: ReplayGainMode
    val preampGain: Float
}