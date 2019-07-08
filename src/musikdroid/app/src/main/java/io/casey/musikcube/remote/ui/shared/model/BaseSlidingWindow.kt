package io.casey.musikcube.remote.ui.shared.model

import androidx.recyclerview.widget.RecyclerView
import com.simplecityapps.recyclerview_fastscroll.interfaces.OnFastScrollStateChangeListener
import com.simplecityapps.recyclerview_fastscroll.views.FastScrollRecyclerView
import io.casey.musikcube.remote.service.websocket.model.IMetadataProxy
import io.casey.musikcube.remote.service.websocket.model.ITrack
import io.casey.musikcube.remote.util.Debouncer
import io.reactivex.disposables.CompositeDisposable

abstract class BaseSlidingWindow(
        private val recyclerView: FastScrollRecyclerView,
        private val metadataProxy: IMetadataProxy) : ITrackListSlidingWindow
{
    private var scrollState = RecyclerView.SCROLL_STATE_IDLE
    private var fastScrollerActive = false

    protected var disposables = CompositeDisposable()
    protected var loadedListener: ITrackListSlidingWindow.OnMetadataLoadedListener? = null
    protected var connected = false

    protected class CacheEntry {
        internal var value: ITrack? = null
        internal var dirty: Boolean = false
    }

    final override var count = 0
        protected set(count) {
            field = count
            invalidate()
            notifyAdapterChanged()
            notifyMetadataLoaded(0, 0)
        }

    override fun pause() {
        connected = false
        recyclerView.removeOnScrollListener(recyclerViewScrollListener)
        disposables.dispose()
        disposables = CompositeDisposable()
    }

    override fun resume() {
        disposables.add(metadataProxy.observePlayQueue()
            .subscribe({ requery() }, { /* error */ }))

        recyclerView.setOnFastScrollStateChangeListener(fastScrollStateChangeListener)
        recyclerView.addOnScrollListener(recyclerViewScrollListener)
        connected = true
        fastScrollerActive = false
    }

    override fun setOnMetadataLoadedListener(loadedListener: ITrackListSlidingWindow.OnMetadataLoadedListener) {
        this.loadedListener = loadedListener
    }

    protected abstract fun invalidate()
    protected abstract fun getPageAround(index: Int)

    protected fun notifyAdapterChanged() =
        adapterChangedDebouncer.call()

    protected fun notifyMetadataLoaded(offset: Int, count: Int) =
        loadedListener?.onMetadataLoaded(offset, count)

    protected fun scrolling(): Boolean =
        scrollState != RecyclerView.SCROLL_STATE_IDLE || fastScrollerActive

    private val recyclerViewScrollListener = object : RecyclerView.OnScrollListener() {
        override fun onScrollStateChanged(recyclerView: RecyclerView, newState: Int) {
            scrollState = newState
            if (!scrolling()) {
                notifyAdapterChanged()
            }
        }
    }

    private val fastScrollStateChangeListener = object: OnFastScrollStateChangeListener {
        override fun onFastScrollStop() {
            fastScrollerActive = false
            requery()
        }

        override fun onFastScrollStart() {
            fastScrollerActive = true
        }
    }

    private val adapterChangedDebouncer = object : Debouncer<String>(50) {
        override fun onDebounced(last: String?) {
            recyclerView.adapter?.notifyDataSetChanged()
        }
    }
}
