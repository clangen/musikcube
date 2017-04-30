package io.casey.musikcube.remote.playback;

import android.content.Context;
import android.content.SharedPreferences;

public class PlaybackServiceFactory {
    private static StreamingPlaybackService streaming;
    private static RemotePlaybackService remote;
    private static SharedPreferences prefs;

    public static synchronized PlaybackService instance(final Context context) {
        init(context);

        if (prefs.getBoolean("streaming_playback", true)) {
            return streaming;
        }

        return remote;
    }

    public static synchronized StreamingPlaybackService streaming(final Context context) {
        init(context);
        return streaming;
    }

    public static synchronized RemotePlaybackService remote(final Context context) {
        init(context);
        return remote;
    }

    private static void init(final Context context) {
        if (streaming == null || remote == null || prefs == null) {
            prefs = context.getSharedPreferences("prefs", Context.MODE_PRIVATE);
            streaming = new StreamingPlaybackService(context);
            remote = new RemotePlaybackService(context);
        }
    }
}
