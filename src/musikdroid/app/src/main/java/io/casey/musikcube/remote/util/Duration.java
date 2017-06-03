package io.casey.musikcube.remote.util;

import java.util.Locale;

public class Duration {
    private Duration() {

    }

    public static String format(double seconds) {
        final int mins = ((int) seconds / 60);
        final int secs = (int) seconds - (mins * 60);
        return String.format(Locale.getDefault(), "%d:%02d", mins, secs);
    }
}
