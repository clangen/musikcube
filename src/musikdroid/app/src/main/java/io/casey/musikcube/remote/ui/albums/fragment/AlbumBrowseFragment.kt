package io.casey.musikcube.remote.ui.albums.fragment

import android.os.Bundle
import android.view.LayoutInflater
import android.view.Menu
import android.view.View
import android.view.ViewGroup
import com.simplecityapps.recyclerview_fastscroll.views.FastScrollRecyclerView
import io.casey.musikcube.remote.R
import io.casey.musikcube.remote.service.playback.impl.remote.Metadata
import io.casey.musikcube.remote.service.websocket.model.IAlbum
import io.casey.musikcube.remote.service.websocket.model.IDataProvider
import io.casey.musikcube.remote.ui.albums.adapter.AlbumBrowseAdapter
import io.casey.musikcube.remote.ui.albums.constant.Album
import io.casey.musikcube.remote.ui.shared.activity.IFilterable
import io.casey.musikcube.remote.ui.shared.activity.ITitleProvider
import io.casey.musikcube.remote.ui.shared.activity.ITransportObserver
import io.casey.musikcube.remote.ui.shared.extension.*
import io.casey.musikcube.remote.ui.shared.fragment.BaseFragment
import io.casey.musikcube.remote.ui.shared.mixin.DataProviderMixin
import io.casey.musikcube.remote.ui.shared.mixin.ItemContextMenuMixin
import io.casey.musikcube.remote.ui.shared.mixin.PlaybackMixin
import io.casey.musikcube.remote.ui.shared.view.EmptyListView
import io.casey.musikcube.remote.ui.tracks.activity.TrackListActivity
import io.casey.musikcube.remote.util.Debouncer
import io.reactivex.rxkotlin.subscribeBy

class AlbumBrowseFragment: BaseFragment(), IFilterable, ITitleProvider, ITransportObserver {
    private var categoryName: String = ""
    private var categoryId: Long = 0
    private var lastFilter = ""
    private lateinit var adapter: AlbumBrowseAdapter
    private lateinit var playback: PlaybackMixin
    private lateinit var data: DataProviderMixin
    private lateinit var emptyView: EmptyListView

    override val title: String
        get() = getTitleOverride(app.getString(R.string.albums_title))

    override fun onCreate(savedInstanceState: Bundle?) {
        component.inject(this)
        data = mixin(DataProviderMixin())
        playback = mixin(PlaybackMixin())
        mixin(ItemContextMenuMixin(appCompatActivity, fragment = this))

        super.onCreate(savedInstanceState)

        extras.run {
            categoryName = getString(Album.Extra.CATEGORY_NAME) ?: ""
            categoryId = getLong(Album.Extra.CATEGORY_ID, categoryId)
        }

        adapter = AlbumBrowseAdapter(eventListener, playback, prefs)
    }

    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View =
        inflater.inflate(this.getLayoutId(), container, false).apply {
            val recyclerView = findViewById<FastScrollRecyclerView>(R.id.recycler_view)
            setupDefaultRecyclerView(recyclerView, adapter)

            emptyView = findViewById(R.id.empty_list_view)
            emptyView.capability = EmptyListView.Capability.OnlineOnly
            emptyView.emptyMessage = getString(R.string.empty_no_items_format, getString(R.string.browse_type_albums))
            emptyView.alternateView = recyclerView

            initToolbarIfNecessary(appCompatActivity, this)
        }

    override fun onResume() {
        super.onResume()
        initObservables()
    }

    override fun setFilter(filter: String) {
        if (filter != lastFilter) {
            lastFilter = filter
            filterDebouncer.call(filter)
        }
    }

    fun createOptionsMenu(menu: Menu): Boolean = initSearchMenu(menu, this)
    override fun onTransportChanged() = adapter.notifyDataSetChanged()

    private fun initObservables() =
        disposables.add(data.provider.observeState().subscribeBy(
            onNext = { state ->
                if (state.first == IDataProvider.State.Connected) {
                    requery()
                }
                else {
                    emptyView.update(state.first, adapter.itemCount)
                }
            },
            onError = {
            }))

    private fun requery() =
        @Suppress("unused")
        data.provider.getAlbumsForCategory(categoryName, categoryId, lastFilter)
            .subscribeBy(
                onNext = { albumList ->
                    adapter.setModel(albumList)
                    emptyView.update(data.provider.state, adapter.itemCount)
                },
                onError =  {
                })

    private val filterDebouncer = object : Debouncer<String>(350) {
        override fun onDebounced(last: String?) {
            if (!paused) {
                requery()
            }
        }
    }

    private val eventListener = object: AlbumBrowseAdapter.EventListener {
        override fun onItemClicked(album: IAlbum) =
            startActivity(TrackListActivity.getStartIntent(
                appCompatActivity, Metadata.Category.ALBUM, album.id, album.value))

        override fun onActionClicked(view: View, album: IAlbum) {
            mixin(ItemContextMenuMixin::class.java)?.showForCategory(album, view)
        }
    }

    companion object {
        const val TAG = "AlbumBrowseFragment"
        fun create(categoryName: String = "", categoryId: Long = -1L): AlbumBrowseFragment =
            AlbumBrowseFragment().apply {
                arguments = Bundle().apply {
                    putString(Album.Extra.CATEGORY_NAME, categoryName)
                    putLong(Album.Extra.CATEGORY_ID, categoryId)
                }
            }
    }
}