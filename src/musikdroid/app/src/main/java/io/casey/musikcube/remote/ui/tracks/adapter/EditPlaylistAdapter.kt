package io.casey.musikcube.remote.ui.tracks.adapter

import android.content.SharedPreferences
import android.support.v7.widget.RecyclerView
import android.support.v7.widget.helper.ItemTouchHelper
import android.text.TextUtils
import android.view.LayoutInflater
import android.view.MotionEvent
import android.view.View
import android.view.ViewGroup
import android.widget.TextView
import io.casey.musikcube.remote.R
import io.casey.musikcube.remote.service.websocket.model.ITrack
import io.casey.musikcube.remote.ui.shared.extension.fallback
import io.casey.musikcube.remote.ui.shared.extension.titleEllipsizeMode
import io.casey.musikcube.remote.ui.tracks.model.EditPlaylistViewModel

class EditPlaylistAdapter(
        private val viewModel: EditPlaylistViewModel,
        private val touchHelper: ItemTouchHelper,
        prefs: SharedPreferences)
            : RecyclerView.Adapter<EditPlaylistAdapter.ViewHolder>()
{
    private val ellipsizeMode = titleEllipsizeMode(prefs)

    override fun getItemCount(): Int {
        return viewModel.count
    }

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): ViewHolder {
        val inflater = LayoutInflater.from(parent.context)
        val view = inflater.inflate(R.layout.edit_playlist_track_row, parent, false)
        val holder = ViewHolder(view, ellipsizeMode)
        val drag = view.findViewById<View>(R.id.dragHandle)
        val swipe = view.findViewById<View>(R.id.swipeHandle)
        view.setOnClickListener(emptyClickListener)
        drag.setOnTouchListener(dragTouchHandler)
        swipe.setOnTouchListener(dragTouchHandler)
        drag.tag = holder
        swipe.tag = holder
        return holder
    }

    override fun onBindViewHolder(holder: ViewHolder, position: Int) {
        holder.bind(viewModel[position])
    }

    private val emptyClickListener = object: View.OnClickListener {
        override fun onClick(view: View?) {
            /* we do this so we get a ripple effect when the user touches the view,
            so there's an indication something is happening before the drag starts */
        }
    }

    private val dragTouchHandler = object: View.OnTouchListener {
        override fun onTouch(view: View, event: MotionEvent?): Boolean {
            if (event?.actionMasked == MotionEvent.ACTION_DOWN) {
                if (view.id == R.id.dragHandle) {
                    touchHelper.startDrag(view.tag as RecyclerView.ViewHolder)
                }
                else if (view.id == R.id.swipeHandle) {
                    touchHelper.startSwipe(view.tag as RecyclerView.ViewHolder)
                    return true
                }
            }
            return false
        }
    }

    class ViewHolder internal constructor(view: View, ellipsizeMode: TextUtils.TruncateAt) : RecyclerView.ViewHolder(view) {
        private val title = itemView.findViewById<TextView>(R.id.title)
        private val subtitle = itemView.findViewById<TextView>(R.id.subtitle)

        init {
            title.ellipsize = ellipsizeMode
        }

        fun bind(track: ITrack) {
            title.text = fallback(track.title, "-")
            subtitle.text = fallback(track.albumArtist, "-")
        }
    }
}