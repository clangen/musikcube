package io.casey.musikcube.remote.ui.category.activity

import android.content.Context
import android.content.Intent
import android.os.Bundle
import android.view.Menu
import android.view.View
import com.simplecityapps.recyclerview_fastscroll.views.FastScrollRecyclerView
import io.casey.musikcube.remote.R
import io.casey.musikcube.remote.service.websocket.Messages
import io.casey.musikcube.remote.service.websocket.model.ICategoryValue
import io.casey.musikcube.remote.service.websocket.model.IDataProvider
import io.casey.musikcube.remote.ui.albums.activity.AlbumBrowseActivity
import io.casey.musikcube.remote.ui.category.adapter.CategoryBrowseAdapter
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
import io.reactivex.rxkotlin.subscribeBy
import io.casey.musikcube.remote.service.websocket.WebSocketService.State as SocketState

class CategoryBrowseActivity : BaseActivity(), Filterable {
    enum class NavigationType {
        Tracks, Albums, Select;

        companion object {
            fun get(ordinal: Int) = values()[ordinal]
        }
    }

    private lateinit var adapter: CategoryBrowseAdapter
    private var navigationType: NavigationType = NavigationType.Tracks
    private var lastFilter: String? = null
    private lateinit var category: String
    private lateinit var predicateType: String
    private var predicateId: Long = -1
    private lateinit var transport: TransportFragment
    private lateinit var emptyView: EmptyListView
    private lateinit var data: DataProviderMixin
    private lateinit var playback: PlaybackMixin

    override fun onCreate(savedInstanceState: Bundle?) {
        component.inject(this)
        data = mixin(DataProviderMixin())
        playback = mixin(PlaybackMixin())
        mixin(ItemContextMenuMixin(this))

        super.onCreate(savedInstanceState)

        category = intent.getStringExtra(EXTRA_CATEGORY)
        predicateType = intent.getStringExtra(EXTRA_PREDICATE_TYPE) ?: ""
        predicateId = intent.getLongExtra(EXTRA_PREDICATE_ID, -1)
        navigationType = NavigationType.get(intent.getIntExtra(EXTRA_NAVIGATION_TYPE, NavigationType.Albums.ordinal))
        adapter = CategoryBrowseAdapter(eventListener, playback, category)

        setContentView(R.layout.recycler_view_activity)
        setTitleFromIntent(categoryTitleStringId)

        val recyclerView = findViewById<FastScrollRecyclerView>(R.id.recycler_view)
        setupDefaultRecyclerView(recyclerView, adapter)

        emptyView = findViewById(R.id.empty_list_view)
        emptyView.capability = EmptyListView.Capability.OnlineOnly
        emptyView.emptyMessage = getString(R.string.empty_no_items_format, getString(categoryTypeStringId))
        emptyView.alternateView = recyclerView

        enableUpNavigation()

        transport = addTransportFragment(object: TransportFragment.OnModelChangedListener {
            override fun onChanged(fragment: TransportFragment) {
                adapter.notifyDataSetChanged()
            }
        })!!
    }

    override fun onResume() {
        super.onResume()
        initObservers()
    }

    override fun onCreateOptionsMenu(menu: Menu): Boolean {
        if (Messages.Category.PLAYLISTS != category) { /* bleh */
            initSearchMenu(menu, this)
        }

        return true
    }

    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        super.onActivityResult(requestCode, resultCode, data)

        if (resultCode == Navigation.ResponseCode.PLAYBACK_STARTED) {
            setResult(Navigation.ResponseCode.PLAYBACK_STARTED)
            finish()
        }
    }

    override fun setFilter(filter: String) {
        this.lastFilter = filter
        this.filterDebouncer.call()
    }

    private fun initObservers() {
        disposables.add(data.provider.observeState().subscribeBy(
            onNext = { states ->
                when (states.first) {
                    IDataProvider.State.Connected -> {
                        filterDebouncer.cancel()
                        requery()
                    }
                    IDataProvider.State.Disconnected -> {
                        emptyView.update(states.first, adapter.itemCount)
                    }
                    else -> { }
                }
            },
            onError = {
            }))
    }

    private val categoryTypeStringId: Int
        get() = CATEGORY_NAME_TO_EMPTY_TYPE[category] ?: R.string.unknown_value

    private val categoryTitleStringId: Int
        get() = CATEGORY_NAME_TO_TITLE[category] ?: R.string.unknown_value

    private fun requery() {
        data.provider.getCategoryValues(category, predicateType, predicateId, lastFilter ?: "").subscribeBy(
            onNext = { values -> adapter.setModel(values) },
            onError = { },
            onComplete = { emptyView.update(data.provider.state, adapter.itemCount)})
    }

    private val filterDebouncer = object : Debouncer<String>(350) {
        override fun onDebounced(last: String?) {
            if (!isPaused()) {
                requery()
            }
        }
    }

    private val eventListener = object: CategoryBrowseAdapter.EventListener {
        override fun onItemClicked(value: ICategoryValue) {
            when (navigationType) {
                NavigationType.Albums -> navigateToAlbums(value)
                NavigationType.Tracks -> navigateToTracks(value)
                NavigationType.Select -> {
                    val intent = Intent()
                        .putExtra(EXTRA_CATEGORY, value.type)
                        .putExtra(EXTRA_ID, value.id)
                    setResult(RESULT_OK, intent)
                    finish()
                }
            }
        }

        override fun onActionClicked(view: View, value: ICategoryValue) {
            mixin(ItemContextMenuMixin::class.java)?.showForCategory(value, view)
        }
    }

    private fun navigateToAlbums(entry: ICategoryValue) {
        val intent = AlbumBrowseActivity.getStartIntent(this, category, entry)
        startActivityForResult(intent, Navigation.RequestCode.ALBUM_BROWSE_ACTIVITY)
    }

    private fun navigateToTracks(entry: ICategoryValue) {
        val categoryId = entry.id
        val value = entry.value
        val intent = TrackListActivity.getStartIntent(this, category, categoryId, value)
        startActivityForResult(intent, Navigation.RequestCode.CATEGORY_TRACKS_ACTIVITY)
    }

    companion object {
        val EXTRA_CATEGORY = "extra_category"
        val EXTRA_ID = "extra_id"
        private val EXTRA_PREDICATE_TYPE = "extra_predicate_type"
        private val EXTRA_PREDICATE_ID = "extra_predicate_id"
        private val EXTRA_NAVIGATION_TYPE = "extra_navigation_type"
        private val EXTRA_TITLE = "extra_title"

        private val CATEGORY_NAME_TO_TITLE: Map<String, Int> = mapOf(
            Messages.Category.ALBUM_ARTIST to R.string.artists_title,
            Messages.Category.GENRE to R.string.genres_title,
            Messages.Category.ARTIST to R.string.artists_title,
            Messages.Category.ALBUM to R.string.albums_title,
            Messages.Category.PLAYLISTS to R.string.playlists_title)

        private val CATEGORY_NAME_TO_EMPTY_TYPE: Map<String, Int> = mapOf(
            Messages.Category.ALBUM_ARTIST to R.string.browse_type_artists,
            Messages.Category.GENRE to R.string.browse_type_genres,
            Messages.Category.ARTIST to R.string.browse_type_artists,
            Messages.Category.ALBUM to R.string.browse_type_albums,
            Messages.Category.PLAYLISTS to R.string.browse_type_playlists)

        fun getStartIntent(context: Context, category: String, predicateType: String = "", predicateId: Long = -1): Intent {
            return Intent(context, CategoryBrowseActivity::class.java)
                .putExtra(EXTRA_CATEGORY, category)
                .putExtra(EXTRA_PREDICATE_TYPE, predicateType)
                .putExtra(EXTRA_PREDICATE_ID, predicateId)
        }

        fun getStartIntent(context: Context, category: String, navigationType: NavigationType, title: String = ""): Intent {
            return Intent(context, CategoryBrowseActivity::class.java)
                .putExtra(EXTRA_CATEGORY, category)
                .putExtra(EXTRA_NAVIGATION_TYPE, navigationType.ordinal)
                .putExtra(EXTRA_TITLE, title)
        }
    }
}
