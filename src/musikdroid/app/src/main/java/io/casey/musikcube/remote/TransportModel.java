package io.casey.musikcube.remote;

import org.json.JSONObject;

public class TransportModel {
    public interface Key {
        String STATE = "state";
        String REPEAT_MODE = "repeat_mode";
        String VOLUME = "volume";
        String SHUFFLED = "shuffled";
        String MUTED = "muted";
        String PLAY_QUEUE_COUNT = "track_count";
        String PLAY_QUEUE_POSITION = "play_queue_position";
        String PLAYING_DURATION = "playing_duration";
        String PLAYING_CURRENT_TIME = "playing_current_time";
        String PLAYING_TRACK = "playing_track";
        String TITLE = "title";
        String ALBUM = "album";
        String ARTIST = "artist";
        String ALBUM_ID = "album_id";
        String ARTIST_ID = "visual_artist_id";
    }

    public enum PlaybackState {
        Unknown("unknown"),
        Stopped("stopped"),
        Playing("playing"),
        Paused("paused");

        private final String rawValue;

        PlaybackState(String rawValue) {
            this.rawValue = rawValue;
        }

        @Override
        public String toString() {
            return rawValue;
        }

        static PlaybackState from(final String rawValue) {
            if (Stopped.rawValue.equals(rawValue)) {
                return Stopped;
            }
            else if (Playing.rawValue.equals(rawValue)) {
                return Playing;
            }
            else if (Paused.rawValue.equals(rawValue)) {
                return Paused;
            }

            throw new IllegalArgumentException("rawValue is invalid");
        }
    }

    public enum RepeatMode {
        None("none"),
        List("list"),
        Track("track");

        private final String rawValue;

        RepeatMode(String rawValue) {
            this.rawValue = rawValue;
        }

        @Override
        public String toString() {
            return rawValue;
        }

        public static RepeatMode from(final String rawValue) {
            if (None.rawValue.equals(rawValue)) {
                return None;
            }
            else if (List.rawValue.equals(rawValue)) {
                return List;
            }
            else if (Track.rawValue.equals(rawValue)) {
                return Track;
            }

            throw new IllegalArgumentException("rawValue is invalid");
        }
    }

    private PlaybackState playbackState = PlaybackState.Unknown;
    private RepeatMode repeatMode;
    private boolean shuffled;
    private boolean muted;
    private double volume;
    private int queueCount;
    private int queuePosition;
    private double duration;
    private double currentTime;
    private JSONObject track = new JSONObject();
    private boolean valid = false;

    public TransportModel() {
        reset();
    }

    public boolean canHandle(SocketMessage socketMessage) {
        if (socketMessage == null) {
            return false;
        }

        final String name = socketMessage.getName();

        return
            name.equals(Messages.Broadcast.PlaybackOverviewChanged.toString()) ||
            name.equals(Messages.Request.GetPlaybackOverview.toString());
    }

    public boolean update(SocketMessage message) {
        if (message == null) {
            reset();
            return false;
        }

        final String name = message.getName();

        if (!name.equals(Messages.Broadcast.PlaybackOverviewChanged.toString()) &&
            !name.equals(Messages.Request.GetPlaybackOverview.toString()))
        {
            throw new IllegalArgumentException("invalid message!");
        }

        playbackState = PlaybackState.from(message.getStringOption(Key.STATE));
        repeatMode = RepeatMode.from(message.getStringOption(Key.REPEAT_MODE));
        shuffled = message.getBooleanOption(Key.SHUFFLED);
        muted = message.getBooleanOption(Key.MUTED);
        volume = message.getDoubleOption(Key.VOLUME);
        queueCount = message.getIntOption(Key.PLAY_QUEUE_COUNT);
        queuePosition = message.getIntOption(Key.PLAY_QUEUE_POSITION);
        duration = message.getDoubleOption(Key.PLAYING_DURATION);
        currentTime = message.getDoubleOption(Key.PLAYING_CURRENT_TIME);
        track = message.getJsonObjectOption(Key.PLAYING_TRACK, new JSONObject());

        valid = true;
        return true;
    }

    public void reset() {
        playbackState = PlaybackState.Unknown;
        repeatMode = RepeatMode.None;
        shuffled = muted = false;
        volume = 0.0f;
        queueCount = queuePosition = 0;
        duration = currentTime = 0.0f;
        track = new JSONObject();
        valid = false;
    }

    public boolean isValid() {
        return valid;
    }

    public PlaybackState getPlaybackState() {
        return playbackState;
    }

    public RepeatMode getRepeatMode() {
        return repeatMode;
    }

    public boolean isShuffled() {
        return shuffled;
    }

    public boolean isMuted() {
        return muted;
    }

    public double getVolume() {
        return volume;
    }

    public int getQueueCount() {
        return queueCount;
    }

    public int getQueuePosition() {
        return queuePosition;
    }

    public double getDuration() {
        return duration;
    }

    public double getCurrentTime() {
        return currentTime;
    }

    public long getTrackValueLong(final String key) {
        return getTrackValueLong(key, -1);
    }

    public long getTrackValueLong(final String key, long defaultValue) {
        if (track.has(key)) {
            return track.optLong(key, defaultValue);
        }
        return defaultValue;
    }

    public String getTrackValueString(final String key) {
        return getTrackValueString(key, "-");
    }

    public String getTrackValueString(final String key, final String defaultValue) {
        if (track.has(key)) {
            return track.optString(key, defaultValue);
        }
        return defaultValue;
    }
}
