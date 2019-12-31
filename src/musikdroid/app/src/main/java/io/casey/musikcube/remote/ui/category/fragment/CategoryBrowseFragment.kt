package io.casey.musikcube.remote.ui.category.fragment

import android.app.Activity
import android.content.Context
import android.content.Intent
import android.os.Bundle
import android.view.LayoutInflater
import android.view.Menu
import android.view.View
import android.view.ViewGroup
import com.google.android.material.floatingactionbutton.FloatingActionButton
import com.simplecityapps.recyclerview_fastscroll.views.FastScrollRecyclerView
import io.casey.musikcube.remote.R
import io.casey.musikcube.remote.service.playback.impl.remote.Metadata
import io.casey.musikcube.remote.service.websocket.model.ICategoryValue
import io.casey.musikcube.remote.service.websocket.model.IMetadataProxy
import io.casey.musikcube.remote.ui.category.adapter.CategoryBrowseAdapter
import io.casey.musikcube.remote.ui.category.constant.Category
import io.casey.musikcube.remote.ui.category.constant.NavigationType
import io.casey.musikcube.remote.ui.navigation.Navigate
import io.casey.musikcube.remote.ui.shared.activity.IFabConsumer
import io.casey.musikcube.remote.ui.shared.activity.IFilterable
import io.casey.musikcube.remote.ui.shared.activity.ITitleProvider
import io.casey.musikcube.remote.ui.shared.activity.ITransportObserver
import io.casey.musikcube.remote.ui.shared.constant.Shared
import io.casey.musikcube.remote.ui.shared.extension.addFilterAction
import io.casey.musikcube.remote.ui.shared.extension.getLayoutId
import io.casey.musikcube.remote.ui.shared.extension.getTitleOverride
import io.casey.musikcube.remote.ui.shared.extension.setupDefaultRecyclerView
import io.casey.musikcube.remote.ui.shared.fragment.BaseFragment
import io.casey.musikcube.remote.ui.shared.mixin.ItemContextMenuMixin
import io.casey.musikcube.remote.ui.shared.mixin.MetadataProxyMixin
import io.casey.musikcube.remote.ui.shared.mixin.PlaybackMixin
import io.casey.musikcube.remote.ui.shared.view.EmptyListView
import io.casey.musikcube.remote.util.Debouncer
import io.reactivex.rxkotlin.subscribeBy

class CategoryBrowseFragment: BaseFragment(), IFilterable, ITitleProvider, ITransportObserver, IFabConsumer {
    private lateinit var adapter: CategoryBrowseAdapter
    private var lastFilter: String? = null
    private lateinit var rootView: View
    private lateinit var emptyView: EmptyListView
    private lateinit var data: MetadataProxyMixin
    private lateinit var playback: PlaybackMixin

    private val navigationType: NavigationType
        get() = NavigationType.get(extras.getInt(
            Category.Extra.NAVIGATION_TYPE, NavigationType.Albums.ordinal))

    private val category
        get() = extras.getString(Category.Extra.CATEGORY, "")

    private val predicateType: String
        get() = extras.getString(Category.Extra.PREDICATE_TYPE, "")

    private val predicateId: Long
        get() = extras.getLong(Category.Extra.PREDICATE_ID, -1L)

    override val title: String
        get() {
            Category.NAME_TO_TITLE[category]?.let {
                return getTitleOverride(getString(it))
            }
            return getTitleOverride(Category.toDisplayString(app, category))
        }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        component.inject(this)
        data = mixin(MetadataProxyMixin())
        playback = mixin(PlaybackMixin())
        mixin(ItemContextMenuMixin(appCompatActivity, contextMenuListener, this))

        adapter = CategoryBrowseAdapter(adapterListener, playback, navigationType, category, prefs)
    }

    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View? =
        inflater.inflate(this.getLayoutId(), container, false).apply {
            this@CategoryBrowseFragment.rootView = this
            val recyclerView = findViewById<FastScrollRecyclerView>(R.id.recycler_view)

            emptyView = findViewById(R.id.empty_list_view)
            emptyView.capability = EmptyListView.Capability.OnlineOnly
            emptyView.emptyMessage = getString(R.string.empty_no_items_format, categoryTypeString)
            emptyView.alternateView = recyclerView

            setupDefaultRecyclerView(recyclerView, adapter)
        }

    override fun onFabPress(fab: FloatingActionButton) {
        mixin(ItemContextMenuMixin::class.java)?.createPlaylist()
    }

    override val fabVisible: Boolean
        get() = (category == Metadata.Category.PLAYLISTS)

    override val addFilterToToolbar: Boolean
        get() = true

    override fun setFilter(filter: String) {
        this.lastFilter = filter
        this.filterDebouncer.call()
    }

    fun createOptionsMenu(menu: Menu): Boolean {
        when (Metadata.Category.PLAYLISTS == category) {
            true -> menu.clear()
            else -> addFilterAction(menu, this)
        }
        return true
    }

    override fun onTransportChanged() =
        adapter.notifyDataSetChanged()

    override fun onInitObservables() {
        disposables.add(data.provider.observeState().subscribeBy(
            onNext = { states ->
                when (states.first) {
                    IMetadataProxy.State.Connected -> {
                        filterDebouncer.cancel()
                        requery()
                    }
                    IMetadataProxy.State.Disconnected -> {
                        emptyView.update(states.first, adapter.itemCount)
                    }
                    else -> {
                    }
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

    private val resolvedFilter: String
        get() =
            when (category) {
                Metadata.Category.PLAYLISTS -> ""
                else -> lastFilter ?: ""
            }

    private fun requery() {
        @Suppress("UNUSED")
        data.provider
            .getCategoryValues(category, predicateType, predicateId, resolvedFilter)
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
        Navigate.toAlbums(category, entry, appCompatActivity, this)

    private fun navigateToTracks(entry: ICategoryValue) =
        Navigate.toTracks(category, entry, appCompatActivity, this)

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

        fun create(context: Context,
                   targetType: String,
                   predicateType: String = "",
                   predicateId: Long = -1,
                   predicateValue: String = ""): CategoryBrowseFragment =
            create(arguments(context, targetType, predicateType, predicateId, predicateValue))

        fun arguments(context: Context,
                      targetType: String,
                      sourceType: String = "",
                      sourceId: Long = -1,
                      sourceValue: String = ""): Bundle =
            Bundle().apply {
                putString(Category.Extra.CATEGORY, targetType)
                putString(Category.Extra.PREDICATE_TYPE, sourceType)
                putLong(Category.Extra.PREDICATE_ID, sourceId)
                if (sourceValue.isNotBlank() && Category.NAME_TO_RELATED_TITLE.containsKey(targetType)) {
                    when (val format = Category.NAME_TO_RELATED_TITLE[targetType]) {
                        null -> throw IllegalArgumentException("unknown category $targetType")
                        else -> putString(Shared.Extra.TITLE_OVERRIDE, context.getString(format, sourceValue))
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