package io.casey.musikcube.remote.service.playback

import io.casey.musikcube.remote.service.websocket.model.ITrack
import io.casey.musikcube.remote.ui.shared.model.TrackListSlidingWindow

interface IPlaybackService {
    fun connect(listener: () -> Unit)
    fun disconnect(listener: () -> Unit)

    fun playAll()
    fun playAll(index: Int, filter: String)
    fun play(category: String, categoryId: Long, index: Int, filter: String)
    fun playAt(index: Int)

    fun pauseOrResume()
    fun pause()
    fun resume()
    fun prev()
    fun next()
    fun stop()

    fun volumeUp()
    fun volumeDown()

    fun seekForward()
    fun seekBackward()
    fun seekTo(seconds: Double)

    val queueCount: Int
    val queuePosition: Int

    val volume: Double
    val duration: Double
    val currentTime: Double
    val bufferedTime: Double

    val state: PlaybackState

    fun toggleShuffle()
    val shuffled: Boolean

    fun toggleMute()
    val muted: Boolean

    fun toggleRepeatMode()
    val repeatMode: RepeatMode

    val playlistQueryFactory: TrackListSlidingWindow.QueryFactory

    val playingTrack: ITrack
}
