package io.casey.musikcube.remote.playback

import android.content.Context
import android.content.SharedPreferences
import android.database.ContentObserver
import android.media.AudioManager
import android.os.Handler
import android.os.Looper
import android.provider.Settings
import android.util.Log
import io.casey.musikcube.remote.Application
import io.casey.musikcube.remote.R
import io.casey.musikcube.remote.data.IDataProvider
import io.casey.musikcube.remote.data.ITrack
import io.casey.musikcube.remote.data.impl.remote.RemoteTrack
import io.casey.musikcube.remote.injection.DaggerServiceComponent
import io.casey.musikcube.remote.injection.DataModule
import io.casey.musikcube.remote.ui.model.TrackListSlidingWindow
import io.casey.musikcube.remote.util.Strings
import io.casey.musikcube.remote.websocket.Messages
import io.casey.musikcube.remote.websocket.Prefs
import io.reactivex.Observable
import io.reactivex.android.schedulers.AndroidSchedulers
import org.json.JSONObject
import java.net.URLEncoder
import java.util.*
import javax.inject.Inject

class StreamingPlaybackService(context: Context) : IPlaybackService {
    @Inject lateinit var dataProvider: IDataProvider

    private val prefs: SharedPreferences = context.getSharedPreferences(Prefs.NAME, Context.MODE_PRIVATE)
    private val listeners = HashSet<() -> Unit>()
    private var params: QueueParams? = null
    private var playContext = PlaybackContext()
    private var audioManager: AudioManager? = null
    private var lastSystemVolume: Int = 0
    private var pausedByTransientLoss = false
    private val random = Random()
    private val handler = Handler()

    private val trackMetadataCache = object : LinkedHashMap<Int, ITrack>() {
        override fun removeEldestEntry(eldest: MutableMap.MutableEntry<Int, ITrack>): Boolean {
            return size >= MAX_TRACK_METADATA_CACHE_SIZE
        }
    }

    private class PlaybackContext {
        internal var queueCount: Int = 0
        internal var currentPlayer: PlayerWrapper? = null
        internal var nextPlayer: PlayerWrapper? = null
        internal var currentMetadata: ITrack? = null
        internal var nextMetadata: ITrack? = null
        internal var currentIndex = -1
        internal var nextIndex = -1
        internal var nextPlayerScheduled: Boolean = false

        fun stopPlaybackAndReset() {
            reset(currentPlayer)
            reset(nextPlayer)
            nextPlayerScheduled = false
            nextPlayer = null
            currentPlayer = null
            nextMetadata = null
            currentMetadata = null
            nextIndex = -1
            currentIndex = -1
        }

        fun notifyNextTrackPrepared() {
            if (currentPlayer != null && nextPlayer != null) {
                currentPlayer?.setNextMediaPlayer(nextPlayer)
                nextPlayerScheduled = true
            }
        }

        fun advanceToNextTrack(currentTrackListener: (PlayerWrapper, PlayerWrapper.State) -> Unit): Boolean {
            var startedNext = false

            if (nextMetadata != null && nextPlayer != null) {
                if (currentPlayer != null) {
                    currentPlayer?.setOnStateChangedListener(null)
                    currentPlayer?.dispose()
                }

                currentMetadata = nextMetadata
                currentIndex = nextIndex
                currentPlayer = nextPlayer
                startedNext = true
            }
            else {
                reset(currentPlayer)
                currentPlayer = null
                currentMetadata = null
                currentIndex = 0
            }

            nextPlayer = null
            nextMetadata = null
            nextIndex = -1
            nextPlayerScheduled = false

            /* needs to be done after swapping current/next, otherwise event handlers
            will fire, and things may get cleaned up before we have a chance to start */
            if (startedNext) {
                currentPlayer?.setOnStateChangedListener(currentTrackListener)
                currentPlayer?.resume() /* no-op if playing already */
            }

            return startedNext
        }

        fun reset(wrapper: PlayerWrapper?) {
            if (wrapper != null) {
                wrapper.setOnStateChangedListener(null)
                wrapper.dispose()

                if (wrapper === nextPlayer) {
                    nextPlayerScheduled = false /* uhh... */
                }
            }
        }
    }

    private class QueueParams {
        internal val category: String?
        internal val categoryId: Long
        internal val filter: String

        constructor(filter: String) {
            this.filter = filter
            this.categoryId = -1
            this.category = null
        }

        constructor(category: String, categoryId: Long, filter: String) {
            this.category = category
            this.categoryId = categoryId
            this.filter = filter
        }
    }

    init {
        DaggerServiceComponent.builder()
            .appComponent(Application.appComponent)
            .dataModule(DataModule())
            .build().inject(this)
    }

    @Synchronized override fun connect(listener: () -> Unit) {
        listeners.add(listener)
        if (listeners.size == 1) {
            dataProvider.attach()
        }
    }

    @Synchronized override fun disconnect(listener: () -> Unit) {
        listeners.remove(listener)
        if (listeners.size == 0) {
            dataProvider.detach()
        }
    }

    override fun playAll() {
        playAll(0, "")
    }

    override fun playAll(index: Int, filter: String) {
        if (requestAudioFocus()) {
            trackMetadataCache.clear()
            loadQueueAndPlay(QueueParams(filter), index)
        }
    }

    override fun play(category: String, categoryId: Long, index: Int, filter: String) {
        if (requestAudioFocus()) {
            trackMetadataCache.clear()
            loadQueueAndPlay(QueueParams(category, categoryId, filter), index)
        }
    }

    override fun playAt(index: Int) {
        if (params != null) {
            if (requestAudioFocus()) {
                playContext.stopPlaybackAndReset()
                loadQueueAndPlay(params!!, index)
            }
        }
    }

    override fun pauseOrResume() {
        if (playContext.currentPlayer != null) {
            if (playbackState === PlaybackState.Playing || playbackState === PlaybackState.Buffering) {
                pause()
            }
            else {
                resume()
            }
        }
    }

    override fun pause() {
        if (playbackState != PlaybackState.Paused) {
            schedulePausedSleep()
            killAudioFocus()

            if (playContext.currentPlayer != null) {
                playContext.currentPlayer?.pause()
                setState(PlaybackState.Paused)
            }
        }
    }

    override fun resume() {
        if (requestAudioFocus()) {
            cancelScheduledPausedSleep()
            pausedByTransientLoss = false

            if (playContext.currentPlayer != null) {
                playContext.currentPlayer?.resume()
                setState(PlaybackState.Playing)
            }
        }
    }

    override fun stop() {
        SystemService.shutdown()
        killAudioFocus()
        playContext.stopPlaybackAndReset()
        trackMetadataCache.clear()
        setState(PlaybackState.Stopped)
    }

    override fun prev() {
        if (requestAudioFocus()) {
            cancelScheduledPausedSleep()

            if (playContext.currentPlayer != null) {
                if (playContext.currentPlayer?.position ?: 0 > PREV_TRACK_GRACE_PERIOD_MILLIS) {
                    playContext.currentPlayer?.position = 0
                    return
                }
            }

            moveToPrevTrack()
        }
    }

    override fun next() {
        if (requestAudioFocus()) {
            cancelScheduledPausedSleep()
            moveToNextTrack(true)
        }
    }

    override fun volumeUp() {
        adjustVolume(volumeStep)
    }

    override fun volumeDown() {
        adjustVolume(-volumeStep)
    }

    override fun seekForward() {
        if (requestAudioFocus()) {
            if (playContext.currentPlayer != null) {
                val pos = playContext.currentPlayer?.position ?: 0
                playContext.currentPlayer?.position = pos + 5000
            }
        }
    }

    override fun seekBackward() {
        if (requestAudioFocus()) {
            if (playContext.currentPlayer != null) {
                val pos = playContext.currentPlayer?.position ?: 0
                playContext.currentPlayer?.position = pos - 5000
            }
        }
    }

    override fun seekTo(seconds: Double) {
        if (requestAudioFocus()) {
            playContext.currentPlayer?.position = (seconds * 1000).toInt()
        }
    }

    override val queueCount: Int
        get() = playContext.queueCount

    override val queuePosition: Int
        get() = playContext.currentIndex

    override val volume: Double
        get() {
            if (prefs.getBoolean(Prefs.Key.SOFTWARE_VOLUME, Prefs.Default.SOFTWARE_VOLUME)) {
                return PlayerWrapper.getVolume().toDouble()
            }

            return systemVolume.toDouble()
        }

    override val duration: Double
        get() {
            return (playContext.currentPlayer?.duration?.toDouble() ?: 0.0) / 1000.0
        }

    override val currentTime: Double
        get() {
            return (playContext.currentPlayer?.position?.toDouble() ?: 0.0) / 1000.0
        }

    override var isShuffled: Boolean = false
        private set(value) {
            field = value
        }

    override var isMuted: Boolean = false
        private set(value) {
            field = value
        }

    override var repeatMode = RepeatMode.None
        private set(value) {
            field = value
        }

    override var playbackState = PlaybackState.Stopped
        private set(value) {
            field = value
        }

    override fun toggleShuffle() {
        isShuffled = !isShuffled
        invalidateAndPrefetchNextTrackMetadata()
        notifyEventListeners()
    }

    override fun toggleMute() {
        isMuted = !isMuted
        PlayerWrapper.setMute(isMuted)
        notifyEventListeners()
    }

    override fun toggleRepeatMode() {
        when (repeatMode) {
            RepeatMode.None -> repeatMode = RepeatMode.List
            RepeatMode.List -> repeatMode = RepeatMode.Track
            else -> repeatMode = RepeatMode.None
        }

        this.prefs.edit().putString(REPEAT_MODE_PREF, repeatMode.toString()).apply()
        invalidateAndPrefetchNextTrackMetadata()
        notifyEventListeners()
    }

    override val playingTrack: ITrack
        get() {
            val playing: ITrack? = playContext.currentMetadata
            return playing ?: RemoteTrack(JSONObject())
        }

    override val bufferedTime: Double /* ms -> sec */
        get() {
            if (playContext.currentPlayer != null) {
                val percent = playContext.currentPlayer!!.bufferedPercent.toFloat() / 100.0f
                return (percent * playContext.currentPlayer!!.duration.toFloat() / 1000.0f).toDouble()
            }
            return 0.0
        }

    private fun pauseTransient() {
        if (playbackState !== PlaybackState.Paused) {
            pausedByTransientLoss = true
            setState(PlaybackState.Paused)
            playContext.currentPlayer?.pause()
        }
    }

    private val volumeStep: Float
        get() {
            if (prefs.getBoolean(Prefs.Key.SOFTWARE_VOLUME, Prefs.Default.SOFTWARE_VOLUME)) {
                return 0.1f
            }
            return 1.0f / maxSystemVolume
        }

    private fun adjustVolume(delta: Float) {
        if (isMuted) {
            toggleMute()
        }

        val softwareVolume = prefs.getBoolean(Prefs.Key.SOFTWARE_VOLUME, Prefs.Default.SOFTWARE_VOLUME)
        var current = if (softwareVolume) PlayerWrapper.getVolume() else systemVolume

        current += delta
        if (current > 1.0) current = 1.0f
        if (current < 0.0) current = 0.0f

        if (softwareVolume) {
            PlayerWrapper.setVolume(current)
        }
        else {
            val actual = Math.round(current * maxSystemVolume)
            lastSystemVolume = actual
            audioManager?.setStreamVolume(AudioManager.STREAM_MUSIC, actual, 0)
        }

        notifyEventListeners()
    }

    private val systemVolume: Float
        get() {
            val current = audioManager?.getStreamVolume(AudioManager.STREAM_MUSIC)?.toFloat() ?: 0.0f
            return current / maxSystemVolume
        }

    private val maxSystemVolume: Float
        get() = audioManager?.getStreamMaxVolume(AudioManager.STREAM_MUSIC)?.toFloat() ?: 0.0f

    private fun killAudioFocus() {
        audioManager?.abandonAudioFocus(audioFocusChangeListener)
    }

    private fun requestAudioFocus(): Boolean {
        return audioManager?.requestAudioFocus(
            audioFocusChangeListener,
            AudioManager.STREAM_MUSIC,
            AudioManager.AUDIOFOCUS_GAIN) == AudioManager.AUDIOFOCUS_REQUEST_GRANTED
    }

    private fun moveToPrevTrack() {
        if (playContext.queueCount > 0) {
            loadQueueAndPlay(params!!, resolvePrevIndex(
                playContext.currentIndex, playContext.queueCount))
        }
    }

    private fun moveToNextTrack(userInitiated: Boolean) {
        val index = playContext.currentIndex
        if (!userInitiated && playContext.advanceToNextTrack(onCurrentPlayerStateChanged)) {
            notifyEventListeners()
            prefetchNextTrackMetadata()
        }
        else {
            /* failed! reset by loading as if the user selected the next track
            manually (this will automatically load the current and next tracks */
            val next = resolveNextIndex(index, playContext.queueCount, userInitiated)
            if (next >= 0) {
                loadQueueAndPlay(params!!, next)
            }
            else {
                stop()
            }
        }
    }

    private val onCurrentPlayerStateChanged = { _: PlayerWrapper, state: PlayerWrapper.State ->
        when (state) {
            PlayerWrapper.State.Playing -> {
                setState(PlaybackState.Playing)
                prefetchNextTrackAudio()
                cancelScheduledPausedSleep()
                precacheTrackMetadata(playContext.currentIndex, PRECACHE_METADATA_SIZE)
            }

            PlayerWrapper.State.Buffering -> setState(PlaybackState.Buffering)

            PlayerWrapper.State.Paused -> pause()

            PlayerWrapper.State.Error -> pause()

            PlayerWrapper.State.Finished -> if (playbackState !== PlaybackState.Paused) {
                moveToNextTrack(false)
            }

            else -> { }
        }
    }

    private val onNextPlayerStateChanged = { mpw: PlayerWrapper, state: PlayerWrapper.State ->
        if (state === PlayerWrapper.State.Prepared) {
            if (mpw === playContext.nextPlayer) {
                playContext.notifyNextTrackPrepared()
            }
        }
    }

    private fun setState(state: PlaybackState) {
        if (playbackState !== state) {
            Log.d(TAG, "state = " + state)
            playbackState = state
            notifyEventListeners()
        }
    }

    @Synchronized private fun notifyEventListeners() {
        for (listener in listeners) {
            listener()
        }
    }

    private fun getUri(track: ITrack?): String? {
        if (track != null) {
            val existingUri = track.uri
            if (Strings.notEmpty(existingUri)) {
                return existingUri
            }

            val externalId = track.externalId
            if (Strings.notEmpty(externalId)) {
                val ssl = prefs.getBoolean(Prefs.Key.SSL_ENABLED, Prefs.Default.SSL_ENABLED)
                val protocol = if (ssl) "https" else "http"

                /* transcoding bitrate, if selected by the user */
                var bitrateQueryParam = ""
                val bitrateIndex = prefs.getInt(
                        Prefs.Key.TRANSCODER_BITRATE_INDEX,
                        Prefs.Default.TRANSCODER_BITRATE_INDEX)

                if (bitrateIndex > 0) {
                    val r = Application.instance!!.resources

                    bitrateQueryParam = String.format(
                        Locale.ENGLISH,
                        "?bitrate=%s",
                        r.getStringArray(R.array.transcode_bitrate_array)[bitrateIndex])
                }

                return String.format(
                    Locale.ENGLISH,
                    "%s://%s:%d/audio/external_id/%s%s",
                    protocol,
                    prefs.getString(Prefs.Key.ADDRESS, Prefs.Default.ADDRESS),
                    prefs.getInt(Prefs.Key.AUDIO_PORT, Prefs.Default.AUDIO_PORT),
                    URLEncoder.encode(externalId),
                    bitrateQueryParam)
            }
        }
        return null
    }

    private fun resolvePrevIndex(currentIndex: Int, count: Int): Int {
        if (currentIndex - 1 < 0) {
            if (repeatMode === RepeatMode.List) {
                return count - 1
            }
            return 0
        }
        return currentIndex - 1
    }

    private fun resolveNextIndex(currentIndex: Int, count: Int, userInitiated: Boolean): Int {
        if (isShuffled) { /* our shuffle matches actually random for now. */
            if (count <= 0) {
                return currentIndex
            }

            var r = random.nextInt(count - 1)
            while (r == currentIndex) {
                r = random.nextInt(count - 1)
            }
            return r
        }
        else if (!userInitiated && repeatMode === RepeatMode.Track) {
            return currentIndex
        }
        else {
            if (currentIndex + 1 >= count) {
                return if (repeatMode === RepeatMode.List) 0 else -1
            }
            else {
                return currentIndex + 1
            }
        }
    }

    private fun getMetadataQuery(index: Int): Observable<List<ITrack>>? {
        return playlistQueryFactory.page(index, 1)
    }

    private fun getCurrentAndNextTrackMessages(context: PlaybackContext, queueCount: Int): Observable<List<ITrack>> {
        val tracks = ArrayList<Observable<List<ITrack>>>()

        if (queueCount > 0) {
            context.queueCount = queueCount

            if (trackMetadataCache.containsKey(context.currentIndex)) {
                context.currentMetadata = trackMetadataCache[context.currentIndex]
            }
            else {
                val query = getMetadataQuery(context.currentIndex)
                if (query != null) {
                    tracks.add(query)
                }
            }

            if (queueCount > 1) { /* let's prefetch the next track as well */
                context.nextIndex = resolveNextIndex(context.currentIndex, queueCount, false)

                if (context.nextIndex >= 0) {
                    if (trackMetadataCache.containsKey(context.nextIndex)) {
                        context.nextMetadata = trackMetadataCache[context.nextIndex]
                    }
                    else {
                        val query = getMetadataQuery(context.nextIndex)
                        if (query != null) {
                            tracks.add(query)
                        }
                    }
                }
            }
        }

        return Observable.concat(tracks)
    }

    private fun prefetchNextTrackAudio() {
        if (playContext.nextMetadata != null) {
            val uri = getUri(playContext.nextMetadata)

            if (uri != null) {
                playContext.reset(playContext.nextPlayer)
                playContext.nextPlayer = PlayerWrapper.newInstance()
                playContext.nextPlayer?.setOnStateChangedListener(onNextPlayerStateChanged)
                playContext.nextPlayer?.prefetch(uri, playContext.nextMetadata!!)
            }
        }
    }

    private fun invalidateAndPrefetchNextTrackMetadata() {
        if (playContext.queueCount > 0) {
            if (playContext.nextMetadata != null) {
                playContext.reset(playContext.nextPlayer)
                playContext.nextMetadata = null
                playContext.nextPlayer = null
                playContext.nextIndex = -1
                playContext.currentPlayer?.setNextMediaPlayer(null)
            }

            prefetchNextTrackMetadata()
        }
    }

    private fun prefetchNextTrackMetadata() {
        if (playContext.nextMetadata == null) {
            val originalParams = params
            val nextIndex = resolveNextIndex(playContext.currentIndex, playContext.queueCount, false)

            if (trackMetadataCache.containsKey(nextIndex)) {
                playContext.nextMetadata = trackMetadataCache[nextIndex]
                playContext.nextIndex = nextIndex
                prefetchNextTrackAudio()
            }
            else if (nextIndex >= 0) {
                val currentIndex = playContext.currentIndex
                val query = getMetadataQuery(nextIndex)

                if (query != null) {
                    query
                        .observeOn(AndroidSchedulers.mainThread())
                        .subscribeOn(AndroidSchedulers.mainThread())
                        .subscribe(
                            { track ->
                                if (originalParams === params && playContext.currentIndex == currentIndex) {
                                    if (playContext.nextMetadata == null) {
                                        playContext.nextIndex = nextIndex
                                        playContext.nextMetadata = track.firstOrNull()
                                        prefetchNextTrackAudio()
                                    }
                                }
                            },
                            { error ->
                                Log.e(TAG, "failed to prefetch next track!", error)
                            })
                }
            }
        }
    }

    private fun loadQueueAndPlay(newParams: QueueParams, startIndex: Int) {
        setState(PlaybackState.Buffering)

        cancelScheduledPausedSleep()
        SystemService.wakeup()

        pausedByTransientLoss = false

        val newPlayContext = PlaybackContext()
        playContext.stopPlaybackAndReset()
        playContext = newPlayContext
        playContext.currentIndex = startIndex

        params = newParams

        val countMessage = playlistQueryFactory.count() ?: return

        countMessage
            .observeOn(AndroidSchedulers.mainThread())
            .subscribeOn(AndroidSchedulers.mainThread())
            .concatMap { count -> getCurrentAndNextTrackMessages(playContext, count) }
            .subscribe(
                { track ->
                    if (playContext.currentMetadata == null) {
                        playContext.currentMetadata = track.firstOrNull()
                    }
                    else {
                        playContext.nextMetadata = track.firstOrNull()
                    }
                },
                { error ->
                    Log.e(TAG, "failed to load track to play!", error)
                    setState(PlaybackState.Stopped)
                },
                {
                    if (this.params === newParams && playContext === newPlayContext) {
                        notifyEventListeners()

                        val uri = getUri(playContext.currentMetadata)

                        if (uri != null) {
                            playContext.currentPlayer = PlayerWrapper.newInstance()
                            playContext.currentPlayer?.setOnStateChangedListener(onCurrentPlayerStateChanged)
                            playContext.currentPlayer?.play(uri, playContext.currentMetadata!!)
                        }
                    }
                    else {
                        Log.d(TAG, "onComplete fired, but params/context changed. discarding!")
                    }
                })
    }

    private fun cancelScheduledPausedSleep() {
        SystemService.wakeup()
        handler.removeCallbacks(pauseServiceSleepRunnable)
    }

    private fun schedulePausedSleep() {
        handler.postDelayed(pauseServiceSleepRunnable, PAUSED_SERVICE_SLEEP_DELAY_MS.toLong())
    }

    private fun precacheTrackMetadata(start: Int, count: Int) {
        val originalParams = params
        val query = playlistQueryFactory.page(start, count)

        if (query != null) {
            query.observeOn(AndroidSchedulers.mainThread())
                .subscribeOn(AndroidSchedulers.mainThread())
                .subscribe(
                    { response ->
                        if (originalParams === this.params) {
                            response.forEachIndexed { i, track ->
                                trackMetadataCache.put(start + i, track)
                            }
                        }
                    },
                    {
                        error -> Log.e(TAG, "failed to prefetch track metadata!", error)
                    })
        }
    }

    override val playlistQueryFactory: TrackListSlidingWindow.QueryFactory = object : TrackListSlidingWindow.QueryFactory() {
        override fun count(): Observable<Int>? {
            val params = params
            if (params != null) {
                if (Strings.notEmpty(params.category) && (params.categoryId >= 0)) {
                    return dataProvider.getTrackCountByCategory(
                        params.category ?: "", params.categoryId, params.filter)
                }
                else {
                    return dataProvider.getTrackCount(params.filter)
                }
            }
            return null
        }

        override fun all(): Observable<List<ITrack>>? {
            val params = params
            if (params != null) {
                if (Strings.notEmpty(params.category) && (params.categoryId >= 0)) {
                    return dataProvider.getTracksByCategory(
                        params.category ?: "", params.categoryId, params.filter)
                }
                else {
                    return dataProvider.getTracks(params.filter)
                }
            }
            return null
        }

        override fun page(offset: Int, limit: Int): Observable<List<ITrack>>? {
            val params = params
            if (params != null) {
                if (Strings.notEmpty(params.category) && (params.categoryId >= 0)) {
                    return dataProvider.getTracksByCategory(
                        params.category ?: "", params.categoryId, limit, offset, params.filter)
                }
                else {
                    return dataProvider.getTracks(limit, offset, params.filter)
                }
            }
            return null
        }

        override fun offline(): Boolean {
            return params?.category == Messages.Category.OFFLINE
        }
    }

    init {
        this.audioManager = Application.instance?.getSystemService(Context.AUDIO_SERVICE) as AudioManager
        this.lastSystemVolume = audioManager?.getStreamVolume(AudioManager.STREAM_MUSIC) ?: 0
        this.repeatMode = RepeatMode.from(this.prefs.getString(REPEAT_MODE_PREF, RepeatMode.None.toString())!!)
        this.audioManager = context.getSystemService(Context.AUDIO_SERVICE) as AudioManager
        context.contentResolver.registerContentObserver(Settings.System.CONTENT_URI, true, SettingsContentObserver())
    }

    private val pauseServiceSleepRunnable = object: Runnable {
        override fun run() {
            SystemService.sleep()
        }
    }

    private val audioFocusChangeListener = AudioManager.OnAudioFocusChangeListener { flag: Int ->
        when (flag) {
            AudioManager.AUDIOFOCUS_GAIN,
            AudioManager.AUDIOFOCUS_GAIN_TRANSIENT,
            AudioManager.AUDIOFOCUS_GAIN_TRANSIENT_EXCLUSIVE,
            AudioManager.AUDIOFOCUS_GAIN_TRANSIENT_MAY_DUCK -> {
                PlayerWrapper.unduck()
                if (pausedByTransientLoss) {
                    pausedByTransientLoss = false
                    resume()
                }
            }

            AudioManager.AUDIOFOCUS_LOSS -> {
                killAudioFocus()
                pause()
            }

            AudioManager.AUDIOFOCUS_LOSS_TRANSIENT -> when (playbackState) {
                PlaybackState.Playing,
                PlaybackState.Buffering -> pauseTransient()
                else -> { }
            }

            AudioManager.AUDIOFOCUS_LOSS_TRANSIENT_CAN_DUCK -> PlayerWrapper.duck()
        }
    }

    private inner class SettingsContentObserver : ContentObserver(Handler(Looper.getMainLooper())) {
        override fun deliverSelfNotifications(): Boolean {
            return false
        }

        override fun onChange(selfChange: Boolean) {
            super.onChange(selfChange)

            val currentVolume = audioManager?.getStreamVolume(AudioManager.STREAM_MUSIC) ?: 0
            if (currentVolume != lastSystemVolume) {
                lastSystemVolume = currentVolume
                notifyEventListeners()
            }
        }
    }

    companion object {
        private val TAG = "StreamingPlayback"
        private val REPEAT_MODE_PREF = "streaming_playback_repeat_mode"
        private val PREV_TRACK_GRACE_PERIOD_MILLIS = 3500
        private val MAX_TRACK_METADATA_CACHE_SIZE = 50
        private val PRECACHE_METADATA_SIZE = 10
        private val PAUSED_SERVICE_SLEEP_DELAY_MS = 1000 * 60 * 5 /* 5 minutes */
    }
}
