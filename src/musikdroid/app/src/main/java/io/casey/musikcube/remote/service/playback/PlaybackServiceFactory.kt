package io.casey.musikcube.remote.service.playback

import android.content.Context
import android.content.SharedPreferences
import io.casey.musikcube.remote.service.playback.impl.remote.RemotePlaybackService
import io.casey.musikcube.remote.service.playback.impl.streaming.StreamingPlaybackService

import io.casey.musikcube.remote.ui.settings.constants.Prefs

object PlaybackServiceFactory {
    private var streaming: StreamingPlaybackService? = null
    private var remote: RemotePlaybackService? = null
    private var prefs: SharedPreferences? = null

    @Synchronized fun instance(context: Context): IPlaybackService {
        init(context)

        if (prefs!!.getBoolean(Prefs.Key.STREAMING_PLAYBACK, Prefs.Default.STREAMING_PLAYBACK)) {
            return streaming!!
        }

        return remote!!
    }

    @Synchronized fun streaming(context: Context): StreamingPlaybackService {
        init(context)
        return streaming!!
    }

    @Synchronized fun remote(context: Context): RemotePlaybackService {
        init(context)
        return remote!!
    }

    private fun init(context: Context) {
        if (streaming == null || remote == null || prefs == null) {
            prefs = context.getSharedPreferences(Prefs.NAME, Context.MODE_PRIVATE)
            streaming = StreamingPlaybackService(context)
            remote = RemotePlaybackService()
        }
    }
}
