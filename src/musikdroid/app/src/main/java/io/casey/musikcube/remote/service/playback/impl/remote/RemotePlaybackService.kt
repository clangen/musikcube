package io.casey.musikcube.remote.service.playback.impl.remote

import android.os.Handler
import io.casey.musikcube.remote.Application
import io.casey.musikcube.remote.injection.DaggerServiceComponent
import io.casey.musikcube.remote.service.playback.IPlaybackService
import io.casey.musikcube.remote.service.playback.PlaybackState
import io.casey.musikcube.remote.service.playback.QueryContext
import io.casey.musikcube.remote.service.playback.RepeatMode
import io.casey.musikcube.remote.service.websocket.Messages
import io.casey.musikcube.remote.service.websocket.SocketMessage
import io.casey.musikcube.remote.service.websocket.WebSocketService
import io.casey.musikcube.remote.service.websocket.model.IMetadataProxy
import io.casey.musikcube.remote.service.websocket.model.ITrack
import io.casey.musikcube.remote.service.websocket.model.ITrackListQueryFactory
import io.casey.musikcube.remote.service.websocket.model.impl.remote.RemoteTrack
import io.reactivex.Observable
import org.json.JSONObject
import java.util.*
import javax.inject.Inject

class RemotePlaybackService : IPlaybackService {
    private interface Key {
        companion object {
            const val STATE = "state"
            const val REPEAT_MODE = "repeat_mode"
            const val VOLUME = "volume"
            const val SHUFFLED = "shuffled"
            const val MUTED = "muted"
            const val PLAY_QUEUE_COUNT = "track_count"
            const val PLAY_QUEUE_POSITION = "play_queue_position"
            const val PLAYING_DURATION = "playing_duration"
            const val PLAYING_CURRENT_TIME = "playing_current_time"
            const val PLAYING_TRACK = "playing_track"
        }
    }

    /**
     * an annoying little class that maintains and updates state that estimates
     * the currently playing time. remember, here we're a remote control, so we
     * don't know the exact position of the play head! we update every 5 seconds
     * and estimate.
     */
    private class EstimatedPosition {
        internal var lastTime = 0.0
        internal var pauseTime = 0.0
        internal var trackId: Long = -1
        internal var queryTime: Long = 0

        internal fun get(track: JSONObject?): Double {
            if (track != null && track.optLong(Metadata.Track.ID, -1L) == trackId && trackId != -1L) {
                return if (pauseTime != 0.0) pauseTime else estimatedTime()
            }
            return 0.0
        }

        internal fun update(message: SocketMessage) {
            queryTime = System.nanoTime()
            lastTime = message.getDoubleOption(Messages.Key.PLAYING_CURRENT_TIME, 0.0)
            trackId = message.getLongOption(Messages.Key.ID, -1)
        }

        internal fun pause() {
            pauseTime = estimatedTime()
        }

        internal fun resume() {
            lastTime = pauseTime
            queryTime = System.nanoTime()
            pauseTime = 0.0
        }

        internal fun update(time: Double, id: Long) {
            queryTime = System.nanoTime()
            lastTime = time
            trackId = id

            if (pauseTime != 0.0) {
                pauseTime = time /* ehh... */
            }
        }

        internal fun reset() {
            pauseTime = 0.0
            lastTime = pauseTime
            queryTime = System.nanoTime()
            trackId = -1
        }

        internal fun estimatedTime(): Double {
            val diff = System.nanoTime() - queryTime
            val seconds = diff.toDouble() / NANOSECONDS_PER_SECOND
            return lastTime + seconds
        }
    }

    @Inject lateinit var wss: WebSocketService
    @Inject lateinit var metadataProxy: IMetadataProxy

    private val handler = Handler()
    private val listeners = HashSet<() -> Unit>()
    private val estimatedTime = EstimatedPosition()

    override var state = PlaybackState.Stopped
        private set(value) {
            field = value
        }

    override val currentTime: Double
        get() = estimatedTime.get(track)

    override var repeatMode: RepeatMode = RepeatMode.None
        private set(value) {
            field = value
        }

    override var shuffled: Boolean = false
        private set(value) {
            field = value
        }

    override var muted: Boolean = false
        private set(value) {
            field = value
        }

    override var volume: Double = 0.0
        private set(value) {
            field = value
        }

    override var queueCount: Int = 0
        private set(value) {
            field = value
        }

    override var queuePosition: Int = 0
        private set(value) {
            field = value
        }

    override var duration: Double = 0.0
        private set(value) {
            field = value
        }

    private var track: JSONObject = JSONObject()

    init {
        DaggerServiceComponent.builder()
            .appComponent(Application.appComponent)
            .build().inject(this)

        reset()
    }

    override fun playAll() {
        playAll(0, "")
    }

    override fun playAll(index: Int, filter: String) {
        wss.send(SocketMessage.Builder
            .request(Messages.Request.PlayAllTracks)
            .addOption(Messages.Key.INDEX, index)
            .addOption(Messages.Key.FILTER, filter)
            .build())
    }

    override fun play(category: String, categoryId: Long, index: Int, filter: String) {
        wss.send(SocketMessage.Builder
            .request(Messages.Request.PlayTracksByCategory)
            .addOption(Messages.Key.CATEGORY, category)
            .addOption(Messages.Key.ID, categoryId)
            .addOption(Messages.Key.INDEX, index)
            .addOption(Messages.Key.FILTER, filter)
            .build())
    }

    override fun playFrom(service: IPlaybackService) {
        service.queryContext?.let {qc ->
            val time = service.currentTime
            val index = service.queuePosition

            when (qc.type) {
                Messages.Request.PlaySnapshotTracks -> {
                    wss.send(SocketMessage.Builder
                        .request(Messages.Request.PlaySnapshotTracks)
                        .addOption(Messages.Key.TIME, time)
                        .addOption(Messages.Key.INDEX, index)
                        .build())
                }
                Messages.Request.QueryTracks,
                Messages.Request.QueryTracksByCategory -> {
                    wss.send(SocketMessage.Builder
                        .request(Messages.Request.PlayTracksByCategory)
                        .addOption(Messages.Key.CATEGORY, qc.category)
                        .addOption(Messages.Key.ID, qc.categoryId)
                        .addOption(Messages.Key.FILTER, qc.filter)
                        .addOption(Messages.Key.TIME, time)
                        .addOption(Messages.Key.INDEX, index)
                        .build())
                }
                else -> { }
            }
        }
    }

    override fun prev() {
        wss.send(SocketMessage.Builder.request(Messages.Request.Previous).build())
    }

    override fun pauseOrResume() {
        wss.send(SocketMessage.Builder.request(Messages.Request.PauseOrResume).build())
    }

    override fun pause() {
        if (state != PlaybackState.Paused) {
            pauseOrResume()
        }
    }

    override fun resume() {
        if (state != PlaybackState.Playing) {
            pauseOrResume()
        }
    }

    override fun stop() {
        /* nothing for now */
    }

    override fun next() {
        wss.send(SocketMessage.Builder.request(Messages.Request.Next).build())
    }

    override fun playAt(index: Int) {
        wss.send(SocketMessage
            .Builder.request(Messages.Request.PlayAtIndex)
            .addOption(Messages.Key.INDEX, index)
            .build())
    }

    override fun volumeUp() {
        wss.send(SocketMessage.Builder
            .request(Messages.Request.SetVolume)
            .addOption(Messages.Key.RELATIVE, Messages.Value.UP)
            .build())
    }

    override fun volumeDown() {
        wss.send(SocketMessage.Builder
            .request(Messages.Request.SetVolume)
            .addOption(Messages.Key.RELATIVE, Messages.Value.DOWN)
            .build())
    }

    override fun seekForward() {
        wss.send(SocketMessage.Builder
            .request(Messages.Request.SeekRelative)
            .addOption(Messages.Key.DELTA, 5.0f).build())
    }

    override fun seekBackward() {
        wss.send(SocketMessage.Builder
            .request(Messages.Request.SeekRelative)
            .addOption(Messages.Key.DELTA, -5.0f).build())
    }

    override fun seekTo(seconds: Double) {
        wss.send(SocketMessage.Builder
            .request(Messages.Request.SeekTo)
            .addOption(Messages.Key.POSITION, seconds).build())

        estimatedTime.update(seconds, estimatedTime.trackId)
    }

    @Synchronized override fun connect(listener: () -> Unit) {
        listeners.add(listener)

        if (listeners.size == 1) {
            wss.addClient(client)
            metadataProxy.attach()
            scheduleTimeSyncMessage()
        }
    }

    @Synchronized override fun disconnect(listener: () -> Unit) {
        listeners.remove(listener)

        if (listeners.size == 0) {
            wss.removeClient(client)
            metadataProxy.detach()
            handler.removeCallbacks(syncTimeRunnable)
        }
    }

    override fun toggleShuffle() {
        wss.send(SocketMessage.Builder
            .request(Messages.Request.ToggleShuffle).build())
    }

    override fun toggleMute() {
        wss.send(SocketMessage.Builder
            .request(Messages.Request.ToggleMute).build())
    }

    override fun toggleRepeatMode() {
        wss.send(SocketMessage.Builder
            .request(Messages.Request.ToggleRepeat)
            .build())
    }

    override val bufferedTime: Double
        get() = duration

    override val playingTrack: ITrack
        get() = RemoteTrack(track)

    private fun reset() {
        state = PlaybackState.Stopped
        repeatMode = RepeatMode.None
        muted = false
        shuffled = muted
        volume = 0.0
        queuePosition = 0
        queueCount = queuePosition
        track = JSONObject()
        estimatedTime.reset()
    }

    private fun isPlaybackOverviewMessage(socketMessage: SocketMessage?): Boolean {
        if (socketMessage == null) {
            return false
        }

        val name = socketMessage.name

        return Messages.Broadcast.PlaybackOverviewChanged.matches(name) ||
            Messages.Request.GetPlaybackOverview.matches(name)
    }

    private fun updatePlaybackOverview(message: SocketMessage?): Boolean {
        if (message == null) {
            reset()
            return false
        }

        if (!isPlaybackOverviewMessage(message)) {
            throw IllegalArgumentException("invalid message!")
        }

        state = PlaybackState.from(message.getStringOption(Key.STATE))

        when (state) {
            PlaybackState.Paused -> estimatedTime.pause()
            PlaybackState.Playing -> {
                estimatedTime.resume()
                scheduleTimeSyncMessage()
            }
            else -> { }
        }

        repeatMode = RepeatMode.from(message.getStringOption(Key.REPEAT_MODE))
        shuffled = message.getBooleanOption(Key.SHUFFLED)
        muted = message.getBooleanOption(Key.MUTED)
        volume = message.getDoubleOption(Key.VOLUME)
        queueCount = message.getIntOption(Key.PLAY_QUEUE_COUNT)
        queuePosition = message.getIntOption(Key.PLAY_QUEUE_POSITION)
        duration = message.getDoubleOption(Key.PLAYING_DURATION)
        track = message.getJsonObjectOption(Key.PLAYING_TRACK, JSONObject()) ?: JSONObject()

        estimatedTime.update(
            message.getDoubleOption(Key.PLAYING_CURRENT_TIME, -1.0),
            track.optLong(Metadata.Track.ID, -1))

        notifyStateUpdated()

        return true
    }

    @Synchronized private fun notifyStateUpdated() {
        for (listener in listeners) {
            listener()
        }
    }

    private fun scheduleTimeSyncMessage() {
        handler.removeCallbacks(syncTimeRunnable)

        if (state == PlaybackState.Playing) {
            handler.postDelayed(syncTimeRunnable, SYNC_TIME_INTERVAL_MS)
        }
    }

    private val syncTimeRunnable = object: Runnable {
        override fun run() {
            if (wss.hasClient(client)) {
                wss.send(SocketMessage.Builder
                    .request(Messages.Request.GetCurrentTime).build())
            }
        }
    }

    override val queryContext: QueryContext?
        get() = QueryContext(Messages.Request.QueryPlayQueueTracks)

    override val playlistQueryFactory: ITrackListQueryFactory = object : ITrackListQueryFactory {
        override fun count(): Observable<Int> = metadataProxy.getPlayQueueTracksCount()
        override fun page(offset: Int, limit: Int): Observable<List<ITrack>> = metadataProxy.getPlayQueueTracks(limit, offset)
        override fun offline(): Boolean  = false
    }

    private val client = object : WebSocketService.Client {
        override fun onStateChanged(newState: WebSocketService.State, oldState: WebSocketService.State) {
            if (newState === WebSocketService.State.Connected) {
                wss.send(SocketMessage.Builder.request(
                    Messages.Request.GetPlaybackOverview.toString()).build())
            }
            else if (newState === WebSocketService.State.Disconnected) {
                reset()
                notifyStateUpdated()
            }
        }

        override fun onMessageReceived(message: SocketMessage) {
            if (isPlaybackOverviewMessage(message)) {
                updatePlaybackOverview(message)
            }
            else if (Messages.Request.GetCurrentTime.matches(message.name)) {
                estimatedTime.update(message)
                scheduleTimeSyncMessage()
            }
        }

        override fun onInvalidPassword() {
        }
    }

    companion object {
        private const val NANOSECONDS_PER_SECOND = 1000000000.0
        private const val SYNC_TIME_INTERVAL_MS = 5000L
    }
}
