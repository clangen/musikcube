package io.casey.musikcube.remote.ui.activity

import android.content.Context
import android.content.Intent
import android.os.Bundle
import android.support.v7.widget.RecyclerView
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.TextView
import com.pluscubed.recyclerfastscroll.RecyclerFastScroller
import io.casey.musikcube.remote.Application
import io.casey.musikcube.remote.R
import io.casey.musikcube.remote.playback.Metadata
import io.casey.musikcube.remote.playback.PlaybackService
import io.casey.musikcube.remote.ui.extension.*
import io.casey.musikcube.remote.ui.model.TrackListSlidingWindow
import io.casey.musikcube.remote.ui.view.EmptyListView
import io.casey.musikcube.remote.websocket.Messages
import io.casey.musikcube.remote.websocket.SocketMessage
import io.casey.musikcube.remote.websocket.WebSocketService
import org.json.JSONObject

class PlayQueueActivity : WebSocketActivityBase() {
    private var adapter: Adapter = Adapter()
    private var offlineQueue: Boolean = false
    private var playback: PlaybackService? = null
    private lateinit var tracks: TrackListSlidingWindow
    private lateinit var emptyView: EmptyListView

    override fun onCreate(savedInstanceState: Bundle?) {
        Application.mainComponent.inject(this)

        super.onCreate(savedInstanceState)

        playback = playbackService

        setContentView(R.layout.recycler_view_activity)

        val fastScroller = findViewById<RecyclerFastScroller>(R.id.fast_scroller)
        val recyclerView = findViewById<RecyclerView>(R.id.recycler_view)
        setupDefaultRecyclerView(recyclerView, fastScroller, adapter)

        val queryFactory = playback!!.playlistQueryFactory
        val message: SocketMessage? = queryFactory.getRequeryMessage()

        emptyView = findViewById<EmptyListView>(R.id.empty_list_view)
        emptyView.capability = EmptyListView.Capability.OfflineOk
        emptyView.emptyMessage = getString(R.string.play_queue_empty)
        emptyView.alternateView = recyclerView

        offlineQueue = (Messages.Category.OFFLINE == message?.getStringOption(Messages.Key.CATEGORY))

        tracks = TrackListSlidingWindow(recyclerView, fastScroller, getWebSocketService(), queryFactory)
        tracks.setInitialPosition(intent.getIntExtra(EXTRA_PLAYING_INDEX, -1))
        tracks.setOnMetadataLoadedListener(slidingWindowListener)

        setTitleFromIntent(R.string.play_queue_title)
        addTransportFragment()
        enableUpNavigation()
    }

    override fun onPause() {
        super.onPause()
        this.tracks.pause()
    }

    override fun onResume() {
        this.tracks.resume() /* needs to happen before */

        super.onResume()

        if (offlineQueue) {
            tracks.requery()
        }
    }

    override val webSocketServiceClient: WebSocketService.Client?
        get() = socketClient

    override val playbackServiceEventListener: (() -> Unit)?
        get() = playbackEvents

    private val onItemClickListener = View.OnClickListener { v ->
        if (v.tag is Int) {
            val index = v.tag as Int
            playback?.playAt(index)
        }
    }

    private val playbackEvents = {
        if (adapter.itemCount == 0) {
            tracks.requery()
        }
        adapter.notifyDataSetChanged()
    }

    private val socketClient = object : WebSocketService.Client {
        override fun onStateChanged(newState: WebSocketService.State, oldState: WebSocketService.State) {
            if (newState == WebSocketService.State.Connected) {
                tracks.requery()
            }
            else {
                emptyView.update(newState, adapter.itemCount)
            }
        }

        override fun onMessageReceived(message: SocketMessage) {}

        override fun onInvalidPassword() {}
    }

    private inner class ViewHolder internal constructor(itemView: View) : RecyclerView.ViewHolder(itemView) {
        private val title: TextView = itemView.findViewById<TextView>(R.id.title)
        private val subtitle: TextView = itemView.findViewById<TextView>(R.id.subtitle)
        private val trackNum: TextView = itemView.findViewById<TextView>(R.id.track_num)

        internal fun bind(entry: JSONObject?, position: Int) {
            trackNum.text = (position + 1).toString()
            itemView.tag = position
            var titleColor = R.color.theme_foreground
            var subtitleColor = R.color.theme_disabled_foreground

            if (entry == null) {
                title.text = "-"
                subtitle.text = "-"
            }
            else {
                val entryExternalId = entry.optString(Metadata.Track.EXTERNAL_ID, "")
                val playingExternalId = playback?.getTrackString(Metadata.Track.EXTERNAL_ID, "")

                if (entryExternalId == playingExternalId) {
                    titleColor = R.color.theme_green
                    subtitleColor = R.color.theme_yellow
                }

                title.text = entry.optString(Metadata.Track.TITLE, "-")
                subtitle.text = entry.optString(Metadata.Track.ALBUM_ARTIST, "-")
            }

            title.setTextColor(getColorCompat(titleColor))
            subtitle.setTextColor(getColorCompat(subtitleColor))
        }
    }

    private inner class Adapter : RecyclerView.Adapter<ViewHolder>() {
        override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): ViewHolder {
            val inflater = LayoutInflater.from(parent.context)
            val view = inflater.inflate(R.layout.play_queue_row, parent, false)
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

    private val slidingWindowListener = object : TrackListSlidingWindow.OnMetadataLoadedListener {
        override fun onReloaded(count: Int) {
            emptyView.update(getWebSocketService().state, count)
        }

        override fun onMetadataLoaded(offset: Int, count: Int) {}
    }

    companion object {
        private val EXTRA_PLAYING_INDEX = "extra_playing_index"

        fun getStartIntent(context: Context, playingIndex: Int): Intent {
            return Intent(context, PlayQueueActivity::class.java)
                .putExtra(EXTRA_PLAYING_INDEX, playingIndex)
        }
    }
}
