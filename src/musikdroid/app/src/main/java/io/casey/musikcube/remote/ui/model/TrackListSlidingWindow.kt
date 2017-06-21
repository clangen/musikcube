package io.casey.musikcube.remote.ui.model

import android.support.v7.widget.RecyclerView
import android.view.MotionEvent
import android.view.View
import com.pluscubed.recyclerfastscroll.RecyclerFastScroller
import io.casey.musikcube.remote.websocket.Messages
import io.casey.musikcube.remote.websocket.SocketMessage
import io.casey.musikcube.remote.websocket.WebSocketService
import org.json.JSONObject
import java.util.*

class TrackListSlidingWindow(private val recyclerView: RecyclerView,
                             private val fastScroller: RecyclerFastScroller,
                             private val wss: WebSocketService,
                             private val queryFactory: TrackListSlidingWindow.QueryFactory) {
    private var scrollState = RecyclerView.SCROLL_STATE_IDLE
    private var fastScrollerActive = false
    private var queryOffset = -1
    private var queryLimit = -1
    private var initialPosition = -1
    private var windowSize = DEFAULT_WINDOW_SIZE
    private var loadedListener: OnMetadataLoadedListener? = null
    internal var connected = false

    private class CacheEntry {
        internal var value: JSONObject? = null
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
        abstract fun getRequeryMessage(): SocketMessage?
        abstract fun getPageAroundMessage(offset: Int, limit: Int): SocketMessage?

        open fun connectionRequired(): Boolean {
            return false
        }
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
        val connectionRequired = queryFactory.connectionRequired()

        if (!connectionRequired || connected) {
            cancelMessages()

            var queried = false
            val message = queryFactory.getRequeryMessage()

            if (message != null) {
                wss.send(message, _client) { response: SocketMessage ->
                    count = response.getIntOption(Messages.Key.COUNT, 0)

                    if (initialPosition != -1) {
                        recyclerView.scrollToPosition(initialPosition)
                        initialPosition = -1
                    }

                    loadedListener?.onReloaded(count)
                }

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
        wss.removeClient(_client)
        recyclerView.removeOnScrollListener(_scrollListener)
        fastScroller.setOnHandleTouchListener(null)
    }

    fun resume() {
        recyclerView.addOnScrollListener(_scrollListener)
        fastScroller.setOnHandleTouchListener(fastScrollerTouch)
        wss.addClient(_client)
        connected = true
        fastScrollerActive = false
    }

    fun setInitialPosition(initialIndex: Int) {
        initialPosition = initialIndex
    }

    fun setOnMetadataLoadedListener(loadedListener: OnMetadataLoadedListener) {
        this.loadedListener = loadedListener
    }

    fun setWindowSize(windowSize: Int) {
        this.windowSize = windowSize
    }

    fun getTrack(index: Int): JSONObject? {
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
        wss.cancelMessages(_client)
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

        val request = queryFactory.getPageAroundMessage(offset, limit)

        if (request != null) {
            cancelMessages()

            queryOffset = offset
            queryLimit = limit

            wss.send(request, _client) { response: SocketMessage ->
                queryLimit = -1
                queryOffset = queryLimit

                val data = response.getJsonArrayOption(Messages.Key.DATA)
                val responseOffset = response.getIntOption(Messages.Key.OFFSET)
                if (data != null) {
                    for (i in 0..data.length() - 1) {
                        val track = data.optJSONObject(i)
                        if (track != null) {
                            val entry = CacheEntry()
                            entry.dirty = false
                            entry.value = track
                            cache.put(responseOffset + i, entry)
                        }
                    }

                    notifyAdapterChanged()
                    notifyMetadataLoaded(responseOffset, data.length())
                }
            }
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

    private val _client = object : WebSocketService.Client {
        override fun onStateChanged(newState: WebSocketService.State, oldState: WebSocketService.State) {

        }

        override fun onMessageReceived(message: SocketMessage) {
            if (message.type == SocketMessage.Type.Broadcast) {
                if (Messages.Broadcast.PlayQueueChanged.matches(message.name)) {
                    requery()
                }
            }
        }

        override fun onInvalidPassword() {}
    }

    companion object {
        private val MAX_SIZE = 150
        val DEFAULT_WINDOW_SIZE = 75
    }
}
