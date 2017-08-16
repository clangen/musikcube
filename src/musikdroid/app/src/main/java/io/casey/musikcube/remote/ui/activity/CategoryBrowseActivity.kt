package io.casey.musikcube.remote.ui.activity

import android.content.Context
import android.content.Intent
import android.os.Bundle
import android.support.v7.widget.RecyclerView
import android.view.LayoutInflater
import android.view.Menu
import android.view.View
import android.view.ViewGroup
import android.widget.TextView
import com.pluscubed.recyclerfastscroll.RecyclerFastScroller
import io.casey.musikcube.remote.R
import io.casey.musikcube.remote.playback.Metadata
import io.casey.musikcube.remote.ui.extension.*
import io.casey.musikcube.remote.ui.fragment.TransportFragment
import io.casey.musikcube.remote.ui.view.EmptyListView
import io.casey.musikcube.remote.util.Debouncer
import io.casey.musikcube.remote.util.Navigation
import io.casey.musikcube.remote.util.Strings
import io.casey.musikcube.remote.websocket.Messages
import io.casey.musikcube.remote.websocket.SocketMessage
import io.casey.musikcube.remote.websocket.WebSocketService
import org.json.JSONArray
import org.json.JSONObject
import io.casey.musikcube.remote.websocket.WebSocketService.State as SocketState

class CategoryBrowseActivity : WebSocketActivityBase(), Filterable {
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
        super.onCreate(savedInstanceState)

        category = intent.getStringExtra(EXTRA_CATEGORY)
        deepLinkType = intent.getIntExtra(EXTRA_DEEP_LINK_TYPE, DeepLink.ALBUMS)
        adapter = Adapter()

        setContentView(R.layout.recycler_view_activity)
        setTitle(categoryTitleStringId)

        val fastScroller = findViewById<RecyclerFastScroller>(R.id.fast_scroller)
        val recyclerView = findViewById<RecyclerView>(R.id.recycler_view)
        setupDefaultRecyclerView(recyclerView, fastScroller, adapter)

        emptyView = findViewById<EmptyListView>(R.id.empty_list_view)
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

    override val webSocketServiceClient: WebSocketService.Client?
        get() = socketClient

    override val playbackServiceEventListener: (() -> Unit)?
        get() = null

    private val categoryTypeStringId: Int
        get() {
            return CATEGORY_NAME_TO_EMPTY_TYPE[category] ?: R.string.unknown_value
        }

    private val categoryTitleStringId: Int
        get() {
            return CATEGORY_NAME_TO_TITLE[category] ?: R.string.unknown_value
        }

    private fun requery() {
        val request = SocketMessage.Builder
            .request(Messages.Request.QueryCategory)
            .addOption(Messages.Key.CATEGORY, category)
            .addOption(Messages.Key.FILTER, lastFilter)
            .build()

        getWebSocketService().send(request, this.socketClient) { response: SocketMessage ->
            val data = response.getJsonArrayOption(Messages.Key.DATA)
            if (data != null && data.length() > 0) {
                adapter.setModel(data)
            }
            emptyView.update(getWebSocketService().state, adapter.itemCount)
        }
    }

    private val filterDebouncer = object : Debouncer<String>(350) {
        override fun onDebounced(last: String?) {
            if (!isPaused()) {
                requery()
            }
        }
    }

    private val socketClient = object : WebSocketService.Client {
        override fun onStateChanged(newState: WebSocketService.State, oldState: WebSocketService.State) {
            if (newState === WebSocketService.State.Connected) {
                filterDebouncer.cancel()
                requery()
            }
            else if (newState === WebSocketService.State.Disconnected) {
                emptyView.update(newState, adapter.itemCount)
            }
        }

        override fun onMessageReceived(message: SocketMessage) {}

        override fun onInvalidPassword() {}
    }

    private val onItemClickListener = { view: View ->
        val entry = view.tag as JSONObject
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
            navigateToTracks(view.tag as JSONObject)
            result = true
        }
        result
    }

    private fun navigateToAlbums(entry: JSONObject) {
        val intent = AlbumBrowseActivity.getStartIntent(this, category, entry)
        startActivityForResult(intent, Navigation.RequestCode.ALBUM_BROWSE_ACTIVITY)
    }

    private fun navigateToTracks(entry: JSONObject) {
        val categoryId = entry.optLong(Messages.Key.ID)
        val value = entry.optString(Messages.Key.VALUE)
        val intent = TrackListActivity.getStartIntent(this, category, categoryId, value)
        startActivityForResult(intent, Navigation.RequestCode.CATEGORY_TRACKS_ACTIVITY)
    }

    private inner class ViewHolder internal constructor(itemView: View) : RecyclerView.ViewHolder(itemView) {
        private val title: TextView = itemView.findViewById<TextView>(R.id.title)

        init {
            itemView.findViewById<View>(R.id.subtitle).visibility = View.GONE
        }

        internal fun bind(entry: JSONObject) {
            val entryId = entry.optLong(Messages.Key.ID)
            var playingId: Long = -1

            val idKey = CATEGORY_NAME_TO_ID[category]
            if (idKey != null && idKey.isNotEmpty()) {
                playingId = transport.playbackService?.getTrackLong(idKey, -1) ?: -1L
            }

            var titleColor = R.color.theme_foreground
            if (playingId != -1L && entryId == playingId) {
                titleColor = R.color.theme_green
            }

            /* note optString only does a null check! */
            var value = entry.optString(Messages.Key.VALUE, "")
            value = if (Strings.empty(value)) getString(R.string.unknown_value) else value

            title.text = value
            title.setTextColor(getColorCompat(titleColor))

            itemView.tag = entry
        }
    }

    private inner class Adapter : RecyclerView.Adapter<ViewHolder>() {
        private var model: JSONArray = JSONArray()

        internal fun setModel(model: JSONArray?) {
            this.model = model ?: JSONArray()
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
            holder.bind(model.optJSONObject(position))
        }

        override fun getItemCount(): Int {
            return model.length()
        }
    }

    companion object {
        private val EXTRA_CATEGORY = "extra_category"
        private val EXTRA_DEEP_LINK_TYPE = "extra_deep_link_type"

        private val CATEGORY_NAME_TO_ID: Map<String, String> = mapOf(
            Messages.Category.ALBUM_ARTIST to Metadata.Track.ALBUM_ARTIST_ID,
            Messages.Category.GENRE to Metadata.Track.GENRE_ID,
            Messages.Category.ARTIST to Metadata.Track.ARTIST_ID,
            Messages.Category.ALBUM to Metadata.Track.ALBUM_ID,
            Messages.Category.PLAYLISTS to Metadata.Track.ALBUM_ID)

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
