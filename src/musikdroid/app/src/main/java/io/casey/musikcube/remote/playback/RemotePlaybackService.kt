package io.casey.musikcube.remote.playback

import android.os.Handler
import io.casey.musikcube.remote.Application
import io.casey.musikcube.remote.ui.model.TrackListSlidingWindow
import io.casey.musikcube.remote.websocket.Messages
import io.casey.musikcube.remote.websocket.SocketMessage
import io.casey.musikcube.remote.websocket.WebSocketService
import org.json.JSONObject
import java.util.*
import javax.inject.Inject

class RemotePlaybackService : PlaybackService {
    private interface Key {
        companion object {
            val STATE = "state"
            val REPEAT_MODE = "repeat_mode"
            val VOLUME = "volume"
            val SHUFFLED = "shuffled"
            val MUTED = "muted"
            val PLAY_QUEUE_COUNT = "track_count"
            val PLAY_QUEUE_POSITION = "play_queue_position"
            val PLAYING_DURATION = "playing_duration"
            val PLAYING_CURRENT_TIME = "playing_current_time"
            val PLAYING_TRACK = "playing_track"
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
                if (pauseTime != 0.0) {
                    return pauseTime
                }
                else {
                    return estimatedTime()
                }
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
    private val handler = Handler()
    private val listeners = HashSet<() -> Unit>()
    private val estimatedTime = EstimatedPosition()

    override var playbackState = PlaybackState.Stopped
        private set(value) {
            field = value
        }

    override val currentTime: Double
        get() {
            return estimatedTime.get(track)
        }

    override var repeatMode: RepeatMode = RepeatMode.None
        private set(value) {
            field = value
        }

    override var isShuffled: Boolean = false
        private set(value) {
            field = value
        }

    override var isMuted: Boolean = false
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
        Application.mainComponent.inject(this)
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

    override fun prev() {
        wss.send(SocketMessage.Builder.request(Messages.Request.Previous).build())
    }

    override fun pauseOrResume() {
        wss.send(SocketMessage.Builder.request(Messages.Request.PauseOrResume).build())
    }

    override fun pause() {
        if (playbackState != PlaybackState.Paused) {
            pauseOrResume()
        }
    }

    override fun resume() {
        if (playbackState != PlaybackState.Playing) {
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
            scheduleTimeSyncMessage()
        }
    }

    @Synchronized override fun disconnect(listener: () -> Unit) {
        listeners.remove(listener)

        if (listeners.size == 0) {
            wss.removeClient(client)
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

    override fun getTrackString(key: String, defaultValue: String): String {
        if (track.has(key)) {
            return track.optString(key, defaultValue)
        }
        return defaultValue
    }

    override fun getTrackLong(key: String, defaultValue: Long): Long {
        if (track.has(key)) {
            return track.optLong(key, defaultValue)
        }
        return defaultValue
    }

    private fun reset() {
        playbackState = PlaybackState.Stopped
        repeatMode = RepeatMode.None
        isMuted = false
        isShuffled = isMuted
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

        playbackState = PlaybackState.from(message.getStringOption(Key.STATE))

        when (playbackState) {
            PlaybackState.Paused -> estimatedTime.pause()
            PlaybackState.Playing -> {
                estimatedTime.resume()
                scheduleTimeSyncMessage()
            }
            else -> { }
        }

        repeatMode = RepeatMode.from(message.getStringOption(Key.REPEAT_MODE))
        isShuffled = message.getBooleanOption(Key.SHUFFLED)
        isMuted = message.getBooleanOption(Key.MUTED)
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

        if (playbackState == PlaybackState.Playing) {
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

    override val playlistQueryFactory: TrackListSlidingWindow.QueryFactory = object : TrackListSlidingWindow.QueryFactory() {
        override fun getRequeryMessage(): SocketMessage {
            return SocketMessage.Builder
                .request(Messages.Request.QueryPlayQueueTracks)
                .addOption(Messages.Key.COUNT_ONLY, true)
                .build()
        }

        override fun getPageAroundMessage(offset: Int, limit: Int): SocketMessage {
            return SocketMessage.Builder
                .request(Messages.Request.QueryPlayQueueTracks)
                .addOption(Messages.Key.OFFSET, offset)
                .addOption(Messages.Key.LIMIT, limit)
                .build()
        }

        override fun connectionRequired(): Boolean {
            return true
        }
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
        private val NANOSECONDS_PER_SECOND = 1000000000.0
        private val SYNC_TIME_INTERVAL_MS = 5000L
    }
}
