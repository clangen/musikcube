package io.casey.musikcube.remote.ui.tracks.adapter

import android.support.v7.widget.RecyclerView
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.TextView
import io.casey.musikcube.remote.R
import io.casey.musikcube.remote.service.playback.IPlaybackService
import io.casey.musikcube.remote.service.websocket.model.ITrack
import io.casey.musikcube.remote.ui.shared.extension.fallback
import io.casey.musikcube.remote.ui.shared.extension.getColorCompat
import io.casey.musikcube.remote.ui.shared.mixin.PlaybackMixin
import io.casey.musikcube.remote.ui.shared.model.TrackListSlidingWindow

class TrackListAdapter(private val tracks: TrackListSlidingWindow,
                       private val onItemClickListener: (View) -> Unit,
                       private val onActionClickListener: (View) -> Unit,
                       private var playback: PlaybackMixin) : RecyclerView.Adapter<TrackListAdapter.ViewHolder>()
{
    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): TrackListAdapter.ViewHolder {
        val inflater = LayoutInflater.from(parent.context)
        val view = inflater.inflate(R.layout.simple_list_item, parent, false)
        view.setOnClickListener(onItemClickListener)
        view.findViewById<View>(R.id.action).setOnClickListener(onActionClickListener)
        return ViewHolder(view, playback)
    }

    override fun onBindViewHolder(holder: ViewHolder, position: Int) {
        holder.bind(tracks.getTrack(position), position)
    }

    override fun getItemCount(): Int = tracks.count

    class ViewHolder internal constructor(private val view: View,
        private val playback: PlaybackMixin) : RecyclerView.ViewHolder(view)
    {
        private val title: TextView = view.findViewById(R.id.title)
        private val subtitle: TextView = view.findViewById(R.id.subtitle)
        private val action: View = view.findViewById(R.id.action)

        internal fun bind(track: ITrack?, position: Int) {
            itemView.tag = position
            action.tag = track

            var titleColor = R.color.theme_foreground
            var subtitleColor = R.color.theme_disabled_foreground

            if (track != null) {
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
            else {
                title.text = "-"
                subtitle.text = "-"
            }

            title.setTextColor(getColorCompat(titleColor))
            subtitle.setTextColor(getColorCompat(subtitleColor))
        }
    }
}
