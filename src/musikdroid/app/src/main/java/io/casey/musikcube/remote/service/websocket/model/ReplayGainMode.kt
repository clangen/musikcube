package io.casey.musikcube.remote.service.websocket.model

enum class ReplayGainMode(val rawValue: String) {
    Disabled("disabled"),
    Album("album"),
    Track("track");

    companion object {
        fun find(raw: String): ReplayGainMode {
            values().forEach {
                if (it.rawValue == raw) {
                    return it
                }
            }
            return Disabled
        }
    }
}