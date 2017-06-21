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

class TrackListSlidingWindow(private val _recyclerView: RecyclerView,
                             private val _fastScroller: RecyclerFastScroller,
                             private val _wss: WebSocketService,
                             private val _queryFactory: TrackListSlidingWindow.QueryFactory?) {
    private var _scrollState = RecyclerView.SCROLL_STATE_IDLE
    private var _fastScrollActive = false
    private var _queryOffset = -1
    private var _queryLimit = -1
    private var _initialPosition = -1
    private var _windowSize = DEFAULT_WINDOW_SIZE
    private var _loadedListener: OnMetadataLoadedListener? = null
    internal var _connected = false

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

    private val _fastScrollerTouch by lazy {
        View.OnTouchListener { _, event ->
            if (event != null) {
                val type = event.actionMasked
                if (type == MotionEvent.ACTION_DOWN) {
                    _fastScrollActive = true
                }
                else if (type == MotionEvent.ACTION_UP) {
                    _fastScrollActive = false
                    requery()
                }
            }
            false
        }
    }

    fun requery() {
        val connectionRequired = _queryFactory != null && _queryFactory.connectionRequired()

        if (!connectionRequired || _connected) {
            cancelMessages()

            var queried = false

            if (_queryFactory != null) {
                val message = _queryFactory.getRequeryMessage()

                if (message != null) {
                    _wss.send(message, _client) { response: SocketMessage ->
                        count = response.getIntOption(Messages.Key.COUNT, 0)

                        if (_initialPosition != -1) {
                            _recyclerView.scrollToPosition(_initialPosition)
                            _initialPosition = -1
                        }

                        _loadedListener?.onReloaded(count)
                    }

                    queried = true
                }
            }

            if (!queried) {
                count = 0
            }
        }
    }

    fun pause() {
        _connected = false
        _wss.removeClient(_client)
        _recyclerView.removeOnScrollListener(_scrollListener)
        _fastScroller.setOnHandleTouchListener(null)
    }

    fun resume() {
        _recyclerView.addOnScrollListener(_scrollListener)
        _fastScroller.setOnHandleTouchListener(_fastScrollerTouch)
        _wss.addClient(_client)
        _connected = true
        _fastScrollActive = false
    }

    fun setInitialPosition(initialIndex: Int) {
        _initialPosition = initialIndex
    }

    fun setOnMetadataLoadedListener(loadedListener: OnMetadataLoadedListener) {
        _loadedListener = loadedListener
    }

    fun setWindowSize(windowSize: Int) {
        _windowSize = windowSize
    }

    fun getTrack(index: Int): JSONObject? {
        val track = cache[index]

        if (track?.dirty ?: true) {
            if (_scrollState == RecyclerView.SCROLL_STATE_IDLE) {
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
        _queryLimit = -1
        _queryOffset = _queryLimit
        _wss.cancelMessages(_client)
    }

    private fun getPageAround(index: Int) {
        if (!_connected || scrolling()) {
            return
        }

        if (index >= _queryOffset && index <= _queryOffset + _queryLimit) {
            return  /* already in flight */
        }

        val offset = Math.max(0, index - 10) /* snag a couple before */
        val limit = _windowSize

        val request = _queryFactory!!.getPageAroundMessage(offset, limit)

        if (request != null) {
            cancelMessages()

            _queryOffset = offset
            _queryLimit = limit

            _wss.send(request, _client) { response: SocketMessage ->
                _queryLimit = -1
                _queryOffset = _queryLimit

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
        _recyclerView.adapter.notifyDataSetChanged()
    }

    private fun notifyMetadataLoaded(offset: Int, count: Int) {
        _loadedListener?.onMetadataLoaded(offset, count)
    }

    private fun scrolling(): Boolean {
        return _scrollState != RecyclerView.SCROLL_STATE_IDLE || _fastScrollActive
    }

    private val _scrollListener = object : RecyclerView.OnScrollListener() {
        override fun onScrollStateChanged(recyclerView: RecyclerView?, newState: Int) {
            _scrollState = newState
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
