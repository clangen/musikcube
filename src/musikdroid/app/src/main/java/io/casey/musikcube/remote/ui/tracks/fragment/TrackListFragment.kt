package io.casey.musikcube.remote.ui.tracks.fragment

import android.content.Context
import android.content.Intent
import android.os.Bundle
import android.view.*
import androidx.appcompat.app.AppCompatActivity
import com.simplecityapps.recyclerview_fastscroll.views.FastScrollRecyclerView
import io.casey.musikcube.remote.R
import io.casey.musikcube.remote.service.playback.impl.remote.Metadata
import io.casey.musikcube.remote.service.websocket.model.IMetadataProxy
import io.casey.musikcube.remote.service.websocket.model.ITrack
import io.casey.musikcube.remote.service.websocket.model.ITrackListQueryFactory
import io.casey.musikcube.remote.ui.shared.activity.IFilterable
import io.casey.musikcube.remote.ui.shared.activity.IMenuProvider
import io.casey.musikcube.remote.ui.shared.activity.ITitleProvider
import io.casey.musikcube.remote.ui.shared.activity.ITransportObserver
import io.casey.musikcube.remote.ui.shared.constant.Shared
import io.casey.musikcube.remote.ui.shared.extension.*
import io.casey.musikcube.remote.ui.shared.fragment.BaseFragment
import io.casey.musikcube.remote.ui.shared.mixin.ItemContextMenuMixin
import io.casey.musikcube.remote.ui.shared.mixin.MetadataProxyMixin
import io.casey.musikcube.remote.ui.shared.mixin.PlaybackMixin
import io.casey.musikcube.remote.ui.shared.model.DefaultSlidingWindow
import io.casey.musikcube.remote.ui.shared.model.ITrackListSlidingWindow
import io.casey.musikcube.remote.ui.shared.view.EmptyListView
import io.casey.musikcube.remote.ui.tracks.activity.EditPlaylistActivity
import io.casey.musikcube.remote.ui.tracks.activity.TrackListActivity
import io.casey.musikcube.remote.ui.tracks.adapter.TrackListAdapter
import io.casey.musikcube.remote.ui.tracks.constant.Track
import io.casey.musikcube.remote.util.Debouncer
import io.reactivex.Observable
import io.reactivex.rxkotlin.subscribeBy

class TrackListFragment: BaseFragment(), IFilterable, ITitleProvider, ITransportObserver, IMenuProvider {
    private lateinit var tracks: DefaultSlidingWindow
    private lateinit var emptyView: EmptyListView
    private lateinit var adapter: TrackListAdapter
    private lateinit var queryFactory: ITrackListQueryFactory

    private lateinit var data: MetadataProxyMixin
    private lateinit var playback: PlaybackMixin

    private var categoryType: String = ""
    private var categoryId: Long = 0
    private var titleId = R.string.songs_title
    private var categoryValue: String = ""
    private var lastFilter = ""

    override fun onCreate(savedInstanceState: Bundle?) {
        component.inject(this)
        data = mixin(MetadataProxyMixin())
        playback = mixin(PlaybackMixin())

        super.onCreate(savedInstanceState)

        extras.run {
            categoryType = getString(Track.Extra.CATEGORY_TYPE, "")
            categoryId = getLong(Track.Extra.SELECTED_ID, 0)
            categoryValue = getString(Track.Extra.CATEGORY_VALUE, "")
            titleId = getInt(Track.Extra.TITLE_ID, titleId)
        }

        /* needs to come after we extract categoryType -- otherwise menuListener
        may get resolved to `null` */
        mixin(ItemContextMenuMixin(appCompatActivity, menuListener, this))

        queryFactory = createCategoryQueryFactory(categoryType, categoryId)
    }

    override fun onPause() {
        super.onPause()
        tracks.pause()
    }

    override fun onResume() {
        tracks.resume() /* needs to happen first */
        super.onResume()
        requeryIfViewingOfflineCache()
    }

    override fun onInitObservables() {
        disposables.add(data.provider.observeState().subscribeBy(
            onNext = { states ->
                val shouldRequery =
                    states.first === IMetadataProxy.State.Connected ||
                    (states.first === IMetadataProxy.State.Disconnected && isOfflineTracks)

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

    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View =
        inflater.inflate(this.getLayoutId(), container, false).apply {
            val recyclerView = findViewById<FastScrollRecyclerView>(R.id.recycler_view)

            tracks = DefaultSlidingWindow(recyclerView, data.provider, queryFactory)
            adapter = TrackListAdapter(tracks, eventListener, playback, prefs)

            setupDefaultRecyclerView(recyclerView, adapter)

            emptyView = findViewById(R.id.empty_list_view)

            emptyView.let {
                it.capability = if (isOfflineTracks) EmptyListView.Capability.OfflineOk else EmptyListView.Capability.OnlineOnly
                it.emptyMessage = emptyMessage
                it.alternateView = recyclerView
            }

            tracks.setOnMetadataLoadedListener(slidingWindowListener)
        }

    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        if (requestCode == Track.RequestCode.EDIT_PLAYLIST && resultCode == AppCompatActivity.RESULT_OK && data != null) {
            val playlistName = data.getStringExtra(EditPlaylistActivity.EXTRA_PLAYLIST_NAME) ?: ""
            val playlistId = data.getLongExtra(EditPlaylistActivity.EXTRA_PLAYLIST_ID, -1L)

            if (categoryType != Metadata.Category.PLAYLISTS || playlistId != this.categoryId) {
                showSnackbar(
                    appCompatActivity.findViewById(android.R.id.content),
                    getString(R.string.playlist_edit_save_success, playlistName),
                    buttonText = getString(R.string.button_view),
                    buttonCb = {
                        startActivity(TrackListActivity.getStartIntent(
                            appCompatActivity, Metadata.Category.PLAYLISTS, playlistId, playlistName))
                    })
            }
        }
        super.onActivityResult(requestCode, resultCode, data)
    }

    override val title: String
        get() = getTitleOverride(getString(titleId))

    override val addFilterToToolbar: Boolean
        get() = false

    override fun setFilter(filter: String) {
        lastFilter = filter
        filterDebouncer.call()
    }

    override fun createOptionsMenu(menu: Menu): Boolean {
        when (Metadata.Category.PLAYLISTS == categoryType) {
            true -> appCompatActivity.menuInflater.inflate(R.menu.view_playlist_menu, menu)
            false -> addFilterAction(menu, this)
        }
        return true
    }

    override fun optionsItemSelected(menuItem: MenuItem): Boolean =
        when (menuItem.itemId == R.id.action_edit) {
            true -> {
                appCompatActivity.startActivityForResult(
                    EditPlaylistActivity.getStartIntent(
                        appCompatActivity,
                        categoryValue,
                        categoryId),
                    Track.RequestCode.EDIT_PLAYLIST)
                true
            }
            else -> false
        }

    override fun onTransportChanged() {
        adapter.notifyDataSetChanged()
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
        get() = Metadata.Category.OFFLINE == categoryType

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
                    Metadata.Category.OFFLINE == categoryType
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
                    Metadata.Category.OFFLINE == categoryType
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
            if (categoryType == Metadata.Category.PLAYLISTS) {
                return object: ItemContextMenuMixin.EventListener () {
                    override fun onPlaylistUpdated(id: Long, name: String) {
                        tracks.requery()
                    }
                }
            }

            return null
        }

    private val eventListener = object: TrackListAdapter.EventListener {
        override fun onItemClick(view: View, track: ITrack, position: Int) {
            if (isValidCategory(categoryType, categoryId)) {
                playback.service.play(categoryType, categoryId, position, lastFilter)
            }
            else {
                playback.service.playAll(position, lastFilter)
            }
        }

        override fun onActionItemClick(view: View, track: ITrack, position: Int) {
            val mixin = mixin(ItemContextMenuMixin::class.java)!!
            if (categoryType == Metadata.Category.PLAYLISTS) {
                mixin.showForPlaylistTrack(track, position, categoryId, categoryValue, view)
            }
            else {
                mixin.showForTrack(track, view)
            }
        }
    }

    companion object {
        const val TAG = "TrackListFragment"

        fun arguments(context: Context,
                      categoryType: String = "",
                      categoryId: Long = 0,
                      categoryValue: String = ""): Bundle = Bundle().apply {
            putLong(Track.Extra.SELECTED_ID, categoryId)
            putString(Track.Extra.CATEGORY_TYPE, categoryType)
            putString(Track.Extra.CATEGORY_VALUE, categoryValue)
            if (categoryValue.isNotEmpty()) {
                putString(
                    Shared.Extra.TITLE_OVERRIDE,
                    context.getString(R.string.songs_from_category, categoryValue))
            }
        }

        fun create(intent: Intent?): TrackListFragment =
            create(intent?.extras?.getBundle(Track.Extra.FRAGMENT_ARGUMENTS) ?: Bundle())

        fun create(arguments: Bundle = Bundle()): TrackListFragment =
            TrackListFragment().apply {
                this.arguments = arguments
            }

        private fun isValidCategory(categoryType: String?, categoryId: Long): Boolean =
            categoryType != null && categoryType.isNotEmpty() && categoryId != -1L
    }
}