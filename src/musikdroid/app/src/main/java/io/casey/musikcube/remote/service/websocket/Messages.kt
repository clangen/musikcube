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
        PlaySnapshotTracks("play_snapshot_tracks"),
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
        SetGainSettings("set_gain_settings"),
        GetEqualizerSettings("get_equalizer_settings"),
        SetEqualizerSettings("set_equalizer_settings"),
        RunIndexer("run_indexer"),
        GetTransportType("get_transport_type"),
        SetTransportType("set_transport_type"),
        SnapshotPlayQueue("snapshot_play_queue"),
        InvalidatePlayQueueSnapshot("invalidate_play_queue_snapshot");

        override fun toString(): String = rawValue
        fun matches(name: String): Boolean = (rawValue == name)

        companion object {
            fun from(rawValue: String): Request? {
                values().forEach {
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
                values().forEach {
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
            const val CATEGORY = "category"
            const val CATEGORY_ID = "category_id"
            const val DATA = "data"
            const val ALL = "all"
            const val SELECTED = "selected"
            const val ID = "id"
            const val COUNT = "count"
            const val COUNT_ONLY = "count_only"
            const val IDS_ONLY = "ids_only"
            const val OFFSET = "offset"
            const val LIMIT = "limit"
            const val INDEX = "index"
            const val DELTA = "delta"
            const val POSITION = "position"
            const val VALUE = "value"
            const val FILTER = "filter"
            const val RELATIVE = "relative"
            const val PLAYING_CURRENT_TIME = "playing_current_time"
            const val PLAYLIST_ID = "playlist_id"
            const val PLAYLIST_NAME = "playlist_name"
            const val PREDICATE_CATEGORY = "predicate_category"
            const val PREDICATE_ID = "predicate_id"
            const val SUBQUERY = "subquery"
            const val TYPE = "type"
            const val TIME = "time"
            const val OPTIONS = "options"
            const val SUCCESS = "success"
            const val EXTERNAL_IDS = "external_ids"
            const val SORT_ORDERS = "sort_orders"
            const val DRIVER_NAME = "driver_name"
            const val DEVICE_ID = "device_id"
            const val REPLAYGAIN_MODE = "replaygain_mode"
            const val PREAMP_GAIN = "preamp_gain"
            const val ENABLED = "enabled"
            const val BANDS = "bands"
        }
    }

    interface Value {
        companion object {
            const val UP = "up"
            const val DOWN = "down"
            const val REINDEX = "reindex"
            const val REBUILD = "rebuild"
        }
    }
}
