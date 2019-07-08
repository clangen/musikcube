package io.casey.musikcube.remote.ui.shared.view

import android.app.Activity
import android.content.Context
import android.util.AttributeSet
import android.view.LayoutInflater
import android.view.View
import android.widget.FrameLayout
import android.widget.TextView
import io.casey.musikcube.remote.Application
import io.casey.musikcube.remote.R
import io.casey.musikcube.remote.injection.DaggerViewComponent
import io.casey.musikcube.remote.service.playback.PlaybackServiceFactory
import io.casey.musikcube.remote.service.playback.impl.streaming.StreamingPlaybackService
import io.casey.musikcube.remote.service.websocket.WebSocketService
import io.casey.musikcube.remote.service.websocket.model.IMetadataProxy
import io.casey.musikcube.remote.ui.shared.extension.setVisible
import io.casey.musikcube.remote.ui.tracks.activity.TrackListActivity
import javax.inject.Inject
import io.casey.musikcube.remote.service.websocket.WebSocketService.State as WebSocketState

class EmptyListView : FrameLayout {
    enum class Capability { OnlineOnly, OfflineOk }

    @Inject lateinit var wss: WebSocketService

    private var mainView: View? = null
    private var emptyTextView: TextView? = null
    private var emptyContainer: View? = null
    private var offlineContainer: View? = null
    private var viewOfflineButton: View? = null
    private var reconnectButton: View? = null

    constructor(context: Context)
    : super(context) {
        initialize()
    }

    constructor(context: Context, attrs: AttributeSet?)
    : super(context, attrs) {
        initialize()
    }
    constructor(context: Context, attrs: AttributeSet?, defStyleAttr: Int)
    : super(context, attrs, defStyleAttr) {
        initialize()
    }

    var capability: Capability = Capability.OnlineOnly

    var alternateView: View? = null
        set(value) {
            field = value
            alternateView?.visibility = when (visibility == View.VISIBLE) {
                true -> View.GONE
                false -> View.VISIBLE
            }
        }

    var emptyMessage: String
        get() {
            return emptyTextView?.text.toString()
        }
        set(message) {
            emptyTextView?.text = message
        }

    fun update(state: WebSocketState, count: Int) { /* TODO REMOVE ME! */
        if (count > 0) {
            visibility = View.INVISIBLE
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

        alternateView?.visibility = when (visibility == View.VISIBLE) {
            true -> View.GONE
            false -> View.VISIBLE
        }
    }

    fun update(state: IMetadataProxy.State, count: Int) {
        if (count > 0) {
            visibility = View.INVISIBLE
        }
        else {
            visibility = View.VISIBLE

            val showOfflineContainer =
                capability == Capability.OnlineOnly &&
                    PlaybackServiceFactory.instance(context) is StreamingPlaybackService &&
                    state != IMetadataProxy.State.Connected

            offlineContainer?.setVisible(showOfflineContainer)
            emptyContainer?.setVisible(!showOfflineContainer)
        }

        alternateView?.visibility = when (visibility == View.INVISIBLE) {
            true -> View.VISIBLE
            false -> View.INVISIBLE
        }
    }

    private fun initialize() {
        DaggerViewComponent.builder()
            .appComponent(Application.appComponent)
            .build().inject(this)

        val inflater = LayoutInflater.from(context)

        mainView = inflater.inflate(R.layout.empty_list_view, this, false)
        emptyContainer = mainView?.findViewById(R.id.empty_container)
        offlineContainer = mainView?.findViewById(R.id.offline_container)
        viewOfflineButton = mainView?.findViewById(R.id.offline_tracks_button)
        reconnectButton = mainView?.findViewById(R.id.disconnected_button)
        emptyTextView = mainView?.findViewById(R.id.empty_text_view)

        addView(mainView)

        reconnectButton?.setOnClickListener {
            wss.reconnect()
        }

        viewOfflineButton?.setOnClickListener {
            val activity = context as Activity
            activity.startActivity(TrackListActivity.getOfflineStartIntent(activity))
            activity.finish()
        }
    }
}