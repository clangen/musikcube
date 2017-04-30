package io.casey.musikcube.remote.util;

import android.os.Looper;

public class Preconditions {
    public static void throwIfNotOnMainThread() {
        if (Thread.currentThread() != Looper.getMainLooper().getThread()) {
            throw new IllegalStateException("not on main thread");
        }
    }
}
