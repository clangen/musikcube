package io.casey.musikcube.remote.service.playback.impl.player

import android.content.Context
import android.content.SharedPreferences
import android.media.AudioManager
import android.media.MediaPlayer
import android.net.Uri
import android.os.PowerManager
import android.util.Base64
import android.util.Log
import io.casey.musikcube.remote.Application
import io.casey.musikcube.remote.service.websocket.model.ITrack
import io.casey.musikcube.remote.service.playback.PlayerWrapper
import io.casey.musikcube.remote.service.playback.impl.streaming.StreamProxy
import io.casey.musikcube.remote.util.Preconditions
import io.casey.musikcube.remote.ui.settings.constants.Prefs
import java.io.IOException
import java.util.*

class MediaPlayerWrapper : PlayerWrapper() {
    private val player = MediaPlayer()
    private var seekTo: Int = 0
    private var prefetching: Boolean = false
    private val context = Application.instance
    private val prefs: SharedPreferences
    private var metadata: ITrack? = null
    private var proxyUri: String? = null
    private var originalUri: String? = null
    override var bufferedPercent: Int = 0

    init {
        this.prefs = context!!.getSharedPreferences(Prefs.NAME, Context.MODE_PRIVATE)
    }

    override fun play(uri: String, metadata: ITrack) {
        Preconditions.throwIfNotOnMainThread()

        try {
            state = State.Preparing

            val userPass = "default:" + prefs.getString(Prefs.Key.PASSWORD, Prefs.Default.PASSWORD)!!
            val encoded = Base64.encodeToString(userPass.toByteArray(), Base64.NO_WRAP)
            val headers = HashMap<String, String>()
            headers.put("Authorization", "Basic " + encoded)

            this.metadata = metadata
            this.originalUri = uri
            this.proxyUri = streamProxy.getProxyUrl(uri)

            player.setDataSource(context, Uri.parse(proxyUri), headers)
            player.setAudioStreamType(AudioManager.STREAM_MUSIC)
            player.setOnPreparedListener(onPrepared)
            player.setOnErrorListener(onError)
            player.setOnCompletionListener(onCompleted)
            player.setOnBufferingUpdateListener(onBuffering)
            player.setWakeMode(Application.instance, PowerManager.PARTIAL_WAKE_LOCK)
            player.prepareAsync()
        }
        catch (e: IOException) {
            Log.e(TAG, "setDataSource failed: " + e.toString())
        }
    }

    override fun prefetch(uri: String, metadata: ITrack) {
        Preconditions.throwIfNotOnMainThread()

        this.prefetching = true
        play(uri, metadata)
    }

    override fun pause() {
        Preconditions.throwIfNotOnMainThread()

        if (isPreparedOrPlaying) {
            player.pause()
            state = State.Paused
        }
    }

    override var position: Int
        get() {
            Preconditions.throwIfNotOnMainThread()
            return if (isPreparedOrPlaying) this.player.currentPosition else 0
        }
        set(millis) {
            Preconditions.throwIfNotOnMainThread()

            if (isPreparedOrPlaying) {
                this.player.seekTo(millis)
                this.seekTo = 0
            }
            else {
                this.seekTo = millis
            }
        }

    override val duration: Int
        get() {
            Preconditions.throwIfNotOnMainThread()
            return if (isPreparedOrPlaying) this.player.duration else 0
        }

    override fun resume() {
        Preconditions.throwIfNotOnMainThread()

        if (state === State.Prepared || state === State.Paused) {
            player.start()
            this.state = State.Playing
        }
        else {
            prefetching = false
        }
    }

    override fun setNextMediaPlayer(wrapper: PlayerWrapper?) {
        Preconditions.throwIfNotOnMainThread()

        if (isPreparedOrPlaying) {
            try {
                if (wrapper is MediaPlayerWrapper) {
                    this.player.setNextMediaPlayer(wrapper.player)
                }
            }
            catch (ex: IllegalStateException) {
                Log.d(TAG, "invalid state for setNextMediaPlayer")
            }
        }
    }

    override fun updateVolume() {
        Preconditions.throwIfNotOnMainThread()

        val state = state
        if (state !== State.Preparing && state !== State.Disposed) {
            val volume = getVolume()
            player.setVolume(volume, volume)
        }
    }

    override val uri get() = originalUri ?: ""

    private val isPreparedOrPlaying: Boolean
        get() = (state === State.Playing || state === State.Prepared)

    override fun dispose() {
        Preconditions.throwIfNotOnMainThread()

        removeActivePlayer(this)

        if (state !== State.Preparing) {
            try {
                this.player.setNextMediaPlayer(null)
            }
            catch (ex: IllegalStateException) {
                Log.d(TAG, "failed to setNextMediaPlayer(null)")
            }

            try {
                this.player.stop()
            }
            catch (ex: IllegalStateException) {
                Log.d(TAG, "failed to stop()")
            }

            try {
                this.player.reset()
            }
            catch (ex: IllegalStateException) {
                Log.d(TAG, "failed to reset()")
            }

            this.player.release()

            setOnStateChangedListener(null)
            state = State.Disposed
        }
        else {
            state = State.Killing
        }
    }

    private val onPrepared = { _: MediaPlayer ->
        if (this.state === State.Killing) {
            dispose()
        }
        else {
            val volume = getVolume()
            player.setVolume(volume, volume)

            addActivePlayer(this)

            if (prefetching) {
                state = State.Prepared
            }
            else {
                this.player.start()

                if (this.seekTo != 0) {
                    position = this.seekTo
                }

                state = State.Playing
            }

            this.prefetching = false
        }
    }

    private val onError = { _: MediaPlayer, _: Int, _: Int ->
        state = State.Error
        dispose()
        true
    }

    private val onCompleted = { _: MediaPlayer ->
        state = State.Finished
        dispose()
    }

    private val onBuffering = { _: MediaPlayer, percent: Int ->
        bufferedPercent = percent

        if (bufferedPercent >= 100) {
            if (originalUri != null && metadata != null) {
                storeOffline(originalUri!!, metadata!!)
            }
        }
    }

    companion object {
        private val TAG = "MediaPlayerWrapper"
    }
}
