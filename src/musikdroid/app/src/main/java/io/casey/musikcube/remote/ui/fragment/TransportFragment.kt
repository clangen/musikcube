package io.casey.musikcube.remote.ui.fragment

import android.content.Intent
import android.os.Bundle
import android.support.v4.app.Fragment
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.TextView

import io.casey.musikcube.remote.MainActivity
import io.casey.musikcube.remote.R
import io.casey.musikcube.remote.playback.Metadata
import io.casey.musikcube.remote.playback.PlaybackService
import io.casey.musikcube.remote.playback.PlaybackServiceFactory
import io.casey.musikcube.remote.playback.PlaybackState
import io.casey.musikcube.remote.ui.activity.PlayQueueActivity
import io.casey.musikcube.remote.ui.extension.getColorCompat

class TransportFragment : Fragment() {
    private var rootView: View? = null
    private var buffering: View? = null
    private var title: TextView? = null
    private var playPause: TextView? = null

    interface OnModelChangedListener {
        fun onChanged(fragment: TransportFragment)
    }

    override fun onCreateView(inflater: LayoutInflater?,
                              container: ViewGroup?,
                              savedInstanceState: Bundle?): View?
    {
        this.rootView = inflater!!.inflate(R.layout.fragment_transport, container, false)
        bindEventHandlers()
        rebindUi()
        return this.rootView
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        this.playbackService = PlaybackServiceFactory.instance(activity)
    }

    override fun onPause() {
        super.onPause()
        this.playbackService?.disconnect(playbackListener)
    }

    override fun onResume() {
        super.onResume()
        rebindUi()
        this.playbackService?.connect(playbackListener)
    }

    var playbackService: PlaybackService? = null
        private set

    var modelChangedListener: OnModelChangedListener? = null
        set(value) {
            field = value
        }

    private fun bindEventHandlers() {
        this.title = this.rootView?.findViewById<TextView>(R.id.track_title)
        this.buffering = this.rootView?.findViewById<View>(R.id.buffering)

        val titleBar = this.rootView?.findViewById<View>(R.id.title_bar)

        titleBar?.setOnClickListener { _: View ->
            if (playbackService?.playbackState != PlaybackState.Stopped) {
                val intent = PlayQueueActivity
                    .getStartIntent(activity, playbackService?.queuePosition ?: 0)
                    .addFlags(Intent.FLAG_ACTIVITY_SINGLE_TOP)

                startActivity(intent)
            }
        }

        this.title?.setOnLongClickListener { _: View ->
            startActivity(MainActivity.getStartIntent(activity))
            true
        }

        this.rootView?.findViewById<View>(R.id.button_prev)?.setOnClickListener { _: View -> playbackService?.prev() }

        this.playPause = this.rootView?.findViewById<TextView>(R.id.button_play_pause)

        this.playPause?.setOnClickListener { _: View ->
            if (playbackService?.playbackState == PlaybackState.Stopped) {
                playbackService?.playAll()
            }
            else {
                playbackService?.pauseOrResume()
            }
        }

        this.rootView?.findViewById<View>(R.id.button_next)?.setOnClickListener { _: View -> playbackService?.next() }
    }

    private fun rebindUi() {
        val state = playbackService?.playbackState

        val playing = state == PlaybackState.Playing
        val buffering = state == PlaybackState.Buffering

        this.playPause?.setText(if (playing) R.string.button_pause else R.string.button_play)
        this.buffering?.visibility = if (buffering) View.VISIBLE else View.GONE

        if (state == PlaybackState.Stopped) {
            title?.setTextColor(getColorCompat(R.color.theme_disabled_foreground))
            title?.setText(R.string.transport_not_playing)
        }
        else {
            title?.setTextColor(getColorCompat(R.color.theme_green))

            val defaultValue = getString(if (buffering) R.string.buffering else R.string.unknown_title)
            title?.text = playbackService?.getTrackString(Metadata.Track.TITLE, defaultValue)
        }
    }

    private val playbackListener: () -> Unit = {
        rebindUi()
        modelChangedListener?.onChanged(this@TransportFragment)
    }

    companion object {
        val TAG = "TransportFragment"

        fun newInstance(): TransportFragment {
            return TransportFragment()
        }
    }
}
