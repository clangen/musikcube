package io.casey.musikcube.remote.playback;

public enum PlaybackState {
    Stopped("stopped"),
    Buffering("buffering"), /* streaming only */
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
        if (Stopped.rawValue.equals(rawValue) || "unknown".equals(rawValue)) {
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
