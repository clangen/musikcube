package io.casey.musikcube.remote.ui.settings.constants

class Prefs {
    object Key {
        const val ADDRESS = "address"
        const val MAIN_PORT = "port"
        const val AUDIO_PORT = "http_port"
        const val PASSWORD = "password"
        const val LASTFM_ENABLED = "lastfm_enabled"
        const val STREAMING_PLAYBACK = "streaming_playback"
        const val SOFTWARE_VOLUME = "software_volume"
        const val SSL_ENABLED = "ssl_enabled"
        const val CERT_VALIDATION_DISABLED = "cert_validation_disabled"
        const val TRANSCODER_BITRATE_INDEX = "transcoder_bitrate_index"
        const val TRANSCODER_FORMAT_INDEX = "transcoder_format_index"
        const val DISK_CACHE_SIZE_INDEX = "disk_cache_size_index"
        const val TITLE_ELLIPSIS_MODE_INDEX = "title_ellipsis_mode_index"
        const val UPDATE_DIALOG_SUPPRESSED_VERSION = "update_dialog_suppressed_version"
        const val TRANSFER_TO_SERVER_ON_HEADSET_DISCONNECT = "transfer_to_server_on_headset_disconnect"
        const val DEVICE_ID = "device_id"
    }

    object Default {
        const val ADDRESS = "192.168.1.100"
        const val MAIN_PORT = 7905
        const val AUDIO_PORT = 7906
        const val PASSWORD = ""
        const val LASTFM_ENABLED = true
        const val STREAMING_PLAYBACK = false
        const val SOFTWARE_VOLUME = false
        const val SSL_ENABLED = false
        const val CERT_VALIDATION_DISABLED = false
        const val TRANSCODER_BITRATE_INDEX = 0
        const val TRANSCODER_FORMAT_INDEX = 0
        const val TRANSFER_TO_SERVER_ON_HEADSET_DISCONNECT = false
        const val DISK_CACHE_SIZE_INDEX = 2
        const val TITLE_ELLIPSIS_SIZE_INDEX = 1
    }

    companion object {
        const val NAME = "prefs"
    }
}
