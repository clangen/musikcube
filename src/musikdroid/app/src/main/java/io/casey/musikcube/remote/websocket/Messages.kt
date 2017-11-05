package io.casey.musikcube.remote.websocket

class Messages {
    enum class Request constructor(private val rawValue: String) {
        Authenticate("authenticate"),
        Ping("ping"),
        GetCurrentTime("get_current_time"),
        GetPlaybackOverview("get_playback_overview"),
        PauseOrResume("pause_or_resume"),
        Stop("stop"),
        Previous("previous"),
        Next("next"),
        PlayAtIndex("play_at_index"),
        ToggleShuffle("toggle_shuffle"),
        ToggleRepeat("toggle_repeat"),
        ToggleMute("toggle_mute"),
        SetVolume("set_volume"),
        SeekTo("seek_to"),
        SeekRelative("seek_relative"),
        PlayAllTracks("play_all_tracks"),
        PlayTracks("play_tracks"),
        PlayTracksByCategory("play_tracks_by_category"),
        QueryTracks("query_tracks"),
        QueryTracksByCategory("query_tracks_by_category"),
        QueryCategory("query_category"),
        QueryAlbums("query_albums"),
        QueryPlayQueueTracks("query_play_queue_tracks"),
        SavePlaylist("save_playlist"),
        RenamePlaylist("rename_playlist"),
        DeletePlaylist("delete_playlist"),
        AppendToPlaylist("append_to_playlist");

        override fun toString(): String {
            return rawValue
        }

        fun matches(name: String): Boolean {
            return rawValue == name
        }

        companion object {

            fun from(rawValue: String): Request? {
                for (value in Request.values()) {
                    if (value.toString() == rawValue) {
                        return value
                    }
                }

                return null
            }
        }
    }

    enum class Broadcast constructor(private val rawValue: String) {
        PlaybackOverviewChanged("playback_overview_changed"),
        PlayQueueChanged("play_queue_changed");

        override fun toString(): String {
            return rawValue
        }

        fun matches(rawValue: String): Boolean {
            return this.rawValue == rawValue
        }

        companion object {
            fun from(rawValue: String): Broadcast? {
                for (value in Broadcast.values()) {
                    if (value.toString() == rawValue) {
                        return value
                    }
                }

                return null
            }
        }
    }

    class Key {
        companion object {
            val CATEGORY = "category"
            val CATEGORY_ID = "category_id"
            val DATA = "data"
            val ID = "id"
            val IDS = "ids"
            val COUNT = "count"
            val COUNT_ONLY = "count_only"
            val OFFSET = "offset"
            val LIMIT = "limit"
            val INDEX = "index"
            val DELTA = "delta"
            val POSITION = "position"
            val VALUE = "value"
            val FILTER = "filter"
            val RELATIVE = "relative"
            val PLAYING_CURRENT_TIME = "playing_current_time"
        }
    }

    interface Value {
        companion object {
            val DELTA = "delta"
            val UP = "up"
            val DOWN = "down"
        }
    }

    interface Category {
        companion object {
            val OFFLINE = "offline"
            val ALBUM = "album"
            val ARTIST = "artist"
            val ALBUM_ARTIST = "album_artist"
            val GENRE = "genre"
            val PLAYLISTS = "playlists"
        }
    }
}
