package io.casey.musikcube.remote.websocket

class Prefs {
    interface Key {
        companion object {
            val ADDRESS = "address"
            val MAIN_PORT = "port"
            val AUDIO_PORT = "http_port"
            val PASSWORD = "password"
            val ALBUM_ART_ENABLED = "album_art_enabled"
            val MESSAGE_COMPRESSION_ENABLED = "message_compression_enabled"
            val STREAMING_PLAYBACK = "streaming_playback"
            val SOFTWARE_VOLUME = "software_volume"
            val SSL_ENABLED = "ssl_enabled"
            val CERT_VALIDATION_DISABLED = "cert_validation_disabled"
            val TRANSCODER_BITRATE_INDEX = "transcoder_bitrate_index"
            val DISK_CACHE_SIZE_INDEX = "disk_cache_size_index"
            val SYSTEM_SERVICE_FOR_REMOTE = "system_service_for_remote"
            val UPDATE_DIALOG_SUPPRESSED_VERSION = "update_dialog_suppressed_version"
        }
    }

    interface Default {
        companion object {
            val ADDRESS = "192.168.1.100"
            val MAIN_PORT = 7905
            val AUDIO_PORT = 7906
            val PASSWORD = ""
            val ALBUM_ART_ENABLED = true
            val MESSAGE_COMPRESSION_ENABLED = true
            val STREAMING_PLAYBACK = false
            val SOFTWARE_VOLUME = false
            val SSL_ENABLED = false
            val CERT_VALIDATION_DISABLED = false
            val TRANSCODER_BITRATE_INDEX = 0
            val DISK_CACHE_SIZE_INDEX = 0
            val SYSTEM_SERVICE_FOR_REMOTE = false
            val UPDATE_DIALOG_SUPPRESSED_VERSION = ""
        }
    }

    companion object {
        val NAME = "prefs"
    }
}
