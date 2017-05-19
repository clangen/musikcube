package io.casey.musikcube.remote.websocket;

public interface Prefs {
    String NAME = "prefs";

    interface Key {
        String ADDRESS = "address";
        String MAIN_PORT = "port";
        String AUDIO_PORT = "http_port";
        String PASSWORD = "password";
        String ALBUM_ART_ENABLED = "album_art_enabled";
        String MESSAGE_COMPRESSION_ENABLED = "message_compression_enabled";
        String STREAMING_PLAYBACK = "streaming_playback";
        String SOFTWARE_VOLUME = "software_volume";
        String SSL_ENABLED = "ssl_enabled";
        String CERT_VALIDATION_DISABLED = "cert_validation_disabled";
        String TRANSCODER_BITRATE_INDEX = "transcoder_bitrate_index";
        String DISK_CACHE_SIZE_INDEX = "disk_cache_size_index";
    }

    interface Default {
        String ADDRESS = "192.168.1.100";
        int MAIN_PORT = 7905;
        int AUDIO_PORT = 7906;
        String PASSWORD = "";
        boolean ALBUM_ART_ENABLED = true;
        boolean MESSAGE_COMPRESSION_ENABLED = true;
        boolean STREAMING_PLAYBACK = false;
        boolean SOFTWARE_VOLUME = false;
        boolean SSL_ENABLED = false;
        boolean CERT_VALIDATION_DISABLED = false;
        int TRANSCODER_BITRATE_INDEX = 0;
        int DISK_CACHE_SIZE_INDEX = 0;
    }
}
