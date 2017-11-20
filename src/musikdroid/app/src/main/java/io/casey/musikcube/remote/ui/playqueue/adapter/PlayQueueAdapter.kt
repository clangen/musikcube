package io.casey.musikcube.remote.ui.playqueue.adapter

import android.support.v7.widget.RecyclerView
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.TextView
import io.casey.musikcube.remote.R
import io.casey.musikcube.remote.service.websocket.model.ITrack
import io.casey.musikcube.remote.ui.shared.extension.fallback
import io.casey.musikcube.remote.ui.shared.extension.getColorCompat
import io.casey.musikcube.remote.ui.shared.mixin.PlaybackMixin
import io.casey.musikcube.remote.ui.shared.model.TrackListSlidingWindow

class PlayQueueAdapter(val tracks: TrackListSlidingWindow,
                       val playback: PlaybackMixin,
                       val listener: EventListener): RecyclerView.Adapter<PlayQueueAdapter.ViewHolder>()
{
    interface EventListener {
        fun onItemClicked(position: Int)
        fun onActionClicked(view: View, value: ITrack)
    }

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): ViewHolder {
        val inflater = LayoutInflater.from(parent.context)
        val view = inflater.inflate(R.layout.play_queue_row, parent, false)
        val action = view.findViewById<View>(R.id.action)
        view.setOnClickListener{ v -> listener.onItemClicked(v.tag as Int) }
        action.setOnClickListener{ v -> listener.onActionClicked(v, v.tag as ITrack) }
        return ViewHolder(view)
    }

    override fun onBindViewHolder(holder: ViewHolder, position: Int) {
        holder.bind(tracks.getTrack(position), position)
    }

    override fun getItemCount(): Int = tracks.count

    inner class ViewHolder internal constructor(itemView: View) : RecyclerView.ViewHolder(itemView) {
        private val title = itemView.findViewById<TextView>(R.id.title)
        private val subtitle = itemView.findViewById<TextView>(R.id.subtitle)
        private val trackNum = itemView.findViewById<TextView>(R.id.track_num)
        private val action = itemView.findViewById<View>(R.id.action)

        internal fun bind(track: ITrack?, position: Int) {
            trackNum.text = (position + 1).toString()
            itemView.tag = position
            action.tag = track

            var titleColor = R.color.theme_foreground
            var subtitleColor = R.color.theme_disabled_foreground

            if (track == null) {
                title.text = "-"
                subtitle.text = "-"
            }
            else {
                val playing = playback.service.playingTrack
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
}