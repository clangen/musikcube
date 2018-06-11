package io.casey.musikcube.remote.ui.shared.fragment

import android.content.Intent
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.TextView
import io.casey.musikcube.remote.R
import io.casey.musikcube.remote.service.playback.PlaybackState
import io.casey.musikcube.remote.ui.home.activity.MainActivity
import io.casey.musikcube.remote.ui.playqueue.activity.PlayQueueActivity
import io.casey.musikcube.remote.ui.shared.extension.fallback
import io.casey.musikcube.remote.ui.shared.extension.getColorCompat
import io.casey.musikcube.remote.ui.shared.mixin.PlaybackMixin

class TransportFragment: BaseFragment() {
    private lateinit var rootView: View
    private lateinit var buffering: View
    private lateinit var title: TextView
    private lateinit var playPause: TextView

    lateinit var playback: PlaybackMixin
        private set

    interface OnModelChangedListener {
        fun onChanged(fragment: TransportFragment)
    }

    override fun onCreateView(
        inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View?
    {
        this.rootView = inflater.inflate(R.layout.fragment_transport, container, false)
        bindEventHandlers()
        rebindUi()
        return this.rootView
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        playback = mixin(PlaybackMixin(playbackListener))
        super.onCreate(savedInstanceState)
    }

    override fun onResume() {
        super.onResume()
        rebindUi()
    }

    var modelChangedListener: OnModelChangedListener? = null

    private fun bindEventHandlers() {
        this.title = this.rootView.findViewById(R.id.track_title)
        this.title.isClickable = false
        this.title.isFocusable = false

        this.buffering = this.rootView.findViewById(R.id.buffering)

        val titleBar = this.rootView.findViewById<View>(R.id.title_bar)

        titleBar?.setOnClickListener { _: View ->
            if (playback.service.state != PlaybackState.Stopped) {
                activity?.let {
                    startActivity(PlayQueueActivity
                        .getStartIntent(it, playback.service.queuePosition)
                        .addFlags(Intent.FLAG_ACTIVITY_SINGLE_TOP))

                    it.overridePendingTransition(R.anim.slide_up, R.anim.stay_put)
                }
            }
        }

        titleBar?.setOnLongClickListener { _: View ->
            activity?.let {
                startActivity(MainActivity.getStartIntent(it))
                return@setOnLongClickListener true
            }
            false
        }

        this.rootView.findViewById<View>(R.id.button_prev)?.setOnClickListener { _: View -> playback.service.prev() }

        this.playPause = this.rootView.findViewById(R.id.button_play_pause)

        this.playPause.setOnClickListener { _: View ->
            if (playback.service.state == PlaybackState.Stopped) {
                playback.service.playAll()
            }
            else {
                playback.service.pauseOrResume()
            }
        }

        this.rootView.findViewById<View>(R.id.button_next)?.setOnClickListener { _: View -> playback.service.next() }
    }

    private fun rebindUi() {
        val state = playback.service.state

        val playing = state == PlaybackState.Playing
        val buffering = state == PlaybackState.Buffering

        this.playPause.setText(if (playing) R.string.button_pause else R.string.button_play)
        this.buffering.visibility = if (buffering) View.VISIBLE else View.GONE

        if (state == PlaybackState.Stopped) {
            title.setTextColor(getColorCompat(R.color.theme_disabled_foreground))
            title.setText(R.string.transport_not_playing)
        }
        else {
            val defaultValue = getString(if (buffering) R.string.buffering else R.string.unknown_title)
            title.text = fallback(playback.service.playingTrack.title, defaultValue)
            title.setTextColor(getColorCompat(R.color.theme_green))
        }
    }

    private val playbackListener: () -> Unit = {
        rebindUi()
        modelChangedListener?.onChanged(this@TransportFragment)
    }

    companion object {
        const val TAG = "TransportFragment"
        fun newInstance(): TransportFragment = TransportFragment()
    }
}
