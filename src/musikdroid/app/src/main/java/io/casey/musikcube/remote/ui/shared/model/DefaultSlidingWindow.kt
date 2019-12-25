package io.casey.musikcube.remote.ui.shared.model

import android.util.Log
import com.simplecityapps.recyclerview_fastscroll.views.FastScrollRecyclerView
import io.casey.musikcube.remote.service.websocket.model.IMetadataProxy
import io.casey.musikcube.remote.service.websocket.model.ITrack
import io.casey.musikcube.remote.service.websocket.model.ITrackListQueryFactory
import io.reactivex.rxkotlin.subscribeBy

class DefaultSlidingWindow(
        private val recyclerView: FastScrollRecyclerView,
        metadataProxy: IMetadataProxy,
        private val queryFactory: ITrackListQueryFactory)
            : BaseSlidingWindow(recyclerView, metadataProxy)
{
    private var queryOffset = -1
    private var queryLimit = -1
    private var initialPosition = -1
    private var windowSize = DEFAULT_WINDOW_SIZE

    private val cache = object : LinkedHashMap<Int, CacheEntry>() {
        override fun removeEldestEntry(eldest: MutableMap.MutableEntry<Int, CacheEntry>): Boolean = size >= MAX_SIZE
    }

    override fun requery() {
        if (queryFactory.offline() || connected) {
            cancelMessages()

            var queried = false
            val countObservable = queryFactory.count()

            if (countObservable != null) {
                @Suppress countObservable.subscribeBy(
                    onNext = { newCount ->
                        count = newCount

                        if (initialPosition != -1) {
                            recyclerView.scrollToPosition(initialPosition)
                            initialPosition = -1
                        }

                        loadedListener?.onReloaded(count)
                    },
                    onError = { _ ->
                        Log.d("DefaultSlidingWindow", "message send failed, likely canceled")
                    })

                queried = true
            }

            if (!queried) {
                count = 0
                loadedListener?.onReloaded(0)
            }
        }
    }

    override fun getTrack(index: Int): ITrack? {
        val track = cache[index]

        if (track == null || track.dirty) {
            if (!scrolling()) {
                getPageAround(index)
            }
        }

        return track?.value
    }

    fun setInitialPosition(initialIndex: Int) {
        initialPosition = initialIndex
    }

    override fun invalidate() {
        cancelMessages()
        for (entry in cache.values) {
            entry.dirty = true
        }
    }

    private fun cancelMessages() {
        queryLimit = -1
        queryOffset = queryLimit
    }

    override fun getPageAround(index: Int) {
        if (!connected || scrolling()) {
            return
        }

        if (index >= queryOffset && index <= queryOffset + queryLimit) {
            return  /* already in flight */
        }

        val offset = Math.max(0, index - 10) /* snag a couple before */
        val limit = windowSize

        val pageRequest = queryFactory.page(offset, limit)

        if (pageRequest != null) {
            cancelMessages()

            queryOffset = offset
            queryLimit = limit

            @Suppress pageRequest.subscribeBy(
                onNext = { response ->
                    queryLimit = -1
                    queryOffset = queryLimit

                    var i = 0
                    response.forEach { track ->
                        val entry = CacheEntry()
                        entry.dirty = false
                        entry.value = track
                        cache[offset + i++] = entry
                    }

                    notifyAdapterChanged()
                    notifyMetadataLoaded(offset, i)
                },
                onError = { _ ->
                    Log.d("DefaultSlidingWindow", "message send failed, likely canceled")
                })
        }
    }

    companion object {
        private const val MAX_SIZE = 150
        private const val DEFAULT_WINDOW_SIZE = 75
    }
}
