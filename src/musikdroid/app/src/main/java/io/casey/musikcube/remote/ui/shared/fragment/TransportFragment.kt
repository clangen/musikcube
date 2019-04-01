package io.casey.musikcube.remote.ui.shared.fragment

import android.os.Bundle
import android.view.LayoutInflater
import android.view.MotionEvent
import android.view.View
import android.view.ViewGroup
import android.widget.TextView
import io.casey.musikcube.remote.R
import io.casey.musikcube.remote.service.playback.PlaybackState
import io.casey.musikcube.remote.ui.home.activity.MainActivity
import io.casey.musikcube.remote.ui.navigation.Navigate
import io.casey.musikcube.remote.ui.playqueue.fragment.PlayQueueFragment
import io.casey.musikcube.remote.ui.shared.extension.dpToPx
import io.casey.musikcube.remote.ui.shared.extension.fallback
import io.casey.musikcube.remote.ui.shared.extension.getColorCompat
import io.casey.musikcube.remote.ui.shared.extension.topOfStack
import io.casey.musikcube.remote.ui.shared.mixin.PlaybackMixin
import io.casey.musikcube.remote.ui.shared.view.InterceptTouchFrameLayout
import me.zhanghai.android.materialprogressbar.MaterialProgressBar

class TransportFragment: BaseFragment() {
    private lateinit var rootView: View
    private lateinit var buffering: View
    private lateinit var title: TextView
    private lateinit var playPause: TextView
    private lateinit var progress: MaterialProgressBar
    private val seekTracker = TouchTracker()
    private var seekOverride = -1

    lateinit var playback: PlaybackMixin
        private set

    var modelChangedListener: ((TransportFragment) -> Unit)? = null

    override fun onCreateView(
        inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View?
    {
        this.rootView = inflater.inflate(R.layout.transport_fragment, container, false)
        progress = this.rootView.findViewById(R.id.progress)
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
        scheduleUpdateTime()
    }

    override fun onPause() {
        super.onPause()
        handler.removeCallbacks(updateTimeRunnable)
    }

    private fun bindEventHandlers() {
        this.title = this.rootView.findViewById(R.id.track_title)
        this.title.isClickable = false
        this.title.isFocusable = false

        this.buffering = this.rootView.findViewById(R.id.buffering)

        val titleBar = this.rootView.findViewById<InterceptTouchFrameLayout>(R.id.title_bar)

        titleBar?.setOnClickListener {
            if (!seekTracker.processed && playback.service.state != PlaybackState.Stopped) {
                appCompatActivity.supportFragmentManager.run {
                    when (topOfStack != PlayQueueFragment.TAG) {
                        true -> Navigate.toPlayQueue(
                            playback.service.queuePosition,
                            appCompatActivity,
                            this@TransportFragment)
                        false ->
                            this.popBackStack()
                    }
                }
            }
        }

        titleBar?.setOnLongClickListener {
            if (!seekTracker.processed) {
                activity?.let { a ->
                    startActivity(MainActivity.getStartIntent(a))
                    return@setOnLongClickListener true
                }
            }
            false
        }

        titleBar?.setOnInterceptTouchEventListener(object: InterceptTouchFrameLayout.OnInterceptTouchEventListener {
            override fun onInterceptTouchEvent(view: InterceptTouchFrameLayout, event: MotionEvent, disallowIntercept: Boolean): Boolean {
                return when (event.action) {
                    MotionEvent.ACTION_DOWN,
                    MotionEvent.ACTION_MOVE,
                    MotionEvent.ACTION_CANCEL,
                    MotionEvent.ACTION_UP -> true
                    else -> false
                }
            }

            override fun onTouchEvent(view: InterceptTouchFrameLayout, event: MotionEvent): Boolean {
                seekTracker.update(event)
                if (seekTracker.processed) {
                    when (event.action) {
                        MotionEvent.ACTION_MOVE -> {
                            seekOverride = (
                                seekTracker.lastX.toFloat() /
                                view.width.toFloat() *
                                playback.service.duration.toFloat()).toInt()
                            rebindProgress()
                        }
                        MotionEvent.ACTION_UP -> {
                            if (seekOverride >= 0) {
                                playback.service.seekTo(seekOverride.toDouble())
                                seekOverride = -1
                            }
                        }
                    }
                }
                view.defaultOnTouchEvent(event)
                return true
            }
        })


        this.rootView.findViewById<View>(R.id.button_prev)?.setOnClickListener {
            playback.service.prev()
        }

        this.playPause = this.rootView.findViewById(R.id.button_play_pause)

        this.playPause.setOnClickListener {
            if (playback.service.state == PlaybackState.Stopped) {
                playback.service.playAll()
            }
            else {
                playback.service.pauseOrResume()
            }
        }

        this.rootView.findViewById<View>(R.id.button_next)?.setOnClickListener {
            playback.service.next()
        }
    }

    private fun rebindProgress() {
        if (playback.service.state == PlaybackState.Stopped) {
            progress.progress = 0
            progress.secondaryProgress = 0
        }
        else {
            val buffered = playback.service.bufferedTime.toInt()
            val total = playback.service.duration.toInt()
            progress.max = total
            progress.secondaryProgress = if (buffered >= total) 0 else buffered

            progress.progress = if (seekTracker.down && seekOverride >= 0) {
                seekOverride
            }
            else {
                playback.service.currentTime.toInt()
            }
        }
    }


    private fun rebindUi() {
        val state = playback.service.state

        val playing = state == PlaybackState.Playing
        val buffering = state == PlaybackState.Buffering

        this.playPause.setText(if (playing || buffering) R.string.button_pause else R.string.button_play)
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

        rebindProgress()
    }

    private fun scheduleUpdateTime() {
        handler.removeCallbacks(updateTimeRunnable)
        handler.postDelayed(updateTimeRunnable, 1000L)
    }

    private val updateTimeRunnable = Runnable {
        rebindProgress()
        scheduleUpdateTime()
    }

    private val playbackListener: () -> Unit = {
        rebindUi()
        modelChangedListener?.invoke(this@TransportFragment)
    }

    private class TouchTracker {
        var down = false
        var startX = 0
        var startY = 0
        var totalDx = 0
        var totalDy = 0
        var lastX = 0
        var lastY = 0

        fun reset() {
            down = false
            startX = 0
            startY = 0
            totalDx = 0
            totalDy = 0
            lastX = 0
            lastY = 0
        }

        fun update(ev: MotionEvent) {
            when (ev.action) {
                MotionEvent.ACTION_DOWN -> {
                    reset()
                    down = true
                    startX = ev.x.toInt()
                    startY = ev.y.toInt()
                    lastX = startX
                    lastY = startY
                }

                MotionEvent.ACTION_MOVE -> {
                    val x = ev.x.toInt()
                    val y = ev.y.toInt()
                    totalDx += Math.abs(lastX - x)
                    totalDy += Math.abs(lastY - y)
                    lastX = x
                    lastY = y
                }

                MotionEvent.ACTION_UP,
                MotionEvent.ACTION_CANCEL -> {
                    down = false
                }
            }
        }

        val processed: Boolean
            get() { return totalDx >= dpToPx(SEEK_SLOP_PX) }
    }

    companion object {
        const val TAG = "TransportFragment"
        private const val SEEK_SLOP_PX = 16
        fun create(): TransportFragment = TransportFragment()
    }
}
