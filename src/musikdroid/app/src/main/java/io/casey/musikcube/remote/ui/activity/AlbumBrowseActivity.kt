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
import io.casey.musikcube.remote.websocket.Messages.Key
import io.casey.musikcube.remote.websocket.SocketMessage
import io.casey.musikcube.remote.websocket.WebSocketService
import org.json.JSONArray
import org.json.JSONObject

class AlbumBrowseActivity : WebSocketActivityBase(), Filterable {
    private var adapter: Adapter = Adapter()
    private var transport: TransportFragment? = null
    private var categoryName: String? = null
    private var categoryId: Long = 0
    private var lastFilter = ""
    private var emptyView: EmptyListView? = null

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        categoryName = intent.getStringExtra(EXTRA_CATEGORY_NAME)
        categoryId = intent.getLongExtra(EXTRA_CATEGORY_ID, categoryId)

        setContentView(R.layout.recycler_view_activity)

        setTitleFromIntent(R.string.albums_title)
        enableUpNavigation()

        val fastScroller = findViewById(R.id.fast_scroller) as RecyclerFastScroller
        val recyclerView = findViewById(R.id.recycler_view) as RecyclerView
        setupDefaultRecyclerView(recyclerView, fastScroller, adapter)

        emptyView = findViewById(R.id.empty_list_view) as EmptyListView
        emptyView?.capability = EmptyListView.Capability.OnlineOnly
        emptyView?.emptyMessage = getString(R.string.empty_no_items_format, getString(R.string.browse_type_albums))
        emptyView?.alternateView = recyclerView

        transport = addTransportFragment(object: TransportFragment.OnModelChangedListener {
            override fun onChanged(fragment: TransportFragment) {
                adapter.notifyDataSetChanged()
            }
        })
    }

    override fun onCreateOptionsMenu(menu: Menu): Boolean {
        initSearchMenu(menu, this)
        return true
    }

    override fun setFilter(filter: String) {
        lastFilter = filter
        filterDebouncer.call(filter)
    }

    override val webSocketServiceClient: WebSocketService.Client?
        get() = socketClient

    override val playbackServiceEventListener: (() -> Unit)?
        get() = null

    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        if (resultCode == Navigation.ResponseCode.PLAYBACK_STARTED) {
            setResult(Navigation.ResponseCode.PLAYBACK_STARTED)
            finish()
        }

        super.onActivityResult(requestCode, resultCode, data)
    }

    private fun requery() {
        val message = SocketMessage.Builder
            .request(Messages.Request.QueryAlbums)
            .addOption(Messages.Key.CATEGORY, categoryName)
            .addOption(Messages.Key.CATEGORY_ID, categoryId)
            .addOption(Key.FILTER, lastFilter)
            .build()

        getWebSocketService().send(message, socketClient) { response: SocketMessage ->
                adapter.setModel(response.getJsonArrayOption(Messages.Key.DATA) ?: JSONArray())
                emptyView?.update(getWebSocketService().state, adapter.itemCount)
        }
    }

    private val filterDebouncer = object : Debouncer<String>(350) {
        override fun onDebounced(last: String?) {
            if (!isPaused) {
                requery()
            }
        }
    }

    private val socketClient = object : WebSocketService.Client {
        override fun onStateChanged(newState: WebSocketService.State, oldState: WebSocketService.State) {
            if (newState === WebSocketService.State.Connected) {
                filterDebouncer.call()
                requery()
            }
            else {
                emptyView!!.update(newState, adapter.itemCount)
            }
        }

        override fun onMessageReceived(message: SocketMessage) {}

        override fun onInvalidPassword() {}
    }

    private val onItemClickListener = { view: View ->
        val album = view.tag as JSONObject
        val id = album.optLong(Key.ID)
        val title = album.optString(Metadata.Album.TITLE, "")

        val intent = TrackListActivity.getStartIntent(
            this@AlbumBrowseActivity, Messages.Category.ALBUM, id, title)

        startActivityForResult(intent, Navigation.RequestCode.ALBUM_TRACKS_ACTIVITY)
    }

    private inner class ViewHolder internal constructor(itemView: View) : RecyclerView.ViewHolder(itemView) {
        private val title: TextView = itemView.findViewById(R.id.title) as TextView
        private val subtitle: TextView = itemView.findViewById(R.id.subtitle) as TextView

        internal fun bind(entry: JSONObject) {
            val playingId = transport?.playbackService?.getTrackLong(Metadata.Track.ALBUM_ID, -1L) ?: -1L
            val entryId = entry.optLong(Key.ID)

            var titleColor = R.color.theme_foreground
            var subtitleColor = R.color.theme_disabled_foreground

            if (playingId != -1L && entryId == playingId) {
                titleColor = R.color.theme_green
                subtitleColor = R.color.theme_yellow
            }

            title.text = entry.optString(Metadata.Album.TITLE, "-")
            title.setTextColor(getColorCompat(titleColor))

            subtitle.text = entry.optString(Metadata.Album.ALBUM_ARTIST, "-")
            subtitle.setTextColor(getColorCompat(subtitleColor))
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
        private val EXTRA_CATEGORY_NAME = "extra_category_name"
        private val EXTRA_CATEGORY_ID = "extra_category_id"

        fun getStartIntent(context: Context): Intent {
            return Intent(context, AlbumBrowseActivity::class.java)
        }

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

        fun getStartIntent(context: Context, categoryName: String, categoryJson: JSONObject): Intent {
            val value = categoryJson.optString(Messages.Key.VALUE)
            val categoryId = categoryJson.optLong(Messages.Key.ID)
            return getStartIntent(context, categoryName, categoryId, value)
        }
    }
}
