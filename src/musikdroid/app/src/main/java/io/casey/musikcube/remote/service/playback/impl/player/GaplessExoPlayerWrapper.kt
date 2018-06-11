package io.casey.musikcube.remote.service.playback.impl.player

import android.content.Context
import android.content.SharedPreferences
import android.net.Uri
import com.danikula.videocache.CacheListener
import com.google.android.exoplayer2.*
import com.google.android.exoplayer2.extractor.DefaultExtractorsFactory
import com.google.android.exoplayer2.source.DynamicConcatenatingMediaSource
import com.google.android.exoplayer2.source.ExtractorMediaSource
import com.google.android.exoplayer2.source.MediaSource
import com.google.android.exoplayer2.source.TrackGroupArray
import com.google.android.exoplayer2.trackselection.AdaptiveTrackSelection
import com.google.android.exoplayer2.trackselection.DefaultTrackSelector
import com.google.android.exoplayer2.trackselection.TrackSelectionArray
import com.google.android.exoplayer2.upstream.DataSource
import com.google.android.exoplayer2.upstream.DefaultBandwidthMeter
import com.google.android.exoplayer2.upstream.DefaultDataSourceFactory
import com.google.android.exoplayer2.upstream.DefaultHttpDataSourceFactory
import com.google.android.exoplayer2.util.Util
import io.casey.musikcube.remote.Application
import io.casey.musikcube.remote.service.playback.PlayerWrapper
import io.casey.musikcube.remote.service.websocket.model.ITrack
import io.casey.musikcube.remote.ui.settings.constants.Prefs
import io.casey.musikcube.remote.util.Preconditions
import java.io.File

class GaplessExoPlayerWrapper : PlayerWrapper() {
    private var sourceFactory: DataSource.Factory
    private val extractorsFactory = DefaultExtractorsFactory()
    private var source: MediaSource? = null
    private var metadata: ITrack? = null
    private var prefetch: Boolean = false
    private var lastPosition: Long = -1
    private var percentAvailable = 0
    private var originalUri: String? = null
    private var proxyUri: String? = null
    private val transcoding: Boolean
    private var initialOffsetMs: Int = 0

    init {
        val userAgent = Util.getUserAgent(context, "musikdroid")

        val httpFactory: DataSource.Factory = DefaultHttpDataSourceFactory(
            userAgent, null, TIMEOUT, TIMEOUT, true)

        this.sourceFactory = DefaultDataSourceFactory(context, null, httpFactory)
        this.transcoding = prefs.getInt(Prefs.Key.TRANSCODER_BITRATE_INDEX, 0) != 0
    }

    override fun play(uri: String, metadata: ITrack, offsetMs: Int) {
        Preconditions.throwIfNotOnMainThread()

        if (!dead()) {
            removeAllAndReset()

            this.metadata = metadata
            this.originalUri = uri
            this.proxyUri = streamProxy.getProxyUrl(uri)
            this.initialOffsetMs = offsetMs

            addCacheListener()

            this.source = ExtractorMediaSource(Uri.parse(proxyUri), sourceFactory, extractorsFactory, null, null)

            addPlayer(this, this.source!!)

            state = State.Preparing
        }
    }

    override fun prefetch(uri: String, metadata: ITrack) {
        Preconditions.throwIfNotOnMainThread()

        if (!dead()) {
            removePending()

            this.metadata = metadata
            this.originalUri = uri
            this.proxyUri = streamProxy.getProxyUrl(uri)
            this.prefetch = true
            this.source = ExtractorMediaSource(Uri.parse(proxyUri), sourceFactory, extractorsFactory, null, null)

            addCacheListener()
            addPlayer(this, source!!)

            state = State.Prepared
        }
    }

    override fun pause() {
        Preconditions.throwIfNotOnMainThread()
        this.prefetch = true
        if (this.state == State.Playing) {
            gaplessPlayer?.playWhenReady = false
            state = State.Paused
        }
    }

    override fun resume() {
        Preconditions.throwIfNotOnMainThread()

        prefetch = false

        when (state) {
            State.Paused,
            State.Prepared -> {
                gaplessPlayer?.playWhenReady = true
                state = State.Playing
            }
            State.Error -> {
                gaplessPlayer?.playWhenReady = lastPosition == -1L
                gaplessPlayer?.prepare(source)
                state = State.Preparing
            }
            else -> { }
        }
    }

    override val uri get() = originalUri ?: ""

    override var position: Int
        get(): Int {
            Preconditions.throwIfNotOnMainThread()
            return gaplessPlayer?.currentPosition?.toInt() ?: 0
        }
        set(millis) {
            Preconditions.throwIfNotOnMainThread()

            this.lastPosition = -1
            if (gaplessPlayer?.playbackState != ExoPlayer.STATE_IDLE) {
                if (gaplessPlayer?.isCurrentWindowSeekable == true) {
                    var offset = millis.toLong()
                    val isInitialSeek = initialOffsetMs > 0 && (millis == initialOffsetMs)

                    /* if we're transcoding we don't want to seek arbitrarily because it may put
                    a lot of pressure on the backend. just allow seeking up to what we currently
                    have buffered! one exception: if we transfer playback context from the backend
                    to here, we want to wait until we are able to pickup where we left off. */
                    if (!isInitialSeek && transcoding && percentAvailable != 100) {
                        /* give ourselves 2% wiggle room! */
                        val percent = Math.max(0, percentAvailable - 2).toFloat() / 100.0f
                        val totalMs = gaplessPlayer?.duration
                        val available = (totalMs!!.toFloat() * percent).toLong()
                        offset = Math.min(millis.toLong(), available)
                    }

                    initialOffsetMs = 0
                    gaplessPlayer?.seekTo(offset)
                }
            }
        }

    override val duration: Int
        get() {
            Preconditions.throwIfNotOnMainThread()
            return gaplessPlayer?.duration?.toInt() ?: 0
        }

    override val bufferedPercent: Int
        get() = if (transcoding) percentAvailable else 100

    override fun updateVolume() {
        Preconditions.throwIfNotOnMainThread()
        gaplessPlayer?.volume = getVolume()
    }

    override fun setNextMediaPlayer(wrapper: PlayerWrapper?) {
        Preconditions.throwIfNotOnMainThread()
    }

    override fun dispose() {
        Preconditions.throwIfNotOnMainThread()

        if (!dead()) {
            state = State.Killing
            removePlayer(this)
            removeCacheListener()
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
        override fun onTimelineChanged(timeline: Timeline, manifest: Any?, reason: Int) {
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

                    gaplessPlayer!!.volume = getVolume()

                    if (lastPosition != -1L) {
                        gaplessPlayer?.seekTo(lastPosition)
                        lastPosition = -1
                    }
                    else if (initialOffsetMs > 0) {
                        position = initialOffsetMs
                    }

                    if (!prefetch) {
                        gaplessPlayer?.playWhenReady = true
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

            lastPosition = gaplessPlayer?.currentPosition ?: 0

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
            /* window index corresponds to the position of the current song in
            the queue. the current song should always be 0! if it's not, then
            that means we advanced to the next one... */
            if (gaplessPlayer?.currentWindowIndex ?: 0 != 0) {
                promoteNext()
                state = State.Finished
            }
        }

        override fun onPlaybackParametersChanged(playbackParameters: PlaybackParameters) {
        }

        override fun onRepeatModeChanged(repeatMode: Int) {
        }
    }

    companion object {
        const val TIMEOUT = 1000 * 60 * 2 /* 2 minutes; makes seeking an incomplete transcode work most of the time */
        private val prefs: SharedPreferences by lazy { Application.instance.getSharedPreferences(Prefs.NAME, Context.MODE_PRIVATE) }
        private val context: Context by lazy { Application.instance }
        private val trackSelector = DefaultTrackSelector(AdaptiveTrackSelection.Factory(DefaultBandwidthMeter()))
        private var all = mutableListOf<GaplessExoPlayerWrapper>()
        private lateinit var dcms: DynamicConcatenatingMediaSource
        private var gaplessPlayer: SimpleExoPlayer? = null

        private fun promoteNext() {
            if (all.size > 0) {
                removePlayer(all[0])
                if (all.size > 0) {
                    val next = all[0]
                    gaplessPlayer?.addListener(next.eventListener)
                    next.state = when(gaplessPlayer?.playbackState) {
                        Player.STATE_READY -> State.Playing
                        else -> State.Buffering
                    }
                }
            }
        }

        private fun removeAllAndReset() {
            all.forEach { gaplessPlayer?.removeListener(it.eventListener) }
            all.clear()
            gaplessPlayer?.stop()
            gaplessPlayer?.release()
            gaplessPlayer = ExoPlayerFactory.newSimpleInstance(context, trackSelector)
            dcms = DynamicConcatenatingMediaSource()
        }

        private fun removePending() {
            if (all.size > 0) {
                (1 until dcms.size).forEach {
                    dcms.removeMediaSource(1)
                }

                val remain = all.removeAt(0)
                all.forEach { gaplessPlayer?.removeListener(it.eventListener) }
                all = mutableListOf(remain)
            }
        }

        private fun addPlayer(wrapper: GaplessExoPlayerWrapper, source: MediaSource) {
            addActivePlayer(wrapper)

            if (all.size == 0) {
                gaplessPlayer?.addListener(wrapper.eventListener)
            }

            dcms.addMediaSource(source)
            all.add(wrapper)

            if (dcms.size == 1) {
                gaplessPlayer?.prepare(dcms)
            }
        }

        private fun removePlayer(wrapper: GaplessExoPlayerWrapper) {
            val index = all.indexOf(wrapper)
            if (index >= 0) {
                dcms.removeMediaSource(index)
                gaplessPlayer?.removeListener(all[index].eventListener)
                all.removeAt(index)
            }
            removeActivePlayer(wrapper)
        }
    }
}
