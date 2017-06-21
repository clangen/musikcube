package io.casey.musikcube.remote.playback

enum class RepeatMode constructor(private val rawValue: String) {
    None("none"),
    List("list"),
    Track("track");

    override fun toString(): String {
        return rawValue
    }

    companion object {
        fun from(rawValue: String): RepeatMode {
            if (None.rawValue == rawValue) {
                return None
            }
            else if (List.rawValue == rawValue) {
                return List
            }
            else if (Track.rawValue == rawValue) {
                return Track
            }

            throw IllegalArgumentException("rawValue matches invalid")
        }
    }
}
