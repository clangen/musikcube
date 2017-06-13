package io.casey.musikcube.remote.websocket;

public class Messages {
    public enum Request {
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
        QueryPlayQueueTracks("query_play_queue_tracks");

        private String rawValue;

        Request(String rawValue) {
            this.rawValue = rawValue;
        }

        @Override
        public String toString() {
            return rawValue;
        }

        public static Request from(String rawValue) {
            for (final Request value : Request.values()) {
                if (value.toString().equals(rawValue)) {
                    return value;
                }
            }

            return null;
        }

        public boolean is(final String name) {
            return rawValue.equals(name);
        }
    }

   public enum Broadcast {
        PlaybackOverviewChanged("playback_overview_changed"),
        PlayQueueChanged("play_queue_changed");

        private String rawValue;

        Broadcast(String rawValue) {
            this.rawValue = rawValue;
        }

        @Override
        public String toString() {
            return rawValue;
        }

       public boolean is(String rawValue) {
           return this.rawValue.equals(rawValue);
       }

        public static Broadcast from(String rawValue) {
            for (final Broadcast value : Broadcast.values()) {
                if (value.toString().equals(rawValue)) {
                    return value;
                }
            }

            return null;
        }
    }

    public interface Key {
        String CATEGORY = "category";
        String CATEGORY_ID = "category_id";
        String DATA = "data";
        String ID = "id";
        String IDS = "ids";
        String COUNT = "count";
        String COUNT_ONLY = "count_only";
        String OFFSET = "offset";
        String LIMIT = "limit";
        String INDEX = "index";
        String DELTA = "delta";
        String POSITION = "position";
        String VALUE = "value";
        String FILTER = "filter";
        String RELATIVE = "relative";
        String PLAYING_CURRENT_TIME = "playing_current_time";
    }

    public interface Value {
        String DELTA = "delta";
        String UP = "up";
        String DOWN = "down";
    }

    public interface Category {
        String OFFLINE = "offline";
        String ALBUM = "album";
        String ARTIST = "artist";
        String ALBUM_ARTIST = "album_artist";
        String GENRE = "genre";
        String PLAYLISTS = "playlists";
    }
}
