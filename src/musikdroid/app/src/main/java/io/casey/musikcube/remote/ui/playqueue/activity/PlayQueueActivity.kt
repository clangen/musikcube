package io.casey.musikcube.remote.ui.playqueue.activity

import android.content.Context
import android.content.Intent
import android.os.Bundle
import android.support.v7.widget.RecyclerView
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.TextView
import com.simplecityapps.recyclerview_fastscroll.views.FastScrollRecyclerView
import io.casey.musikcube.remote.R
import io.casey.musikcube.remote.service.websocket.model.IDataProvider
import io.casey.musikcube.remote.service.websocket.model.ITrack
import io.casey.musikcube.remote.service.playback.IPlaybackService
import io.casey.musikcube.remote.ui.shared.extension.*
import io.casey.musikcube.remote.ui.shared.model.TrackListSlidingWindow
import io.casey.musikcube.remote.ui.shared.activity.BaseActivity
import io.casey.musikcube.remote.ui.shared.view.EmptyListView

class PlayQueueActivity : BaseActivity() {
    private var adapter: Adapter = Adapter()
    private var offlineQueue: Boolean = false
    private var playback: IPlaybackService? = null
    private lateinit var tracks: TrackListSlidingWindow
    private lateinit var emptyView: EmptyListView

    override fun onCreate(savedInstanceState: Bundle?) {
        component.inject(this)

        super.onCreate(savedInstanceState)

        playback = playbackService

        setContentView(R.layout.recycler_view_activity)

        val recyclerView = findViewById<FastScrollRecyclerView>(R.id.recycler_view)
        setupDefaultRecyclerView(recyclerView, adapter)

        emptyView = findViewById(R.id.empty_list_view)
        emptyView.capability = EmptyListView.Capability.OfflineOk
        emptyView.emptyMessage = getString(R.string.play_queue_empty)
        emptyView.alternateView = recyclerView

        val queryFactory = playback!!.playlistQueryFactory
        offlineQueue = playback!!.playlistQueryFactory.offline()

        tracks = TrackListSlidingWindow(recyclerView, dataProvider, queryFactory)
        tracks.setInitialPosition(intent.getIntExtra(EXTRA_PLAYING_INDEX, -1))
        tracks.setOnMetadataLoadedListener(slidingWindowListener)

        dataProvider.observeState().subscribe(
            { states ->
                if (states.first == IDataProvider.State.Connected) {
                    tracks.requery()
                }
                else {
                    emptyView.update(states.first, adapter.itemCount)
                }
            }, { /* error */ })

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

    private inner class ViewHolder internal constructor(itemView: View) : RecyclerView.ViewHolder(itemView) {
        private val title: TextView = itemView.findViewById(R.id.title)
        private val subtitle: TextView = itemView.findViewById(R.id.subtitle)
        private val trackNum: TextView = itemView.findViewById(R.id.track_num)

        internal fun bind(track: ITrack?, position: Int) {
            trackNum.text = (position + 1).toString()
            itemView.tag = position
            var titleColor = R.color.theme_foreground
            var subtitleColor = R.color.theme_disabled_foreground

            if (track == null) {
                title.text = "-"
                subtitle.text = "-"
            }
            else {
                val playing = playback!!.playingTrack
                val entryExternalId = track.externalId
                val playingExternalId = playing.externalId

                if (entryExternalId == playingExternalId) {
                    titleColor = R.color.theme_green
                    subtitleColor = R.color.theme_yellow
                }

                title.text = fallback(track.title, "-")
                subtitle.text = fallback(track.albumArtist, "-")
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
            emptyView.update(dataProvider.state, count)
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
