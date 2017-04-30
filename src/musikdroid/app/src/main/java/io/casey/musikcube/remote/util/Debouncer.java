package io.casey.musikcube.remote.util;

import android.os.Handler;
import android.os.Looper;

public abstract class Debouncer<T> {
    private Handler handler = new Handler(Looper.getMainLooper());
    private T t = null;
    private long delay;

    public Debouncer(long delay) {
        this.delay = delay;
    }

    public void call() {
        handler.removeCallbacks(deferred);
        handler.postDelayed(deferred, delay);
    }

    public void call(T t) {
        this.t = t;
        handler.removeCallbacks(deferred);
        handler.postDelayed(deferred, delay);
    }

    public void cancel() {
        handler.removeCallbacks(deferred);
    }

    protected abstract void onDebounced(T caller);

    private Runnable deferred = () -> {
        onDebounced(t);
    };
}
