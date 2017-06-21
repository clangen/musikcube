package io.casey.musikcube.remote.playback

import android.content.Context
import android.content.SharedPreferences
import android.net.Uri
import android.util.Base64
import android.util.Log
import com.danikula.videocache.CacheListener
import com.google.android.exoplayer2.*
import com.google.android.exoplayer2.ext.okhttp.OkHttpDataSourceFactory
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
import io.casey.musikcube.remote.util.NetworkUtil
import io.casey.musikcube.remote.util.Preconditions
import io.casey.musikcube.remote.websocket.Prefs
import io.casey.musikcube.remote.playback.StreamProxy.*
import okhttp3.Cache
import okhttp3.OkHttpClient
import org.json.JSONObject
import java.io.File

class ExoPlayerWrapper : PlayerWrapper() {
    private val prefs: SharedPreferences
    private var datasources: DataSource.Factory? = null
    private val extractors: ExtractorsFactory
    private var source: MediaSource? = null
    private val player: SimpleExoPlayer?
    private var metadata: JSONObject? = null
    private var prefetch: Boolean = false
    private val context: Context
    private var lastPosition: Long = -1
    private var percentAvailable = 0
    private var originalUri: String? = null
    private var proxyUri: String? = null
    private val transcoding: Boolean

    private fun initHttpClient(uri: String) {
        if (StreamProxy.ENABLED) {
            return
        }

        synchronized(ExoPlayerWrapper::class.java) {
            if (audioStreamHttpClient == null) {
                val path = File(context.externalCacheDir, "audio")

                var diskCacheIndex = prefs.getInt(
                        Prefs.Key.DISK_CACHE_SIZE_INDEX, Prefs.Default.DISK_CACHE_SIZE_INDEX)

                if (diskCacheIndex < 0 || diskCacheIndex > StreamProxy.CACHE_SETTING_TO_BYTES.size) {
                    diskCacheIndex = 0
                }

                val builder = OkHttpClient.Builder()
                        .cache(Cache(path, StreamProxy.CACHE_SETTING_TO_BYTES[diskCacheIndex] ?: StreamProxy.MINIMUM_CACHE_SIZE_BYTES))
                        .addInterceptor { chain ->
                            var request = chain.request()
                            val userPass = "default:" + prefs.getString(Prefs.Key.PASSWORD, Prefs.Default.PASSWORD)!!
                            val encoded = Base64.encodeToString(userPass.toByteArray(), Base64.NO_WRAP)
                            request = request.newBuilder().addHeader("Authorization", "Basic " + encoded).build()
                            chain.proceed(request)
                        }

                if (prefs.getBoolean(Prefs.Key.CERT_VALIDATION_DISABLED, Prefs.Default.CERT_VALIDATION_DISABLED)) {
                    NetworkUtil.disableCertificateValidation(builder)
                }

                audioStreamHttpClient = builder.build()
            }
        }

        if (uri.startsWith("http")) {
            this.datasources = OkHttpDataSourceFactory(
                    audioStreamHttpClient,
                    Util.getUserAgent(context, "musikdroid"),
                    DefaultBandwidthMeter())
        }
        else {
            this.datasources = DefaultDataSourceFactory(
                    context, Util.getUserAgent(context, "musikdroid"))
        }
    }

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

    override fun play(uri: String, metadata: JSONObject) {
        Preconditions.throwIfNotOnMainThread()

        if (!dead()) {
            initHttpClient(uri)

            this.metadata = metadata
            this.originalUri = uri
            this.proxyUri = StreamProxy.getProxyUrl(uri)
            Log.d("ExoPlayerWrapper", "originalUri: ${this.originalUri} proxyUri: ${this.proxyUri}")

            addCacheListener()

            this.source = ExtractorMediaSource(Uri.parse(proxyUri), datasources, extractors, null, null)
            this.player!!.playWhenReady = true
            this.player.prepare(this.source)
            PlayerWrapper.addActivePlayer(this)
            state = State.Preparing
        }
    }

    override fun prefetch(uri: String, metadata: JSONObject) {
        Preconditions.throwIfNotOnMainThread()

        if (!dead()) {
            initHttpClient(uri)

            this.metadata = metadata
            this.originalUri = uri
            this.proxyUri = StreamProxy.getProxyUrl(uri)
            Log.d("ExoPlayerWrapper", "originalUri: ${this.originalUri} proxyUri: ${this.proxyUri}")

            this.prefetch = true

            addCacheListener()

            this.source = ExtractorMediaSource(Uri.parse(proxyUri), datasources, extractors, null, null)
            this.player!!.playWhenReady = false
            this.player.prepare(this.source)
            PlayerWrapper.addActivePlayer(this)
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
        this.player!!.volume = PlayerWrapper.getVolume()
    }

    override fun setNextMediaPlayer(wrapper: PlayerWrapper?) {
        Preconditions.throwIfNotOnMainThread()
    }

    override fun dispose() {
        Preconditions.throwIfNotOnMainThread()

        if (!dead()) {
            state = State.Killing
            PlayerWrapper.removeActivePlayer(this)
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
        if (StreamProxy.ENABLED) {
            if (StreamProxy.isCached(this.originalUri!!)) {
                percentAvailable = 100

                if (originalUri != null && metadata != null) {
                    PlayerWrapper.storeOffline(originalUri!!, metadata!!)
                }
            }
            else {
                StreamProxy.registerCacheListener(this.cacheListener, this.originalUri!!)
            }
        }
        else {
            percentAvailable = 100
        }
    }

    private fun removeCacheListener() {
        if (StreamProxy.ENABLED) {
            StreamProxy.unregisterCacheListener(this.cacheListener)
        }
    }

    private val cacheListener = CacheListener { _: File, _: String, percent: Int ->
        //Log.e("CLCLCL", String.format("%d", percent));
        percentAvailable = percent

        if (percentAvailable >= 100) {
            if (originalUri != null && metadata != null) {
                storeOffline(originalUri!!, metadata!!)
            }
        }
    }

    private var eventListener = object : ExoPlayer.EventListener {
        override fun onTimelineChanged(timeline: Timeline, manifest: Any?) {
        }

        override fun onTracksChanged(trackGroups: TrackGroupArray, trackSelections: TrackSelectionArray) {
        }

        override fun onLoadingChanged(isLoading: Boolean) {
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

                    player!!.volume = PlayerWrapper.getVolume()

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

        override fun onPositionDiscontinuity() {
        }

        override fun onPlaybackParametersChanged(playbackParameters: PlaybackParameters) {
        }
    }

    companion object {
        private var audioStreamHttpClient: OkHttpClient? = null
    }

    init {
        this.player!!.addListener(eventListener)
    }
}
