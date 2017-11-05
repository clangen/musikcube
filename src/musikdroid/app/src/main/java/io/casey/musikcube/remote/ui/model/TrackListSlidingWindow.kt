package io.casey.musikcube.remote.ui.model

import android.support.v7.widget.RecyclerView
import android.util.Log
import android.view.MotionEvent
import android.view.View
import com.pluscubed.recyclerfastscroll.RecyclerFastScroller
import io.casey.musikcube.remote.data.IDataProvider
import io.casey.musikcube.remote.data.ITrack
import io.reactivex.Observable
import io.reactivex.disposables.CompositeDisposable

class TrackListSlidingWindow(private val recyclerView: RecyclerView,
                             private val fastScroller: RecyclerFastScroller,
                             val dataProvider: IDataProvider,
                             private val queryFactory: TrackListSlidingWindow.QueryFactory)
{
    private var scrollState = RecyclerView.SCROLL_STATE_IDLE
    private var fastScrollerActive = false
    private var disposables = CompositeDisposable()
    private var queryOffset = -1
    private var queryLimit = -1
    private var initialPosition = -1
    private var windowSize = DEFAULT_WINDOW_SIZE
    private var loadedListener: OnMetadataLoadedListener? = null
    internal var connected = false

    private class CacheEntry {
        internal var value: ITrack? = null
        internal var dirty: Boolean = false
    }

    private val cache = object : LinkedHashMap<Int, CacheEntry>() {
        override fun removeEldestEntry(eldest: MutableMap.MutableEntry<Int, CacheEntry>): Boolean {
            return size >= MAX_SIZE
        }
    }

    interface OnMetadataLoadedListener {
        fun onMetadataLoaded(offset: Int, count: Int)
        fun onReloaded(count: Int)
    }

    abstract class QueryFactory {
        abstract fun count(): Observable<Int>?
        abstract fun all(): Observable<List<ITrack>>?
        abstract fun page(offset: Int, limit: Int): Observable<List<ITrack>>?
        abstract fun offline(): Boolean
    }

    var count = 0
        set(count) {
            field = count
            invalidateCache()
            cancelMessages()
            notifyAdapterChanged()
            notifyMetadataLoaded(0, 0)
        }

    private val fastScrollerTouch by lazy {
        View.OnTouchListener { _, event ->
            if (event != null) {
                val type = event.actionMasked
                if (type == MotionEvent.ACTION_DOWN) {
                    fastScrollerActive = true
                }
                else if (type == MotionEvent.ACTION_UP) {
                    fastScrollerActive = false
                    requery()
                }
            }
            false
        }
    }

    fun requery() {
        if (queryFactory.offline() || connected) {
            cancelMessages()

            var queried = false
            val countObservable = queryFactory.count()

            if (countObservable != null) {
                countObservable.subscribe(
                { newCount ->
                    count = newCount

                    if (initialPosition != -1) {
                        recyclerView.scrollToPosition(initialPosition)
                        initialPosition = -1
                    }

                    loadedListener?.onReloaded(count)
                },
                { _ ->
                    Log.d("TrackListSlidingWindow", "message send failed, likely canceled")
                })

                queried = true
            }

            if (!queried) {
                count = 0
                loadedListener?.onReloaded(0)
            }
        }
    }

    fun pause() {
        connected = false
        recyclerView.removeOnScrollListener(_scrollListener)
        fastScroller.setOnHandleTouchListener(null)
        disposables.dispose()
        disposables = CompositeDisposable()
    }

    fun resume() {
        disposables.add(dataProvider.observePlayQueue()
            .subscribe({ requery() }, { /* error */ }))

        recyclerView.addOnScrollListener(_scrollListener)
        fastScroller.setOnHandleTouchListener(fastScrollerTouch)
        connected = true
        fastScrollerActive = false
    }

    fun setInitialPosition(initialIndex: Int) {
        initialPosition = initialIndex
    }

    fun setOnMetadataLoadedListener(loadedListener: OnMetadataLoadedListener) {
        this.loadedListener = loadedListener
    }

    fun getTrack(index: Int): ITrack? {
        val track = cache[index]

        if (track?.dirty ?: true) {
            if (scrollState == RecyclerView.SCROLL_STATE_IDLE) {
                getPageAround(index)
            }
        }

        return track?.value
    }

    private fun invalidateCache() {
        for (entry in cache.values) {
            entry.dirty = true
        }
    }

    private fun cancelMessages() {
        queryLimit = -1
        queryOffset = queryLimit
    }

    private fun getPageAround(index: Int) {
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

            pageRequest.subscribe(
                { response ->
                    queryLimit = -1
                    queryOffset = queryLimit

                    var i = 0
                    response.forEach { track ->
                        val entry = CacheEntry()
                        entry.dirty = false
                        entry.value = track
                        cache.put(offset + i++, entry)
                    }

                    notifyAdapterChanged()
                    notifyMetadataLoaded(offset, i)
                },
                { _ ->
                    Log.d("TrackListSlidingWindow", "message send failed, likely canceled")
                })
        }
    }

    private fun notifyAdapterChanged() {
        recyclerView.adapter.notifyDataSetChanged()
    }

    private fun notifyMetadataLoaded(offset: Int, count: Int) {
        loadedListener?.onMetadataLoaded(offset, count)
    }

    private fun scrolling(): Boolean {
        return scrollState != RecyclerView.SCROLL_STATE_IDLE || fastScrollerActive
    }

    private val _scrollListener = object : RecyclerView.OnScrollListener() {
        override fun onScrollStateChanged(recyclerView: RecyclerView?, newState: Int) {
            scrollState = newState
            if (!scrolling()) {
                notifyAdapterChanged()
            }
        }
    }

    companion object {
        private val MAX_SIZE = 150
        val DEFAULT_WINDOW_SIZE = 75
    }
}
