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
import io.casey.musikcube.remote.ui.model.TrackListSlidingWindow
import io.casey.musikcube.remote.ui.model.TrackListSlidingWindow.QueryFactory
import io.casey.musikcube.remote.ui.view.EmptyListView
import io.casey.musikcube.remote.ui.view.EmptyListView.Capability
import io.casey.musikcube.remote.util.Debouncer
import io.casey.musikcube.remote.util.Navigation
import io.casey.musikcube.remote.util.Strings
import io.casey.musikcube.remote.websocket.Messages
import io.casey.musikcube.remote.websocket.SocketMessage
import io.casey.musikcube.remote.websocket.WebSocketService
import org.json.JSONObject

class TrackListActivity : WebSocketActivityBase(), Filterable {
    private lateinit var tracks: TrackListSlidingWindow
    private lateinit var emptyView: EmptyListView
    private lateinit var transport: TransportFragment
    private var categoryType: String = ""
    private var categoryId: Long = 0
    private var lastFilter = ""
    private var adapter = Adapter()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        val intent = intent
        categoryType = intent.getStringExtra(EXTRA_CATEGORY_TYPE)
        categoryId = intent.getLongExtra(EXTRA_SELECTED_ID, 0)
        val titleId = intent.getIntExtra(EXTRA_TITLE_ID, R.string.songs_title)

        setContentView(R.layout.recycler_view_activity)

        setTitleFromIntent(titleId)
        enableUpNavigation()

        val queryFactory = createCategoryQueryFactory(categoryType, categoryId)

        val fastScroller = findViewById<RecyclerFastScroller>(R.id.fast_scroller)
        val recyclerView = findViewById<RecyclerView>(R.id.recycler_view)
        setupDefaultRecyclerView(recyclerView, fastScroller, adapter)

        emptyView = findViewById(R.id.empty_list_view)
        emptyView.let {
            it.capability = if (isOfflineTracks) Capability.OfflineOk else Capability.OnlineOnly
            it.emptyMessage = emptyMessage
            it.alternateView = recyclerView
        }

        tracks = TrackListSlidingWindow(
            recyclerView, fastScroller, getWebSocketService(), queryFactory)

        tracks.setOnMetadataLoadedListener(slidingWindowListener)

        transport = addTransportFragment(object: TransportFragment.OnModelChangedListener {
            override fun onChanged(fragment: TransportFragment) {
                adapter.notifyDataSetChanged()
            }
        })!!
    }

    override fun onCreateOptionsMenu(menu: Menu): Boolean {
        if (Messages.Category.PLAYLISTS != categoryType) {
            initSearchMenu(menu, this)
        }
        return true
    }

    override fun onPause() {
        super.onPause()
        tracks.pause()
    }

    override fun onResume() {
        tracks.resume() /* needs to happen before */
        super.onResume()
        requeryIfViewingOfflineCache()
    }

    override val webSocketServiceClient: WebSocketService.Client?
        get() = socketClient

    override val playbackServiceEventListener: (() -> Unit)?
        get() = null

    override fun setFilter(filter: String) {
        lastFilter = filter
        filterDebouncer.call()
    }

    private val socketClient = object : WebSocketService.Client {
        override fun onStateChanged(newState: WebSocketService.State, oldState: WebSocketService.State) {
            if (newState === WebSocketService.State.Connected) {
                filterDebouncer.cancel()
                tracks.requery()
            }
            else {
                emptyView.update(newState, adapter.itemCount)
            }
        }

        override fun onMessageReceived(message: SocketMessage) {}

        override fun onInvalidPassword() {}
    }

    private val filterDebouncer = object : Debouncer<String>(350) {
        override fun onDebounced(last: String?) {
            if (!isPaused) {
                tracks.requery()
            }
        }
    }

    private val onItemClickListener = { view: View ->
        val index = view.tag as Int

        if (isValidCategory(categoryType, categoryId)) {
            playbackService?.play(categoryType, categoryId, index, lastFilter)
        }
        else {
            playbackService?.playAll(index, lastFilter)
        }

        setResult(Navigation.ResponseCode.PLAYBACK_STARTED)
        finish()
    }

    private inner class ViewHolder internal constructor(itemView: View) : RecyclerView.ViewHolder(itemView) {
        private val title: TextView = itemView.findViewById<TextView>(R.id.title)
        private val subtitle: TextView = itemView.findViewById<TextView>(R.id.subtitle)

        internal fun bind(entry: JSONObject?, position: Int) {
            itemView.tag = position

            /* TODO: this colorizing logic matches copied from PlayQueueActivity. can we generalize
            it cleanly somehow? matches it worth it? */
            var titleColor = R.color.theme_foreground
            var subtitleColor = R.color.theme_disabled_foreground

            if (entry != null) {
                val entryExternalId = entry.optString(Metadata.Track.EXTERNAL_ID, "")
                val playingExternalId = transport.playbackService?.getTrackString(Metadata.Track.EXTERNAL_ID, "")

                if (entryExternalId == playingExternalId) {
                    titleColor = R.color.theme_green
                    subtitleColor = R.color.theme_yellow
                }

                title.text = entry.optString(Metadata.Track.TITLE, "-")
                subtitle.text = entry.optString(Metadata.Track.ALBUM_ARTIST, "-")
            }
            else {
                title.text = "-"
                subtitle.text = "-"
            }

            title.setTextColor(getColorCompat(titleColor))
            subtitle.setTextColor(getColorCompat(subtitleColor))
        }
    }

    private inner class Adapter : RecyclerView.Adapter<ViewHolder>() {
        override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): ViewHolder {
            val inflater = LayoutInflater.from(parent.context)
            val view = inflater.inflate(R.layout.simple_list_item, parent, false)
            view.setOnClickListener(onItemClickListener)
            return ViewHolder(view)
        }

        override fun onBindViewHolder(holder: ViewHolder, position: Int) {
            holder.bind(tracks.getTrack(position), position)
        }

        override fun getItemCount(): Int {
            return tracks.count
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

    private fun createCategoryQueryFactory(categoryType: String?, categoryId: Long): QueryFactory {
        if (isValidCategory(categoryType, categoryId)) {
            /* tracks for a specified category (album, artists, genres, etc */
            return object : QueryFactory() {
                override fun getRequeryMessage(): SocketMessage? {
                    return SocketMessage.Builder
                        .request(Messages.Request.QueryTracksByCategory)
                        .addOption(Messages.Key.CATEGORY, categoryType)
                        .addOption(Messages.Key.ID, categoryId)
                        .addOption(Messages.Key.COUNT_ONLY, true)
                        .addOption(Messages.Key.FILTER, lastFilter)
                        .build()
                }

                override fun getPageAroundMessage(offset: Int, limit: Int): SocketMessage? {
                    return SocketMessage.Builder
                        .request(Messages.Request.QueryTracksByCategory)
                        .addOption(Messages.Key.CATEGORY, categoryType)
                        .addOption(Messages.Key.ID, categoryId)
                        .addOption(Messages.Key.OFFSET, offset)
                        .addOption(Messages.Key.LIMIT, limit)
                        .addOption(Messages.Key.FILTER, lastFilter)
                        .build()
                }

                override fun connectionRequired(): Boolean {
                    return Messages.Category.OFFLINE == categoryType
                }
            }
        }
        else {
            /* all tracks */
            return object : QueryFactory() {
                override fun getRequeryMessage(): SocketMessage? {
                    return SocketMessage.Builder
                        .request(Messages.Request.QueryTracks)
                        .addOption(Messages.Key.FILTER, lastFilter)
                        .addOption(Messages.Key.COUNT_ONLY, true)
                        .build()
                }

                override fun getPageAroundMessage(offset: Int, limit: Int): SocketMessage? {
                    return SocketMessage.Builder
                        .request(Messages.Request.QueryTracks)
                        .addOption(Messages.Key.OFFSET, offset)
                        .addOption(Messages.Key.LIMIT, limit)
                        .addOption(Messages.Key.FILTER, lastFilter)
                        .build()
                }
            }
        }
    }

    private val slidingWindowListener = object : TrackListSlidingWindow.OnMetadataLoadedListener {
        override fun onReloaded(count: Int) {
            emptyView.update(getWebSocketService().state, count)
        }

        override fun onMetadataLoaded(offset: Int, count: Int) {}
    }

    companion object {
        private val EXTRA_CATEGORY_TYPE = "extra_category_type"
        private val EXTRA_SELECTED_ID = "extra_selected_id"
        private val EXTRA_TITLE_ID = "extra_title_id"

        fun getStartIntent(context: Context, type: String, id: Long): Intent {
            return Intent(context, TrackListActivity::class.java)
                .putExtra(EXTRA_CATEGORY_TYPE, type)
                .putExtra(EXTRA_SELECTED_ID, id)
        }

        fun getOfflineStartIntent(context: Context): Intent {
            return getStartIntent(context, Messages.Category.OFFLINE, 0)
                .putExtra(EXTRA_TITLE_ID, R.string.offline_tracks_title)
        }

        fun getStartIntent(context: Context, type: String, id: Long, categoryValue: String): Intent {
            val intent = getStartIntent(context, type, id)

            if (Strings.notEmpty(categoryValue)) {
                intent.putExtra(
                    EXTRA_ACTIVITY_TITLE,
                    context.getString(R.string.songs_from_category, categoryValue))
            }

            return intent
        }

        fun getStartIntent(context: Context): Intent {
            return Intent(context, TrackListActivity::class.java)
        }

        private fun isValidCategory(categoryType: String?, categoryId: Long): Boolean {
            return categoryType != null && categoryType.isNotEmpty() && categoryId != -1L
        }
    }
}
