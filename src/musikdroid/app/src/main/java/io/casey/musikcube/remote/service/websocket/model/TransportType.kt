package io.casey.musikcube.remote.service.websocket.model

enum class TransportType(val rawValue: String) {
    Gapless("gapless"),
    Crossfade("crossfade");

    companion object {
        fun find(raw: String): TransportType {
            values().forEach {
                if (it.rawValue == raw) {
                    return it
                }
            }
            return Gapless
        }
    }
}