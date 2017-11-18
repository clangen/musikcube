package io.casey.musikcube.remote.ui.albums.activity

import android.content.Context
import android.content.Intent
import android.os.Bundle
import android.view.Menu
import android.view.View
import com.simplecityapps.recyclerview_fastscroll.views.FastScrollRecyclerView
import io.casey.musikcube.remote.R
import io.casey.musikcube.remote.service.websocket.Messages
import io.casey.musikcube.remote.service.websocket.model.IAlbum
import io.casey.musikcube.remote.service.websocket.model.ICategoryValue
import io.casey.musikcube.remote.service.websocket.model.IDataProvider
import io.casey.musikcube.remote.ui.albums.adapter.AlbumBrowseAdapter
import io.casey.musikcube.remote.ui.shared.activity.BaseActivity
import io.casey.musikcube.remote.ui.shared.activity.Filterable
import io.casey.musikcube.remote.ui.shared.constants.Navigation
import io.casey.musikcube.remote.ui.shared.extension.*
import io.casey.musikcube.remote.ui.shared.fragment.TransportFragment
import io.casey.musikcube.remote.ui.shared.mixin.DataProviderMixin
import io.casey.musikcube.remote.ui.shared.mixin.ItemContextMenuMixin
import io.casey.musikcube.remote.ui.shared.mixin.PlaybackMixin
import io.casey.musikcube.remote.ui.shared.view.EmptyListView
import io.casey.musikcube.remote.ui.tracks.activity.TrackListActivity
import io.casey.musikcube.remote.util.Debouncer
import io.casey.musikcube.remote.util.Strings
import io.reactivex.rxkotlin.subscribeBy

class AlbumBrowseActivity : BaseActivity(), Filterable {
    private var categoryName: String = ""
    private var categoryId: Long = 0
    private var lastFilter = ""
    private lateinit var adapter: AlbumBrowseAdapter
    private lateinit var playback: PlaybackMixin
    private lateinit var data: DataProviderMixin
    private lateinit var transport: TransportFragment
    private lateinit var emptyView: EmptyListView

    override fun onCreate(savedInstanceState: Bundle?) {
        component.inject(this)
        data = mixin(DataProviderMixin())
        playback = mixin(PlaybackMixin())
        mixin(ItemContextMenuMixin(this))

        super.onCreate(savedInstanceState)

        categoryName = intent.getStringExtra(EXTRA_CATEGORY_NAME) ?: ""
        categoryId = intent.getLongExtra(EXTRA_CATEGORY_ID, categoryId)

        setContentView(R.layout.recycler_view_activity)

        setTitleFromIntent(R.string.albums_title)
        enableUpNavigation()

        adapter = AlbumBrowseAdapter(eventListener, playback)

        val recyclerView = findViewById<FastScrollRecyclerView>(R.id.recycler_view)
        setupDefaultRecyclerView(recyclerView, adapter)

        emptyView = findViewById(R.id.empty_list_view)
        emptyView.capability = EmptyListView.Capability.OnlineOnly
        emptyView.emptyMessage = getString(R.string.empty_no_items_format, getString(R.string.browse_type_albums))
        emptyView.alternateView = recyclerView

        transport = addTransportFragment(object: TransportFragment.OnModelChangedListener {
            override fun onChanged(fragment: TransportFragment) {
                adapter.notifyDataSetChanged()
            }
        })!!
    }

    override fun onResume() {
        super.onResume()
        initObservables()
    }

    override fun onCreateOptionsMenu(menu: Menu): Boolean {
        initSearchMenu(menu, this)
        return true
    }

    override fun setFilter(filter: String) {
        lastFilter = filter
        filterDebouncer.call(filter)
    }

    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        if (resultCode == Navigation.ResponseCode.PLAYBACK_STARTED) {
            setResult(Navigation.ResponseCode.PLAYBACK_STARTED)
            finish()
        }

        super.onActivityResult(requestCode, resultCode, data)
    }

    private fun initObservables() {
        disposables.add(data.provider.observeState().subscribeBy(
            onNext = { state ->
                if (state.first == IDataProvider.State.Connected) {
                    filterDebouncer.call()
                    requery()
                }
                else {
                    emptyView.update(state.first, adapter.itemCount)
                }
            },
            onError = {
            }))
    }

    private fun requery() {
        data.provider.getAlbumsForCategory(categoryName, categoryId, lastFilter)
            .subscribeBy(
                onNext = { albumList ->
                    adapter.setModel(albumList)
                    emptyView.update(data.provider.state, adapter.itemCount)
                },
                onError =  {
                })
    }

    private val filterDebouncer = object : Debouncer<String>(350) {
        override fun onDebounced(last: String?) {
            if (!isPaused()) {
                requery()
            }
        }
    }

    private val eventListener = object: AlbumBrowseAdapter.EventListener {
        override fun onItemClicked(album: IAlbum) {
            val intent = TrackListActivity.getStartIntent(
                this@AlbumBrowseActivity, Messages.Category.ALBUM, album.id, album.value)

            startActivityForResult(intent, Navigation.RequestCode.ALBUM_TRACKS_ACTIVITY)        }

        override fun onActionClicked(view: View, album: IAlbum) {
            mixin(ItemContextMenuMixin::class.java)?.showForCategory(album, view)
        }
    }

    companion object {
        private val EXTRA_CATEGORY_NAME = "extra_category_name"
        private val EXTRA_CATEGORY_ID = "extra_category_id"

        fun getStartIntent(context: Context): Intent =
            Intent(context, AlbumBrowseActivity::class.java)

        fun getStartIntent(context: Context, categoryName: String, categoryId: Long): Intent {
            return Intent(context, AlbumBrowseActivity::class.java)
                .putExtra(EXTRA_CATEGORY_NAME, categoryName)
                .putExtra(EXTRA_CATEGORY_ID, categoryId)
        }

        fun getStartIntent(context: Context, categoryName: String, categoryId: Long, categoryValue: String): Intent {
            val intent = getStartIntent(context, categoryName, categoryId)

            if (Strings.notEmpty(categoryValue)) {
                intent.putExtra(
                    EXTRA_ACTIVITY_TITLE,
                    context.getString(R.string.albums_by_title, categoryValue))
            }

            return intent
        }

        fun getStartIntent(context: Context, categoryName: String, categoryValue: ICategoryValue): Intent =
            getStartIntent(context, categoryName, categoryValue.id, categoryValue.value)
    }
}
