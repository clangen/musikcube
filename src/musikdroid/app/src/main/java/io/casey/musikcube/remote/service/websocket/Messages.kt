package io.casey.musikcube.remote.service.websocket

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
        ListCategories("list_categories"),
        QueryTracksByCategory("query_tracks_by_category"),
        QueryTracksByExternalIds("query_tracks_by_external_ids"),
        QueryCategory("query_category"),
        QueryAlbums("query_albums"),
        QueryPlayQueueTracks("query_play_queue_tracks"),
        SavePlaylist("save_playlist"),
        RenamePlaylist("rename_playlist"),
        DeletePlaylist("delete_playlist"),
        AppendToPlaylist("append_to_playlist"),
        RemoveTracksFromPlaylist("remove_tracks_from_playlist"),
        ListOutputDrivers("list_output_drivers"),
        SetDefaultOutputDriver("set_default_output_driver"),
        GetGainSettings("get_gain_settings"),
        UpdateGainSettings("update_gain_settings"),
        RunIndexer("run_indexer");

        override fun toString(): String = rawValue
        fun matches(name: String): Boolean = (rawValue == name)

        companion object {
            fun from(rawValue: String): Request? {
                Request.values().forEach {
                    if (it.toString() == rawValue) {
                        return it
                    }
                }
                return null
            }
        }
    }

    enum class Broadcast constructor(private val rawValue: String) {
        PlaybackOverviewChanged("playback_overview_changed"),
        PlayQueueChanged("play_queue_changed");

        override fun toString(): String = rawValue
        fun matches(rawValue: String): Boolean = (this.rawValue == rawValue)

        companion object {
            fun from(rawValue: String): Broadcast? {
                Broadcast.values().forEach {
                    if (it.toString() == rawValue) {
                        return it
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
            val ALL = "all"
            val SELECTED = "selected"
            val ID = "id"
            val COUNT = "count"
            val COUNT_ONLY = "count_only"
            val IDS_ONLY = "ids_only"
            val OFFSET = "offset"
            val LIMIT = "limit"
            val INDEX = "index"
            val DELTA = "delta"
            val POSITION = "position"
            val VALUE = "value"
            val FILTER = "filter"
            val RELATIVE = "relative"
            val PLAYING_CURRENT_TIME = "playing_current_time"
            val PLAYLIST_ID = "playlist_id"
            val PLAYLIST_NAME = "playlist_name"
            val PREDICATE_CATEGORY = "predicate_category"
            val PREDICATE_ID = "predicate_id"
            val SUBQUERY = "subquery"
            val TYPE = "type"
            val OPTIONS = "options"
            val SUCCESS = "success"
            val EXTERNAL_IDS = "external_ids"
            val SORT_ORDERS = "sort_orders"
            val DRIVER_NAME = "driver_name"
            val DEVICE_ID = "device_id"
            val REPLAYGAIN_MODE = "replaygain_mode"
            val PREAMP_GAIN = "preamp_gain"
        }
    }

    interface Value {
        companion object {
            val UP = "up"
            val DOWN = "down"
            val REINDEX = "reindex"
            val REBUILD = "rebuild"
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
