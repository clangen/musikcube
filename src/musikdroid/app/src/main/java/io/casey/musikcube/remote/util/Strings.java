package io.casey.musikcube.remote.util;

public class Strings {
    public static boolean empty(final String s) {
        return (s == null || s.length() == 0);
    }

    public static boolean notEmpty(final String s) {
        return (s != null && s.length() > 0);
    }
}
