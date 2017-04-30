package io.casey.musikcube.remote.util;

import android.app.Activity;

public final class Navigation {
    private Navigation() {
    }

    public interface RequestCode {
        int ALBUM_TRACKS_ACTIVITY = 10;
        int ALBUM_BROWSE_ACTIVITY = 11;
        int CATEGORY_TRACKS_ACTIVITY = 12;
    }

    public interface ResponseCode {
        int PLAYBACK_STARTED = Activity.RESULT_FIRST_USER + 0xbeef;
    }
}
