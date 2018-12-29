package io.casey.musikcube.remote.ui.playqueue.activity

import android.content.Context
import android.content.Intent
import android.os.Bundle
import android.view.MenuItem
import android.view.View
import com.simplecityapps.recyclerview_fastscroll.views.FastScrollRecyclerView
import io.casey.musikcube.remote.R
import io.casey.musikcube.remote.service.websocket.model.IDataProvider
import io.casey.musikcube.remote.service.websocket.model.ITrack
import io.casey.musikcube.remote.ui.playqueue.adapter.PlayQueueAdapter
import io.casey.musikcube.remote.ui.shared.activity.BaseActivity
import io.casey.musikcube.remote.ui.shared.extension.addTransportFragment
import io.casey.musikcube.remote.ui.shared.extension.enableUpNavigation
import io.casey.musikcube.remote.ui.shared.extension.setTitleFromIntent
import io.casey.musikcube.remote.ui.shared.extension.setupDefaultRecyclerView
import io.casey.musikcube.remote.ui.shared.mixin.DataProviderMixin
import io.casey.musikcube.remote.ui.shared.mixin.ItemContextMenuMixin
import io.casey.musikcube.remote.ui.shared.mixin.PlaybackMixin
import io.casey.musikcube.remote.ui.shared.model.DefaultSlidingWindow
import io.casey.musikcube.remote.ui.shared.model.ITrackListSlidingWindow
import io.casey.musikcube.remote.ui.shared.view.EmptyListView
import io.reactivex.rxkotlin.subscribeBy

class PlayQueueActivity : BaseActivity() {
    private var offlineQueue: Boolean = false
    private lateinit var data: DataProviderMixin
    private lateinit var playback: PlaybackMixin
    private lateinit var tracks: DefaultSlidingWindow
    private lateinit var adapter: PlayQueueAdapter
    private lateinit var emptyView: EmptyListView

    override fun onCreate(savedInstanceState: Bundle?) {
        component.inject(this)

        data = mixin(DataProviderMixin())
        playback = mixin(PlaybackMixin(playbackEvents))
        mixin(ItemContextMenuMixin(this))

        super.onCreate(savedInstanceState)

        setContentView(R.layout.recycler_view_activity)

        val queryFactory = playback.service.playlistQueryFactory
        offlineQueue = playback.service.playlistQueryFactory.offline()

        val recyclerView = findViewById<FastScrollRecyclerView>(R.id.recycler_view)
        tracks = DefaultSlidingWindow(recyclerView, data.provider, queryFactory)
        tracks.setInitialPosition(intent.getIntExtra(EXTRA_PLAYING_INDEX, -1))
        tracks.setOnMetadataLoadedListener(slidingWindowListener)
        adapter = PlayQueueAdapter(tracks, playback, prefs, adapterListener)

        setupDefaultRecyclerView(recyclerView, adapter)

        emptyView = findViewById(R.id.empty_list_view)
        emptyView.capability = EmptyListView.Capability.OfflineOk
        emptyView.emptyMessage = getString(R.string.play_queue_empty)
        emptyView.alternateView = recyclerView

        setTitleFromIntent(R.string.play_queue_title)
        addTransportFragment()
        enableUpNavigation()
    }

    override fun onPause() {
        super.onPause()
        this.tracks.pause()
    }

    override fun onResume() {
        this.tracks.resume() /* needs to happen before */
        super.onResume()
        initObservers()
        if (offlineQueue) {
            tracks.requery()
        }
    }

    override fun onOptionsItemSelected(item: MenuItem): Boolean {
        val result = super.onOptionsItemSelected(item)

        if (item.itemId == android.R.id.home) {
            overridePendingTransition(R.anim.stay_put, R.anim.slide_down)
        }

        return result
    }

    override fun onBackPressed() {
        super.onBackPressed()
        overridePendingTransition(R.anim.stay_put, R.anim.slide_down)
    }

    override val transitionType: Transition
        get() = Transition.Vertical

    private fun initObservers() {
        disposables.add(data.provider.observeState().subscribeBy(
            onNext = { states ->
                if (states.first == IDataProvider.State.Connected) {
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
        private const val EXTRA_PLAYING_INDEX = "extra_playing_index"

        fun getStartIntent(context: Context, playingIndex: Int): Intent {
            return Intent(context, PlayQueueActivity::class.java)
                .putExtra(EXTRA_PLAYING_INDEX, playingIndex)
        }
    }
}
