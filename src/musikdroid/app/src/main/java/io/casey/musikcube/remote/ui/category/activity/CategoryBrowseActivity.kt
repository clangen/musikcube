package io.casey.musikcube.remote.ui.category.activity

import android.content.Context
import android.content.Intent
import android.os.Bundle
import android.support.v7.widget.RecyclerView
import android.view.LayoutInflater
import android.view.Menu
import android.view.View
import android.view.ViewGroup
import android.widget.TextView
import com.simplecityapps.recyclerview_fastscroll.views.FastScrollRecyclerView
import io.casey.musikcube.remote.R
import io.casey.musikcube.remote.service.websocket.model.ICategoryValue
import io.casey.musikcube.remote.service.websocket.model.IDataProvider
import io.casey.musikcube.remote.ui.shared.extension.*
import io.casey.musikcube.remote.ui.shared.fragment.TransportFragment
import io.casey.musikcube.remote.ui.shared.view.EmptyListView
import io.casey.musikcube.remote.util.Debouncer
import io.casey.musikcube.remote.ui.shared.constants.Navigation
import io.casey.musikcube.remote.service.websocket.Messages
import io.casey.musikcube.remote.ui.albums.activity.AlbumBrowseActivity
import io.casey.musikcube.remote.ui.shared.activity.BaseActivity
import io.casey.musikcube.remote.ui.shared.activity.Filterable
import io.casey.musikcube.remote.ui.tracks.activity.TrackListActivity
import io.casey.musikcube.remote.service.websocket.WebSocketService.State as SocketState

class CategoryBrowseActivity : BaseActivity(), Filterable {
    interface DeepLink {
        companion object {
            val TRACKS = 0
            val ALBUMS = 1
        }
    }

    private var adapter: Adapter = Adapter()
    private var deepLinkType: Int = 0
    private var lastFilter: String? = null
    private lateinit var category: String
    private lateinit var transport: TransportFragment
    private lateinit var emptyView: EmptyListView

    override fun onCreate(savedInstanceState: Bundle?) {
        component.inject(this)

        super.onCreate(savedInstanceState)

        category = intent.getStringExtra(EXTRA_CATEGORY)
        deepLinkType = intent.getIntExtra(EXTRA_DEEP_LINK_TYPE, DeepLink.ALBUMS)
        adapter = Adapter()

        setContentView(R.layout.recycler_view_activity)
        setTitle(categoryTitleStringId)

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
        if (resultCode == Navigation.ResponseCode.PLAYBACK_STARTED) {
            setResult(Navigation.ResponseCode.PLAYBACK_STARTED)
            finish()
        }

        super.onActivityResult(requestCode, resultCode, data)
    }

    override fun setFilter(filter: String) {
        this.lastFilter = filter
        this.filterDebouncer.call()
    }

    private fun initObservers() {
        disposables.add(dataProvider.observeState().subscribe(
            { states ->
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
            }, { /* error */ }
        ))
    }

    private val categoryTypeStringId: Int
        get() {
            return CATEGORY_NAME_TO_EMPTY_TYPE[category] ?: R.string.unknown_value
        }

    private val categoryTitleStringId: Int
        get() {
            return CATEGORY_NAME_TO_TITLE[category] ?: R.string.unknown_value
        }

    private fun requery() {
        dataProvider.getCategoryValues(category, lastFilter ?: "").subscribe(
            { values -> adapter.setModel(values) },
            { /* error */ },
            { emptyView.update(dataProvider.state, adapter.itemCount)})
    }

    private val filterDebouncer = object : Debouncer<String>(350) {
        override fun onDebounced(last: String?) {
            if (!isPaused()) {
                requery()
            }
        }
    }

    private val onItemClickListener = { view: View ->
        val entry = view.tag as ICategoryValue
        if (deepLinkType == DeepLink.ALBUMS) {
            navigateToAlbums(entry)
        }
        else {
            navigateToTracks(entry)
        }
    }

    private val onItemLongClickListener = { view: View ->
        /* if we deep link to albums by default, long press will get to
        tracks. if we deep link to tracks, just ignore */
        var result = false
        if (deepLinkType == DeepLink.ALBUMS) {
            navigateToTracks(view.tag as ICategoryValue)
            result = true
        }
        result
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

    private inner class ViewHolder internal constructor(itemView: View) : RecyclerView.ViewHolder(itemView) {
        private val title: TextView = itemView.findViewById(R.id.title)

        init {
            itemView.findViewById<View>(R.id.subtitle).visibility = View.GONE
        }

        internal fun bind(categoryValue: ICategoryValue) {
            val playing = transport.playbackService?.playingTrack
            val playingId = playing?.getCategoryId(category) ?: -1

            var titleColor = R.color.theme_foreground
            if (playingId != -1L && categoryValue.id == playingId) {
                titleColor = R.color.theme_green
            }

            title.text = fallback(categoryValue.value, getString(R.string.unknown_value))
            title.setTextColor(getColorCompat(titleColor))
            itemView.tag = categoryValue
        }
    }

    private inner class Adapter : RecyclerView.Adapter<ViewHolder>() {
        private var model: List<ICategoryValue> = ArrayList()

        internal fun setModel(model: List<ICategoryValue>?) {
            this.model = model ?: ArrayList()
            notifyDataSetChanged()
        }

        override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): ViewHolder {
            val inflater = LayoutInflater.from(parent.context)
            val view = inflater.inflate(R.layout.simple_list_item, parent, false)
            view.setOnClickListener(onItemClickListener)
            view.setOnLongClickListener(onItemLongClickListener)
            return ViewHolder(view)
        }

        override fun onBindViewHolder(holder: ViewHolder, position: Int) {
            holder.bind(model[position])
        }

        override fun getItemCount(): Int {
            return model.size
        }
    }

    companion object {
        private val EXTRA_CATEGORY = "extra_category"
        private val EXTRA_DEEP_LINK_TYPE = "extra_deep_link_type"

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

        fun getStartIntent(context: Context, category: String): Intent {
            return Intent(context, CategoryBrowseActivity::class.java)
                .putExtra(EXTRA_CATEGORY, category)
        }

        fun getStartIntent(context: Context, category: String, deepLinkType: Int): Intent {
            return Intent(context, CategoryBrowseActivity::class.java)
                .putExtra(EXTRA_CATEGORY, category)
                .putExtra(EXTRA_DEEP_LINK_TYPE, deepLinkType)
        }
    }
}
