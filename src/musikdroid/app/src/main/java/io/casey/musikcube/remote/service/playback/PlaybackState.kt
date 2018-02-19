package io.casey.musikcube.remote.service.playback

enum class PlaybackState constructor(private val rawValue: String) {
    Stopped("stopped"),
    Buffering("buffering"), /* streaming only */
    Playing("playing"),
    Paused("paused");

    override fun toString(): String = rawValue

    companion object {
        internal fun from(rawValue: String): PlaybackState {
            if (Stopped.rawValue == rawValue || "unknown" == rawValue) {
                return Stopped
            }
            else if (Playing.rawValue == rawValue) {
                return Playing
            }
            else if (Paused.rawValue == rawValue || "prepared" == rawValue) {
                return Paused
            }

            throw IllegalArgumentException("rawValue '$rawValue' is unknown")
        }
    }
}
