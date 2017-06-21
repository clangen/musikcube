package io.casey.musikcube.remote.ui.view

import android.app.Activity
import android.content.Context
import android.util.AttributeSet
import android.view.LayoutInflater
import android.view.View
import android.widget.FrameLayout
import android.widget.TextView
import io.casey.musikcube.remote.R
import io.casey.musikcube.remote.playback.PlaybackServiceFactory
import io.casey.musikcube.remote.playback.StreamingPlaybackService
import io.casey.musikcube.remote.ui.activity.TrackListActivity
import io.casey.musikcube.remote.ui.extension.*
import io.casey.musikcube.remote.websocket.WebSocketService
import io.casey.musikcube.remote.websocket.WebSocketService.State as WebSocketState

class EmptyListView : FrameLayout {
    enum class Capability { OnlineOnly, OfflineOk }

    private var mainView: View? = null
    private var emptyTextView: TextView? = null
    private var emptyContainer: View? = null
    private var offlineContainer: View? = null
    private var viewOfflineButton: View? = null
    private var reconnectButton: View? = null

    constructor(context: Context?)
    : super(context) {
        initialize()
    }

    constructor(context: Context?, attrs: AttributeSet?)
    : super(context, attrs) {
        initialize()
    }
    constructor(context: Context?, attrs: AttributeSet?, defStyleAttr: Int)
    : super(context, attrs, defStyleAttr) {
        initialize()
    }

    var capability: Capability = Capability.OnlineOnly
        set(value) {
            field = value
        }

    var alternateView: View? = null
        get() {
            return field
        }
        set(value) {
            field = value
            alternateView?.visibility = if (visibility == View.GONE) View.VISIBLE else View.GONE
        }

    var emptyMessage: String
        get() {
            return emptyTextView?.text.toString()
        }
        set(message) {
            emptyTextView?.text = message
        }

    fun update(state: WebSocketState, count: Int) {
        if (count > 0) {
            visibility = View.GONE
        }
        else {
            visibility = View.VISIBLE

            val showOfflineContainer =
                capability == Capability.OnlineOnly &&
                PlaybackServiceFactory.instance(context) is StreamingPlaybackService &&
                state != WebSocketState.Connected

            offlineContainer?.setVisible(showOfflineContainer)
            emptyContainer?.setVisible(!showOfflineContainer)
        }

        alternateView?.visibility = if (visibility == View.GONE) View.VISIBLE else View.GONE
    }

    private fun initialize() {
        val inflater = LayoutInflater.from(context)

        mainView = inflater.inflate(R.layout.empty_list_view, this, false)
        emptyContainer = mainView?.findViewById(R.id.empty_container)
        offlineContainer = mainView?.findViewById(R.id.offline_container)
        viewOfflineButton = mainView?.findViewById(R.id.offline_tracks_button)
        reconnectButton = mainView?.findViewById(R.id.disconnected_button)
        emptyTextView = mainView?.findViewById(R.id.empty_text_view) as TextView

        addView(mainView)

        reconnectButton?.setOnClickListener { _ ->
            WebSocketService.getInstance(context).reconnect()
        }

        viewOfflineButton?.setOnClickListener { _ ->
            val activity = context as Activity
            activity.startActivity(TrackListActivity.getOfflineStartIntent(activity))
            activity.finish()
        }
    }
}