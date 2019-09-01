package io.casey.musikcube.remote.ui.playqueue.fragment

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import com.simplecityapps.recyclerview_fastscroll.views.FastScrollRecyclerView
import io.casey.musikcube.remote.R
import io.casey.musikcube.remote.service.websocket.model.IMetadataProxy
import io.casey.musikcube.remote.service.websocket.model.ITrack
import io.casey.musikcube.remote.ui.playqueue.adapter.PlayQueueAdapter
import io.casey.musikcube.remote.ui.playqueue.constant.PlayQueue
import io.casey.musikcube.remote.ui.shared.activity.ITitleProvider
import io.casey.musikcube.remote.ui.shared.extension.getLayoutId
import io.casey.musikcube.remote.ui.shared.extension.setupDefaultRecyclerView
import io.casey.musikcube.remote.ui.shared.fragment.BaseFragment
import io.casey.musikcube.remote.ui.shared.mixin.ItemContextMenuMixin
import io.casey.musikcube.remote.ui.shared.mixin.MetadataProxyMixin
import io.casey.musikcube.remote.ui.shared.mixin.PlaybackMixin
import io.casey.musikcube.remote.ui.shared.model.DefaultSlidingWindow
import io.casey.musikcube.remote.ui.shared.model.ITrackListSlidingWindow
import io.casey.musikcube.remote.ui.shared.view.EmptyListView
import io.reactivex.rxkotlin.subscribeBy

class PlayQueueFragment: BaseFragment(), ITitleProvider {
    private var offlineQueue: Boolean = false
    private lateinit var data: MetadataProxyMixin
    private lateinit var playback: PlaybackMixin
    private lateinit var tracks: DefaultSlidingWindow
    private lateinit var adapter: PlayQueueAdapter
    private lateinit var emptyView: EmptyListView

    override val title: String
        get() = getString(R.string.play_queue_title)

    override fun onCreate(savedInstanceState: Bundle?) {
        component.inject(this)
        data = mixin(MetadataProxyMixin())
        playback = mixin(PlaybackMixin(playbackEvents))
        mixin(ItemContextMenuMixin(appCompatActivity, null, this))
        super.onCreate(savedInstanceState)
    }

    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View =
        inflater.inflate(this.getLayoutId(), container, false).apply {
            val queryFactory = playback.service.playlistQueryFactory
            offlineQueue = playback.service.playlistQueryFactory.offline()

            val recyclerView = findViewById<FastScrollRecyclerView>(R.id.recycler_view)
            tracks = DefaultSlidingWindow(recyclerView, data.provider, queryFactory)
            tracks.setInitialPosition(extras.getInt(PlayQueue.Extra.PLAYING_INDEX, -1))
            tracks.setOnMetadataLoadedListener(slidingWindowListener)
            adapter = PlayQueueAdapter(tracks, playback, prefs, adapterListener)

            setupDefaultRecyclerView(recyclerView, adapter)

            emptyView = findViewById(R.id.empty_list_view)
            emptyView.capability = EmptyListView.Capability.OfflineOk
            emptyView.emptyMessage = getString(R.string.play_queue_empty)
            emptyView.alternateView = recyclerView
        }

    override fun onPause() {
        super.onPause()
        this.tracks.pause()
    }

    override fun onResume() {
        this.tracks.resume() /* needs to happen before */
        super.onResume()
        if (offlineQueue) {
            tracks.requery()
        }
    }

    override fun onInitObservables() {
        disposables.add(data.provider.observeState().subscribeBy(
            onNext = { states ->
                if (states.first == IMetadataProxy.State.Connected) {
                    tracks.requery()
                }
                else {
                    emptyView.update(states.first, adapter.itemCount)
                }
            },
            onError = {
            }))
    }

    private val adapterListener = object: PlayQueueAdapter.EventListener {
        override fun onItemClicked(position: Int) =
            playback.service.playAt(position)

        override fun onActionClicked(view: View, value: ITrack) {
            mixin(ItemContextMenuMixin::class.java)?.showForTrack(value, view)
        }
    }

    private val playbackEvents = {
        if (adapter.itemCount == 0) {
            tracks.requery()
        }
        adapter.notifyDataSetChanged()
    }

    private val slidingWindowListener = object : ITrackListSlidingWindow.OnMetadataLoadedListener {
        override fun onReloaded(count: Int) =
                emptyView.update(data.provider.state, count)

        override fun onMetadataLoaded(offset: Int, count: Int) = Unit
    }

    companion object {
        const val TAG = "PlayQueueFragment"

        fun create(playingIndex: Int): PlayQueueFragment =
            PlayQueueFragment().apply {
                arguments = Bundle().apply {
                    putInt(PlayQueue.Extra.PLAYING_INDEX, playingIndex)
                }
            }
    }
}