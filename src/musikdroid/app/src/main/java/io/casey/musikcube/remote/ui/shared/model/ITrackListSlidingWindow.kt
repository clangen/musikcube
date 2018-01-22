package io.casey.musikcube.remote.ui.shared.model

import io.casey.musikcube.remote.service.websocket.model.ITrack

interface ITrackListSlidingWindow {
    interface OnMetadataLoadedListener {
        fun onMetadataLoaded(offset: Int, count: Int)
        fun onReloaded(count: Int)
    }

    val count: Int
    fun requery()
    fun pause()
    fun resume()
    fun getTrack(index: Int): ITrack?
    fun setOnMetadataLoadedListener(loadedListener: OnMetadataLoadedListener)
}