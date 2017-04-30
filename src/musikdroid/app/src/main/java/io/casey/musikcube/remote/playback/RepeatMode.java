package io.casey.musikcube.remote.playback;

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
        } else if (List.rawValue.equals(rawValue)) {
            return List;
        } else if (Track.rawValue.equals(rawValue)) {
            return Track;
        }

        throw new IllegalArgumentException("rawValue is invalid");
    }
}
