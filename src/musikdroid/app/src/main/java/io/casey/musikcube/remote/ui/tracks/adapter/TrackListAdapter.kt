package io.casey.musikcube.remote.ui.tracks.adapter

import android.support.v7.widget.RecyclerView
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.TextView
import io.casey.musikcube.remote.R
import io.casey.musikcube.remote.service.websocket.model.ITrack
import io.casey.musikcube.remote.ui.shared.extension.fallback
import io.casey.musikcube.remote.ui.shared.extension.getColorCompat
import io.casey.musikcube.remote.ui.shared.extension.letMany
import io.casey.musikcube.remote.ui.shared.mixin.PlaybackMixin
import io.casey.musikcube.remote.ui.shared.model.DefaultSlidingWindow

class TrackListAdapter(private val tracks: DefaultSlidingWindow,
                       private val listener: EventListener?,
                       private var playback: PlaybackMixin) : RecyclerView.Adapter<TrackListAdapter.ViewHolder>()
{
    interface EventListener {
        fun onItemClick(view: View, track: ITrack, position: Int)
        fun onActionItemClick(view: View, track: ITrack, position: Int)
    }

    private data class Tag(var position: Int?, var track: ITrack?)

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): TrackListAdapter.ViewHolder {
        val inflater = LayoutInflater.from(parent.context)
        val view = inflater.inflate(R.layout.simple_list_item, parent, false)
        view.tag = Tag(null, null)

        view.setOnClickListener({ v ->
            val tag = v.tag as Tag
            letMany(listener, tag.track, tag.position) { listener, track, pos ->
                listener.onItemClick(v, track, pos)
            }
        })

        view.findViewById<View>(R.id.action).setOnClickListener({ v ->
            val tag = v.tag as Tag
            letMany(listener, tag.track, tag.position) { listener, track, position ->
                listener.onActionItemClick(v, track, position)
            }
        })

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
            val tag = itemView.tag as Tag
            tag.position = position
            tag.track = track
            itemView.tag = tag
            action.tag = tag

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
