package io.casey.musikcube.remote.ui.albums.adapter

import android.content.SharedPreferences
import android.text.TextUtils
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.ImageView
import android.widget.TextView
import androidx.recyclerview.widget.RecyclerView
import com.bumptech.glide.load.engine.DiskCacheStrategy
import com.bumptech.glide.request.RequestOptions
import io.casey.musikcube.remote.R
import io.casey.musikcube.remote.injection.GlideApp
import io.casey.musikcube.remote.service.websocket.model.IAlbum
import io.casey.musikcube.remote.ui.shared.extension.fallback
import io.casey.musikcube.remote.ui.shared.extension.getColorCompat
import io.casey.musikcube.remote.ui.shared.extension.titleEllipsizeMode
import io.casey.musikcube.remote.ui.shared.mixin.PlaybackMixin
import io.casey.musikcube.remote.ui.shared.util.AlbumArtLookup.getUrl
import io.casey.musikcube.remote.ui.shared.util.Size

class AlbumBrowseAdapter(private val listener: EventListener,
                         private val playback: PlaybackMixin,
                         val prefs: SharedPreferences)
    : RecyclerView.Adapter<AlbumBrowseAdapter.ViewHolder>()
{
    interface EventListener {
        fun onItemClicked(album: IAlbum)
        fun onActionClicked(view: View, album: IAlbum)
    }

    private var model: List<IAlbum> = listOf()
    private val ellipsizeMode = titleEllipsizeMode(prefs)

    internal fun setModel(model: List<IAlbum>) {
        this.model = model
        notifyDataSetChanged()
    }

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): ViewHolder {
        val inflater = LayoutInflater.from(parent.context)
        val view = inflater.inflate(R.layout.simple_list_item, parent, false)
        val action = view.findViewById<View>(R.id.action)
        view.setOnClickListener { v -> listener.onItemClicked(v.tag as IAlbum) }
        action.setOnClickListener { v -> listener.onActionClicked(v, v.tag as IAlbum) }
        return ViewHolder(view, playback, ellipsizeMode)
    }

    override fun onBindViewHolder(holder: ViewHolder, position: Int) {
        holder.bind(model[position])
    }

    override fun getItemCount(): Int = model.size

    class ViewHolder internal constructor(
            itemView: View,
            val playback: PlaybackMixin,
            ellipsizeMode: TextUtils.TruncateAt)
                : RecyclerView.ViewHolder(itemView)
    {
        private val title = itemView.findViewById<TextView>(R.id.title)
        private val subtitle = itemView.findViewById<TextView>(R.id.subtitle)
        private val artwork = itemView.findViewById<ImageView>(R.id.artwork)
        private val action = itemView.findViewById<View>(R.id.action)

        init {
            title.ellipsize = ellipsizeMode
        }

        internal fun bind(album: IAlbum) {
            val playing = playback.service.playingTrack
            val playingId = playing.albumId

            var titleColor = R.color.theme_foreground
            var subtitleColor = R.color.theme_disabled_foreground

            if (playingId != -1L && album.id == playingId) {
                titleColor = R.color.theme_green
                subtitleColor = R.color.theme_yellow
            }

            artwork.visibility = View.VISIBLE

            GlideApp
                .with(itemView.context)
                .load(getUrl(album, Size.Large))
                .apply(OPTIONS)
                .into(artwork)

            title.text = fallback(album.value, "-")
            title.setTextColor(getColorCompat(titleColor))

            subtitle.text = fallback(album.albumArtist, "-")
            subtitle.setTextColor(getColorCompat(subtitleColor))
            itemView.tag = album
            action.tag = album
        }

        companion object {
            val OPTIONS = RequestOptions()
                .placeholder(R.drawable.ic_art_placeholder)
                .error(R.drawable.ic_art_placeholder)
                .diskCacheStrategy(DiskCacheStrategy.RESOURCE)
        }
    }
}