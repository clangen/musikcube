package io.casey.musikcube.remote.ui.category.fragment

import android.app.Activity
import android.content.Context
import android.content.Intent
import android.os.Bundle
import android.view.LayoutInflater
import android.view.Menu
import android.view.View
import android.view.ViewGroup
import com.simplecityapps.recyclerview_fastscroll.views.FastScrollRecyclerView
import io.casey.musikcube.remote.R
import io.casey.musikcube.remote.service.websocket.Messages
import io.casey.musikcube.remote.service.websocket.model.ICategoryValue
import io.casey.musikcube.remote.service.websocket.model.IDataProvider
import io.casey.musikcube.remote.ui.albums.activity.AlbumBrowseActivity
import io.casey.musikcube.remote.ui.category.adapter.CategoryBrowseAdapter
import io.casey.musikcube.remote.ui.category.constant.Category
import io.casey.musikcube.remote.ui.category.constant.NavigationType
import io.casey.musikcube.remote.ui.shared.activity.Filterable
import io.casey.musikcube.remote.ui.shared.extension.EXTRA_ACTIVITY_TITLE
import io.casey.musikcube.remote.ui.shared.extension.initSearchMenu
import io.casey.musikcube.remote.ui.shared.extension.setFabVisible
import io.casey.musikcube.remote.ui.shared.extension.setupDefaultRecyclerView
import io.casey.musikcube.remote.ui.shared.fragment.BaseFragment
import io.casey.musikcube.remote.ui.shared.mixin.DataProviderMixin
import io.casey.musikcube.remote.ui.shared.mixin.ItemContextMenuMixin
import io.casey.musikcube.remote.ui.shared.mixin.PlaybackMixin
import io.casey.musikcube.remote.ui.shared.view.EmptyListView
import io.casey.musikcube.remote.ui.tracks.activity.TrackListActivity
import io.casey.musikcube.remote.util.Debouncer
import io.reactivex.rxkotlin.subscribeBy
import java.lang.IllegalArgumentException

class CategoryBrowseFragment: BaseFragment(), Filterable {
    private lateinit var adapter: CategoryBrowseAdapter
    private var navigationType: NavigationType = NavigationType.Tracks
    private var lastFilter: String? = null
    private lateinit var category: String
    private lateinit var predicateType: String
    private var predicateId: Long = -1
    private lateinit var rootView: View
    private lateinit var emptyView: EmptyListView
    private lateinit var data: DataProviderMixin
    private lateinit var playback: PlaybackMixin

    val title: String
        get() {
            Category.NAME_TO_TITLE[category]?.let {
                return getString(it)
            }
            return Category.toDisplayString(app, category)
        }

    override fun onCreate(savedInstanceState: Bundle?) {
        component.inject(this)
        data = mixin(DataProviderMixin())
        playback = mixin(PlaybackMixin())
        mixin(ItemContextMenuMixin(appCompatActivity, contextMenuListener))

        super.onCreate(savedInstanceState)

        val args = arguments as Bundle
        category = args.getString(Category.Extra.CATEGORY, "")
        predicateType = args.getString(Category.Extra.PREDICATE_TYPE, "")
        predicateId = args.getLong(Category.Extra.PREDICATE_ID, -1)
        navigationType = NavigationType.get(args.getInt(Category.Extra.NAVIGATION_TYPE, NavigationType.Albums.ordinal))
        adapter = CategoryBrowseAdapter(adapterListener, playback, navigationType, category, prefs)
    }

    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View? =
        inflater.inflate(R.layout.recycler_view_fragment, container, false).apply {
            this@CategoryBrowseFragment.rootView = this

            val recyclerView = findViewById<FastScrollRecyclerView>(R.id.recycler_view)
            val fab = findViewById<View>(R.id.fab)
            val fabVisible = (category == Messages.Category.PLAYLISTS)

            emptyView = findViewById(R.id.empty_list_view)
            emptyView.capability = EmptyListView.Capability.OnlineOnly
            emptyView.emptyMessage = getString(R.string.empty_no_items_format, categoryTypeString)
            emptyView.alternateView = recyclerView

            findViewById<View>(R.id.fab).setOnClickListener {
                if (category == Messages.Category.PLAYLISTS) {
                    mixin(ItemContextMenuMixin::class.java)?.createPlaylist()
                }
            }

            setupDefaultRecyclerView(recyclerView, adapter)
            setFabVisible(fabVisible, fab, recyclerView)
        }

    override fun onResume() {
        super.onResume()
        initObservers()
    }

    override fun setFilter(filter: String) {
        this.lastFilter = filter
        this.filterDebouncer.call()
    }

    fun createOptionsMenu(menu: Menu): Boolean {
        when (Messages.Category.PLAYLISTS == category) {
            true -> menu.clear()
            else -> initSearchMenu(menu, this)
        }
        return true
    }

    fun notifyTransportChanged() =
        adapter.notifyDataSetChanged()

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

    private val categoryTypeString: String
        get() {
            Category.NAME_TO_EMPTY_TYPE[category]?.let {
                return getString(it)
            }
            return Category.toDisplayString(app, category)
        }

    private fun requery() {
        @Suppress("UNUSED")
        data.provider
            .getCategoryValues(category, predicateType, predicateId, lastFilter ?: "")
            .subscribeBy(
                onNext = { values -> adapter.setModel(values) },
                onError = { },
                onComplete = { emptyView.update(data.provider.state, adapter.itemCount)})
    }

    private val filterDebouncer = object : Debouncer<String>(350) {
        override fun onDebounced(last: String?) {
            if (!paused) {
                requery()
            }
        }
    }

    private val contextMenuListener = object: ItemContextMenuMixin.EventListener() {
        override fun onPlaylistDeleted(id: Long, name: String) = requery()

        override fun onPlaylistUpdated(id: Long, name: String) = requery()

        override fun onPlaylistCreated(id: Long, name: String) =
            if (navigationType == NavigationType.Select) navigateToSelect(id, name) else requery()
    }

    private val adapterListener = object: CategoryBrowseAdapter.EventListener {
        override fun onItemClicked(value: ICategoryValue) {
            when (navigationType) {
                NavigationType.Albums -> navigateToAlbums(value)
                NavigationType.Tracks -> navigateToTracks(value)
                NavigationType.Select -> navigateToSelect(value.id, value.value)
            }
        }

        override fun onActionClicked(view: View, value: ICategoryValue) {
            mixin(ItemContextMenuMixin::class.java)?.showForCategory(value, view)
        }
    }

    private fun navigateToAlbums(entry: ICategoryValue) =
            startActivity(AlbumBrowseActivity.getStartIntent(appCompatActivity, category, entry))

    private fun navigateToTracks(entry: ICategoryValue) =
            startActivity(TrackListActivity.getStartIntent(appCompatActivity, category, entry.id, entry.value))

    private fun navigateToSelect(id: Long, name: String) =
        appCompatActivity.run {
            setResult(Activity.RESULT_OK, Intent().apply {
                putExtra(Category.Extra.CATEGORY, category)
                putExtra(Category.Extra.ID, id)
                putExtra(Category.Extra.NAME, name)
            })
            finish()
        }

    companion object {
        const val TAG = "CategoryBrowseFragment"

        fun create(intent: Intent?): CategoryBrowseFragment {
            if (intent == null) {
                throw IllegalArgumentException("invalid intent")
            }
            return create(intent.getBundleExtra(Category.Extra.FRAGMENT_ARGUMENTS))
        }

        fun create(arguments: Bundle): CategoryBrowseFragment =
            CategoryBrowseFragment().apply {
                this.arguments = arguments
            }

        fun arguments(context: Context,
                      category: String,
                      predicateType: String = "",
                      predicateId: Long = -1,
                      predicateValue: String = ""): Bundle =
            Bundle().apply {
                putString(Category.Extra.CATEGORY, category)
                putString(Category.Extra.PREDICATE_TYPE, predicateType)
                putLong(Category.Extra.PREDICATE_ID, predicateId)
                if (predicateValue.isNotBlank() && Category.NAME_TO_RELATED_TITLE.containsKey(category)) {
                    val format = Category.NAME_TO_RELATED_TITLE[category]
                    when (format) {
                        null -> throw IllegalArgumentException("unknown category $category")
                        else -> putString(EXTRA_ACTIVITY_TITLE, context.getString(format, predicateValue))
                    }
                }
            }

        fun arguments(category: String,
                      navigationType: NavigationType,
                      title: String = ""): Bundle =
            Bundle().apply {
                putString(Category.Extra.CATEGORY, category)
                putInt(Category.Extra.NAVIGATION_TYPE, navigationType.ordinal)
                putString(Category.Extra.TITLE, title)
            }
    }
}