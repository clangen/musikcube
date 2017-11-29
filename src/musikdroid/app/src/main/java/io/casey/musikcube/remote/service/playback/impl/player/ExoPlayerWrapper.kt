package io.casey.musikcube.remote.service.playback.impl.player

import android.content.Context
import android.content.SharedPreferences
import android.net.Uri
import android.util.Log
import com.danikula.videocache.CacheListener
import com.google.android.exoplayer2.*
import com.google.android.exoplayer2.extractor.DefaultExtractorsFactory
import com.google.android.exoplayer2.extractor.ExtractorsFactory
import com.google.android.exoplayer2.source.ExtractorMediaSource
import com.google.android.exoplayer2.source.MediaSource
import com.google.android.exoplayer2.source.TrackGroupArray
import com.google.android.exoplayer2.trackselection.AdaptiveTrackSelection
import com.google.android.exoplayer2.trackselection.DefaultTrackSelector
import com.google.android.exoplayer2.trackselection.TrackSelectionArray
import com.google.android.exoplayer2.upstream.DataSource
import com.google.android.exoplayer2.upstream.DefaultBandwidthMeter
import com.google.android.exoplayer2.upstream.DefaultDataSourceFactory
import com.google.android.exoplayer2.util.Util
import io.casey.musikcube.remote.Application
import io.casey.musikcube.remote.service.playback.PlayerWrapper
import io.casey.musikcube.remote.service.playback.impl.streaming.StreamProxy
import io.casey.musikcube.remote.service.websocket.model.ITrack
import io.casey.musikcube.remote.ui.settings.constants.Prefs
import io.casey.musikcube.remote.util.Preconditions
import okhttp3.OkHttpClient
import java.io.File

class ExoPlayerWrapper : PlayerWrapper() {
    private val prefs: SharedPreferences
    private var datasources: DataSource.Factory? = null
    private val extractors: ExtractorsFactory
    private var source: MediaSource? = null
    private val player: SimpleExoPlayer?
    private var metadata: ITrack? = null
    private var prefetch: Boolean = false
    private val context: Context
    private var lastPosition: Long = -1
    private var percentAvailable = 0
    private var originalUri: String? = null
    private var proxyUri: String? = null
    private val transcoding: Boolean

    init {
        this.context = Application.instance!!
        val bandwidth = DefaultBandwidthMeter()
        val trackFactory = AdaptiveTrackSelection.Factory(bandwidth)
        val trackSelector = DefaultTrackSelector(trackFactory)
        this.player = ExoPlayerFactory.newSimpleInstance(this.context, trackSelector)
        this.extractors = DefaultExtractorsFactory()
        this.datasources = DefaultDataSourceFactory(context, Util.getUserAgent(context, "musikdroid"))
        this.prefs = Application.instance!!.getSharedPreferences(Prefs.NAME, Context.MODE_PRIVATE)
        this.transcoding = this.prefs.getInt(Prefs.Key.TRANSCODER_BITRATE_INDEX, 0) != 0
    }

    override fun play(uri: String, metadata: ITrack) {
        Preconditions.throwIfNotOnMainThread()

        if (!dead()) {
            this.metadata = metadata
            this.originalUri = uri
            this.proxyUri = streamProxy.getProxyUrl(uri)
            Log.d("ExoPlayerWrapper", "originalUri: ${this.originalUri} proxyUri: ${this.proxyUri}")

            addCacheListener()

            this.source = ExtractorMediaSource(Uri.parse(proxyUri), datasources, extractors, null, null)
            this.player!!.playWhenReady = true
            this.player.prepare(this.source)
            addActivePlayer(this)
            state = State.Preparing
        }
    }

    override fun prefetch(uri: String, metadata: ITrack) {
        Preconditions.throwIfNotOnMainThread()

        if (!dead()) {
            this.metadata = metadata
            this.originalUri = uri
            this.proxyUri = streamProxy.getProxyUrl(uri)
            Log.d("ExoPlayerWrapper", "originalUri: ${this.originalUri} proxyUri: ${this.proxyUri}")

            this.prefetch = true

            addCacheListener()

            this.source = ExtractorMediaSource(Uri.parse(proxyUri), datasources, extractors, null, null)
            this.player!!.playWhenReady = false
            this.player.prepare(this.source)
            addActivePlayer(this)
            state = State.Preparing
        }
    }

    override fun pause() {
        Preconditions.throwIfNotOnMainThread()

        this.prefetch = true

        if (this.state == State.Playing) {
            this.player!!.playWhenReady = false
            state = State.Paused
        }
    }

    override fun resume() {
        Preconditions.throwIfNotOnMainThread()

        prefetch = false

        when (state) {
            State.Paused,
            State.Prepared -> {
                player!!.playWhenReady = true
                state = State.Playing
            }

            State.Error -> {
                player!!.playWhenReady = lastPosition == -1L
                player.prepare(source)
                state = State.Preparing
            }

            else -> { }
        }

    }

    override val uri get() = originalUri ?: ""

    override var position: Int
        get(): Int {
            Preconditions.throwIfNotOnMainThread()
            return this.player!!.currentPosition.toInt()
        }
        set(millis) {
            Preconditions.throwIfNotOnMainThread()

            this.lastPosition = -1
            if (this.player!!.playbackState != ExoPlayer.STATE_IDLE) {
                if (this.player.isCurrentWindowSeekable) {
                    var offset = millis.toLong()

                    /* if we're transcoding we don't want to seek arbitrarily because it may put
                    a lot of pressure on the backend. just allow seeking up to what we currently
                    have buffered! */
                    if (transcoding && percentAvailable != 100) {
                        /* give ourselves 2% wiggle room! */
                        val percent = Math.max(0, percentAvailable - 2).toFloat() / 100.0f
                        val totalMs = this.player.duration
                        val available = (totalMs.toFloat() * percent).toLong()
                        offset = Math.min(millis.toLong(), available)
                    }

                    this.player.seekTo(offset)
                }
            }
        }

    override val duration: Int
        get() {
            Preconditions.throwIfNotOnMainThread()
            return this.player!!.duration.toInt()
        }

    override val bufferedPercent: Int
        get() {
            return if (transcoding) percentAvailable else 100
        }

    override fun updateVolume() {
        Preconditions.throwIfNotOnMainThread()
        this.player!!.volume = getVolume()
    }

    override fun setNextMediaPlayer(wrapper: PlayerWrapper?) {
        Preconditions.throwIfNotOnMainThread()
    }

    override fun dispose() {
        Preconditions.throwIfNotOnMainThread()

        if (!dead()) {
            state = State.Killing
            removeActivePlayer(this)
            removeCacheListener()
            this.player?.playWhenReady = false
            this.player?.removeListener(eventListener)
            this.player?.stop()
            this.player?.release()
            state = State.Disposed
        }
    }

    private fun dead(): Boolean {
        val state = state
        return state == State.Killing || state == State.Disposed
    }

    private fun addCacheListener() {
        if (streamProxy.isCached(this.originalUri!!)) {
            percentAvailable = 100

            if (originalUri != null && metadata != null) {
                storeOffline(originalUri!!, metadata!!)
            }
        }
        else {
            streamProxy.registerCacheListener(this.cacheListener, this.originalUri!!)
        }
    }

    private fun removeCacheListener() {
        streamProxy.unregisterCacheListener(this.cacheListener)
     }

    private val cacheListener = CacheListener { _: File, _: String, percent: Int ->
        percentAvailable = percent

        if (percentAvailable >= 100) {
            if (originalUri != null && metadata != null) {
                storeOffline(originalUri!!, metadata!!)
            }
        }
    }

    private var eventListener = object : Player.EventListener {
        override fun onTimelineChanged(timeline: Timeline, manifest: Any?) {
        }

        override fun onTracksChanged(trackGroups: TrackGroupArray, trackSelections: TrackSelectionArray) {
        }

        override fun onLoadingChanged(isLoading: Boolean) {
        }

        override fun onSeekProcessed() {
        }

        override fun onShuffleModeEnabledChanged(shuffleModeEnabled: Boolean) {
        }

        override fun onPlayerStateChanged(playWhenReady: Boolean, playbackState: Int) {
            Preconditions.throwIfNotOnMainThread()

            if (playbackState == ExoPlayer.STATE_BUFFERING) {
                state = State.Buffering
            }
            else if (playbackState == ExoPlayer.STATE_READY) {
                if (dead()) {
                    dispose()
                }
                else {
                    state = State.Prepared

                    player!!.volume = getVolume()

                    if (lastPosition != -1L) {
                        player.seekTo(lastPosition)
                        lastPosition = -1
                    }

                    if (!prefetch) {
                        player.playWhenReady = true
                        state = State.Playing
                    }
                    else {
                        state = State.Paused
                    }
                }
            }
            else if (playbackState == ExoPlayer.STATE_ENDED) {
                state = State.Finished
                dispose()
            }
        }

        override fun onPlayerError(error: ExoPlaybackException) {
            Preconditions.throwIfNotOnMainThread()

            lastPosition = player!!.currentPosition

            when (state) {
                State.Preparing,
                State.Prepared,
                State.Playing,
                State.Paused ->
                    state = State.Error
                else -> { }
            }
        }

        override fun onPositionDiscontinuity(type: Int) {
        }

        override fun onPlaybackParametersChanged(playbackParameters: PlaybackParameters) {
        }

        override fun onRepeatModeChanged(repeatMode: Int) {
        }
    }

    init {
        this.player!!.addListener(eventListener)
    }
}
