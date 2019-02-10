package io.casey.musikcube.remote.ui.tracks.activity

import android.content.Context
import android.content.Intent
import android.os.Bundle
import android.view.Menu
import android.view.MenuItem
import android.view.View
import com.simplecityapps.recyclerview_fastscroll.views.FastScrollRecyclerView
import io.casey.musikcube.remote.R
import io.casey.musikcube.remote.service.websocket.Messages
import io.casey.musikcube.remote.service.websocket.model.IDataProvider
import io.casey.musikcube.remote.service.websocket.model.ITrack
import io.casey.musikcube.remote.service.websocket.model.ITrackListQueryFactory
import io.casey.musikcube.remote.ui.home.activity.MainActivity
import io.casey.musikcube.remote.ui.shared.activity.BaseActivity
import io.casey.musikcube.remote.ui.shared.activity.Filterable
import io.casey.musikcube.remote.ui.shared.extension.*
import io.casey.musikcube.remote.ui.shared.fragment.TransportFragment
import io.casey.musikcube.remote.ui.shared.mixin.DataProviderMixin
import io.casey.musikcube.remote.ui.shared.mixin.ItemContextMenuMixin
import io.casey.musikcube.remote.ui.shared.mixin.PlaybackMixin
import io.casey.musikcube.remote.ui.shared.model.DefaultSlidingWindow
import io.casey.musikcube.remote.ui.shared.model.ITrackListSlidingWindow
import io.casey.musikcube.remote.ui.shared.view.EmptyListView
import io.casey.musikcube.remote.ui.shared.view.EmptyListView.Capability
import io.casey.musikcube.remote.ui.tracks.adapter.TrackListAdapter
import io.casey.musikcube.remote.ui.tracks.constant.Track
import io.casey.musikcube.remote.util.Debouncer
import io.casey.musikcube.remote.util.Strings
import io.reactivex.Observable
import io.reactivex.rxkotlin.subscribeBy

class TrackListActivity : BaseActivity(), Filterable {
    private lateinit var tracks: DefaultSlidingWindow
    private lateinit var emptyView: EmptyListView
    private lateinit var transport: TransportFragment
    private lateinit var adapter: TrackListAdapter

    private lateinit var data: DataProviderMixin
    private lateinit var playback: PlaybackMixin

    private var categoryType: String = ""
    private var categoryId: Long = 0
    private var categoryValue: String = ""
    private var lastFilter = ""

    override fun onCreate(savedInstanceState: Bundle?) {
        component.inject(this)
        data = mixin(DataProviderMixin())
        playback = mixin(PlaybackMixin())

        super.onCreate(savedInstanceState)

        val intent = intent
        categoryType = intent.getStringExtra(Track.Extra.CATEGORY_TYPE) ?: ""
        categoryId = intent.getLongExtra(Track.Extra.SELECTED_ID, 0)
        categoryValue = intent.getStringExtra(Track.Extra.CATEGORY_VALUE) ?: ""
        val titleId = intent.getIntExtra(Track.Extra.TITLE_ID, R.string.songs_title)

        mixin(ItemContextMenuMixin(this, menuListener))

        setContentView(R.layout.recycler_view_activity)

        setTitleFromIntent(titleId)
        enableUpNavigation()

        val queryFactory = createCategoryQueryFactory(categoryType, categoryId)
        val recyclerView = findViewById<FastScrollRecyclerView>(R.id.recycler_view)

        tracks = DefaultSlidingWindow(recyclerView, data.provider, queryFactory)
        adapter = TrackListAdapter(tracks, eventListener, playback, prefs)

        setupDefaultRecyclerView(recyclerView, adapter)

        emptyView = findViewById(R.id.empty_list_view)

        emptyView.let {
            it.capability = if (isOfflineTracks) Capability.OfflineOk else Capability.OnlineOnly
            it.emptyMessage = emptyMessage
            it.alternateView = recyclerView
        }

        tracks.setOnMetadataLoadedListener(slidingWindowListener)

        transport = addTransportFragment {
            adapter.notifyDataSetChanged()
        }
    }

    override fun onCreateOptionsMenu(menu: Menu): Boolean {
        if (Messages.Category.PLAYLISTS == categoryType) {
            menuInflater.inflate(R.menu.view_playlist_menu, menu)
        }
        else {
            initSearchMenu(menu, this)
        }
        return true
    }

    override fun onOptionsItemSelected(item: MenuItem): Boolean {
        if (item.itemId == R.id.action_edit) {
            startActivityForResult(EditPlaylistActivity.getStartIntent(
                this, categoryValue, categoryId), Track.RequestCode.EDIT_PLAYLIST)
        }
        return super.onOptionsItemSelected(item)
    }

    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        if (requestCode == Track.RequestCode.EDIT_PLAYLIST && resultCode == RESULT_OK && data != null) {
            val playlistName = data.getStringExtra(EditPlaylistActivity.EXTRA_PLAYLIST_NAME) ?: ""
            val playlistId = data.getLongExtra(EditPlaylistActivity.EXTRA_PLAYLIST_ID, -1L)

            if (categoryType != Messages.Category.PLAYLISTS || playlistId != this.categoryId) {
                showSnackbar(
                    getString(R.string.playlist_edit_save_success, playlistName),
                    buttonText = getString(R.string.button_view),
                    buttonCb = {
                        startActivity(TrackListActivity.getStartIntent(
                                this@TrackListActivity, Messages.Category.PLAYLISTS, playlistId, playlistName))
                    })
            }
        }
        super.onActivityResult(requestCode, resultCode, data)
    }

    override fun onPause() {
        super.onPause()
        tracks.pause()
    }

    override fun onResume() {
        tracks.resume() /* needs to happen before */
        super.onResume()
        initObservers()
        requeryIfViewingOfflineCache()
    }

    override fun setFilter(filter: String) {
        lastFilter = filter
        filterDebouncer.call()
    }

    private val eventListener = object: TrackListAdapter.EventListener {
        override fun onItemClick(view: View, track: ITrack, position: Int) {
            if (isValidCategory(categoryType, categoryId)) {
                playback.service.play(categoryType, categoryId, position, lastFilter)
            }
            else {
                playback.service.playAll(position, lastFilter)
            }

            startActivity(MainActivity.getStartIntent(this@TrackListActivity))
            finish()
        }

        override fun onActionItemClick(view: View, track: ITrack, position: Int) {
            val mixin = mixin(ItemContextMenuMixin::class.java)!!
            if (categoryType == Messages.Category.PLAYLISTS) {
                mixin.showForPlaylistTrack(track, position, categoryId, categoryValue, view)
            }
            else {
                mixin.showForTrack(track, view)
            }
        }
    }

    private fun initObservers() {
        disposables.add(data.provider.observeState().subscribeBy(
            onNext = { states ->
                val shouldRequery =
                    states.first === IDataProvider.State.Connected ||
                    (states.first === IDataProvider.State.Disconnected && isOfflineTracks)

                if (shouldRequery) {
                    filterDebouncer.cancel()
                    tracks.requery()
                }
                else {
                    emptyView.update(states.first, adapter.itemCount)
                }
            },
            onError = {
            }))
    }

    private val filterDebouncer = object : Debouncer<String>(350) {
        override fun onDebounced(last: String?) {
            if (!paused) {
                tracks.requery()
            }
        }
    }

    private val emptyMessage: String
        get() {
            if (isOfflineTracks) {
                return getString(R.string.empty_no_offline_tracks_message)
            }

            return getString(R.string.empty_no_items_format, getString(R.string.browse_type_tracks))
        }

    private val isOfflineTracks: Boolean
        get() = Messages.Category.OFFLINE == categoryType

    private fun requeryIfViewingOfflineCache() {
        if (isOfflineTracks) {
            tracks.requery()
        }
    }

    private fun createCategoryQueryFactory(categoryType: String?, categoryId: Long): ITrackListQueryFactory {
        if (isValidCategory(categoryType, categoryId)) {
            /* tracks for a specified category (album, artists, genres, etc */
            return object : ITrackListQueryFactory {
               override fun count(): Observable<Int> =
                   data.provider.getTrackCountByCategory(categoryType ?: "", categoryId, lastFilter)

                override fun page(offset: Int, limit: Int): Observable<List<ITrack>> =
                    data.provider.getTracksByCategory(categoryType ?: "", categoryId, limit, offset, lastFilter)

                override fun offline(): Boolean =
                    Messages.Category.OFFLINE == categoryType
            }
        }
        else {
            /* all tracks */
            return object : ITrackListQueryFactory {
                override fun count(): Observable<Int> =
                    data.provider.getTrackCount(lastFilter)

                override fun page(offset: Int, limit: Int): Observable<List<ITrack>> =
                    data.provider.getTracks(limit, offset, lastFilter)

                override fun offline(): Boolean =
                    Messages.Category.OFFLINE == categoryType
            }
        }
    }

    private val slidingWindowListener = object : ITrackListSlidingWindow.OnMetadataLoadedListener {
        override fun onReloaded(count: Int) =
            emptyView.update(data.provider.state, count)

        override fun onMetadataLoaded(offset: Int, count: Int) {}
    }

    private val menuListener: ItemContextMenuMixin.EventListener?
        get() {
            if (categoryType == Messages.Category.PLAYLISTS) {
                return object: ItemContextMenuMixin.EventListener () {
                    override fun onPlaylistUpdated(id: Long, name: String) {
                        tracks.requery()
                    }
                }
            }

            return null
        }

    companion object {
        fun getStartIntent(context: Context, type: String, id: Long): Intent =
            getStartIntent(context, type, id, "")

        fun getOfflineStartIntent(context: Context): Intent =
            getStartIntent(context, Messages.Category.OFFLINE, 0).apply {
                putExtra(Track.Extra.TITLE_ID, R.string.offline_tracks_title)
            }

        fun getStartIntent(context: Context, type: String, id: Long, categoryValue: String): Intent =
            Intent(context, TrackListActivity::class.java).apply {
                putExtra(Track.Extra.CATEGORY_TYPE, type)
                putExtra(Track.Extra.SELECTED_ID, id)
                putExtra(Track.Extra.CATEGORY_VALUE, categoryValue)

                if (Strings.notEmpty(categoryValue)) {
                    putExtra(
                        EXTRA_ACTIVITY_TITLE,
                        context.getString(R.string.songs_from_category, categoryValue))
                }
            }

        fun getStartIntent(context: Context): Intent =
            Intent(context, TrackListActivity::class.java)

        private fun isValidCategory(categoryType: String?, categoryId: Long): Boolean =
            categoryType != null && categoryType.isNotEmpty() && categoryId != -1L
    }
}
