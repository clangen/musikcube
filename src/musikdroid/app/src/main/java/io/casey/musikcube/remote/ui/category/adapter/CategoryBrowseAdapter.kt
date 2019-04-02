package io.casey.musikcube.remote.ui.category.adapter

import android.content.SharedPreferences
import android.text.TextUtils
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.TextView
import androidx.recyclerview.widget.RecyclerView
import io.casey.musikcube.remote.R
import io.casey.musikcube.remote.service.websocket.model.ICategoryValue
import io.casey.musikcube.remote.ui.category.constant.NavigationType
import io.casey.musikcube.remote.ui.shared.extension.fallback
import io.casey.musikcube.remote.ui.shared.extension.getColorCompat
import io.casey.musikcube.remote.ui.shared.extension.titleEllipsizeMode
import io.casey.musikcube.remote.ui.shared.mixin.PlaybackMixin

class CategoryBrowseAdapter(private val listener: EventListener,
                            private val playback: PlaybackMixin,
                            private val navigationType: NavigationType,
                            private val category: String,
                            prefs: SharedPreferences)
    : RecyclerView.Adapter<CategoryBrowseAdapter.ViewHolder>()
{
    interface EventListener {
        fun onItemClicked(value: ICategoryValue)
        fun onActionClicked(view: View, value: ICategoryValue)
    }

    private var model: List<ICategoryValue> = ArrayList()
    private val ellipsizeMode = titleEllipsizeMode(prefs)

    internal fun setModel(model: List<ICategoryValue>?) {
        this.model = model ?: ArrayList()
        notifyDataSetChanged()
    }

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): ViewHolder {
        val inflater = LayoutInflater.from(parent.context)
        val view = inflater.inflate(R.layout.simple_list_item, parent, false)
        val action = view.findViewById<View>(R.id.action)
        view.setOnClickListener{ v -> listener.onItemClicked(v.tag as ICategoryValue) }
        action.setOnClickListener{ v -> listener.onActionClicked(v, v.tag as ICategoryValue) }
        return ViewHolder(view, playback, navigationType, category, ellipsizeMode)
    }

    override fun onBindViewHolder(holder: ViewHolder, position: Int) {
        holder.bind(model[position])
    }

    override fun getItemCount(): Int = model.size

    class ViewHolder internal constructor(
            itemView: View,
            private val playback: PlaybackMixin,
            private val navigationType: NavigationType,
            private val category: String,
            ellipsizeMode: TextUtils.TruncateAt) : RecyclerView.ViewHolder(itemView)
    {
        private val title: TextView = itemView.findViewById(R.id.title)
        private val action: View = itemView.findViewById(R.id.action)

        init {
            itemView.findViewById<View>(R.id.subtitle).visibility = View.GONE
            title.ellipsize = ellipsizeMode
        }

        internal fun bind(categoryValue: ICategoryValue) {
            action.tag = categoryValue

            if (navigationType != NavigationType.Select && OVERFLOW_SUPPORTED.contains(category)) {
                action.visibility = View.VISIBLE
            }
            else {
                action.visibility = View.GONE
            }

            val playing = playback.service.playingTrack
            val playingId = playing.getCategoryId(category)

            var titleColor = R.color.theme_foreground
            if (playingId > 0 && categoryValue.id == playingId) {
                titleColor = R.color.theme_green
            }

            title.text = fallback(categoryValue.value, R.string.unknown_value)
            title.setTextColor(getColorCompat(titleColor))
            itemView.tag = categoryValue
        }
    }

    companion object {
        private val OVERFLOW_SUPPORTED = setOf("album", "artist", "album_artist", "genre", "playlists")
    }
}