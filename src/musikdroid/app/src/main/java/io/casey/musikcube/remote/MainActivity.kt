package io.casey.musikcube.remote

import android.app.Dialog
import android.content.Context
import android.content.Intent
import android.content.SharedPreferences
import android.os.Bundle
import android.os.Handler
import android.support.design.widget.Snackbar
import android.support.v4.app.DialogFragment
import android.support.v4.content.ContextCompat
import android.support.v7.app.AlertDialog
import android.view.Menu
import android.view.MenuItem
import android.view.View
import android.widget.CheckBox
import android.widget.CompoundButton
import android.widget.SeekBar
import android.widget.TextView

import io.casey.musikcube.remote.playback.PlaybackService
import io.casey.musikcube.remote.playback.PlaybackState
import io.casey.musikcube.remote.playback.RepeatMode
import io.casey.musikcube.remote.ui.activity.AlbumBrowseActivity
import io.casey.musikcube.remote.ui.activity.CategoryBrowseActivity
import io.casey.musikcube.remote.ui.activity.PlayQueueActivity
import io.casey.musikcube.remote.ui.activity.SettingsActivity
import io.casey.musikcube.remote.ui.activity.TrackListActivity
import io.casey.musikcube.remote.ui.activity.WebSocketActivityBase
import io.casey.musikcube.remote.ui.fragment.InvalidPasswordDialogFragment
import io.casey.musikcube.remote.ui.extension.*
import io.casey.musikcube.remote.ui.view.MainMetadataView
import io.casey.musikcube.remote.util.Duration
import io.casey.musikcube.remote.websocket.Messages
import io.casey.musikcube.remote.websocket.Prefs
import io.casey.musikcube.remote.websocket.SocketMessage
import io.casey.musikcube.remote.websocket.WebSocketService

class MainActivity : WebSocketActivityBase() {
    private val handler = Handler()
    private var prefs: SharedPreferences? = null
    private var playback: PlaybackService? = null

    private var mainLayout: View? = null
    private var metadataView: MainMetadataView? = null
    private var playPause: TextView? = null
    private var currentTime: TextView? = null
    private var totalTime: TextView? = null
    private var connectedNotPlayingContainer: View? = null
    private var disconnectedButton: View? = null
    private var showOfflineButton: View? = null
    private var disconnectedContainer: View? = null
    private var shuffleCb: CheckBox? = null
    private var muteCb: CheckBox? = null
    private var repeatCb: CheckBox? = null
    private var disconnectedOverlay: View? = null
    private var seekbar: SeekBar? = null
    private var seekbarValue = -1
    private var blink = 0

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        prefs = this.getSharedPreferences(Prefs.NAME, Context.MODE_PRIVATE)
        playback = playbackService

        setContentView(R.layout.activity_main)
        bindEventListeners()

        if (!getWebSocketService().hasValidConnection()) {
            startActivity(SettingsActivity.getStartIntent(this))
        }
    }

    override fun onPause() {
        super.onPause()
        metadataView?.onPause()
        unbindCheckboxEventListeners()
        handler.removeCallbacks(updateTimeRunnable)
    }

    override fun onResume() {
        super.onResume()
        this.playback = playbackService
        metadataView?.onResume()
        bindCheckBoxEventListeners()
        rebindUi()
        scheduleUpdateTime(true)
    }

    override fun onRestoreInstanceState(savedInstanceState: Bundle) {
        super.onRestoreInstanceState(savedInstanceState)
    }

    override fun onCreateOptionsMenu(menu: Menu): Boolean {
        menuInflater.inflate(R.menu.main_menu, menu)
        return true
    }

    override fun onPrepareOptionsMenu(menu: Menu): Boolean {
        val connected = getWebSocketService().state === WebSocketService.State.Connected
        val streaming = isStreamingSelected

        menu.findItem(R.id.action_playlists).isEnabled = connected
        menu.findItem(R.id.action_genres).isEnabled = connected

        menu.findItem(R.id.action_remote_toggle).setIcon(
            if (streaming) R.mipmap.ic_toolbar_streaming else R.mipmap.ic_toolbar_remote)

        return super.onPrepareOptionsMenu(menu)
    }

    override fun onOptionsItemSelected(item: MenuItem): Boolean {
        when (item.itemId) {
            R.id.action_remote_toggle -> {
                togglePlaybackService()
                return true
            }

            R.id.action_settings -> {
                startActivity(SettingsActivity.getStartIntent(this))
                return true
            }

            R.id.action_genres -> {
                startActivity(CategoryBrowseActivity.getStartIntent(this, Messages.Category.GENRE))
                return true
            }

            R.id.action_playlists -> {
                startActivity(CategoryBrowseActivity.getStartIntent(
                    this, Messages.Category.PLAYLISTS, CategoryBrowseActivity.DeepLink.TRACKS))
                return true
            }

            R.id.action_offline_tracks -> {
                onOfflineTracksSelected()
                return true
            }
        }

        return super.onOptionsItemSelected(item)
    }

    override val webSocketServiceClient: WebSocketService.Client?
        get() = serviceClient

    override val playbackServiceEventListener: (() -> Unit)?
        get() = playbackEvents

    private fun onOfflineTracksSelected() {
        if (isStreamingSelected) {
            startActivity(TrackListActivity.getOfflineStartIntent(this))
        }
        else {
            val tag = SwitchToOfflineTracksDialog.TAG
            if (supportFragmentManager.findFragmentByTag(tag) == null) {
                SwitchToOfflineTracksDialog.newInstance().show(supportFragmentManager, tag)
            }
        }
    }

    private fun onConfirmSwitchToOfflineTracks() {
        togglePlaybackService()
        onOfflineTracksSelected()
    }

    private val isStreamingSelected: Boolean
        get() = prefs!!.getBoolean(
            Prefs.Key.STREAMING_PLAYBACK,
            Prefs.Default.STREAMING_PLAYBACK)

    private fun togglePlaybackService() {
        val streaming = isStreamingSelected

        if (streaming) {
            playback?.stop()
        }

        prefs?.edit()?.putBoolean(Prefs.Key.STREAMING_PLAYBACK, !streaming)?.apply()

        val messageId = if (streaming)
            R.string.snackbar_remote_enabled
        else
            R.string.snackbar_streaming_enabled

        showSnackbar(messageId)

        reloadPlaybackService()
        playback = playbackService

        supportInvalidateOptionsMenu()
        rebindUi()
    }

    private fun showSnackbar(stringId: Int) {
        val sb = Snackbar.make(mainLayout!!, stringId, Snackbar.LENGTH_LONG)
        val sbView = sb.view
        sbView.setBackgroundColor(ContextCompat.getColor(this, R.color.color_primary))
        val tv = sbView.findViewById(android.support.design.R.id.snackbar_text) as TextView
        tv.setTextColor(ContextCompat.getColor(this, R.color.theme_foreground))
        sb.show()
    }

    private fun bindCheckBoxEventListeners() {
        this.shuffleCb?.setOnCheckedChangeListener(shuffleListener)
        this.muteCb?.setOnCheckedChangeListener(muteListener)
        this.repeatCb?.setOnCheckedChangeListener(repeatListener)
    }

    /* onRestoreInstanceState() calls setChecked(), which has the side effect of
    running these callbacks. this screws up state, especially for the repeat checkbox */
    private fun unbindCheckboxEventListeners() {
        this.shuffleCb?.setOnCheckedChangeListener(null)
        this.muteCb?.setOnCheckedChangeListener(null)
        this.repeatCb?.setOnCheckedChangeListener(null)
    }

    private fun bindEventListeners() {
        this.mainLayout = findViewById(R.id.activity_main)
        this.metadataView = findViewById(R.id.main_metadata_view) as MainMetadataView
        this.shuffleCb = findViewById(R.id.check_shuffle) as CheckBox
        this.muteCb = findViewById(R.id.check_mute) as CheckBox
        this.repeatCb = findViewById(R.id.check_repeat) as CheckBox
        this.connectedNotPlayingContainer = findViewById(R.id.connected_not_playing)
        this.disconnectedButton = findViewById(R.id.disconnected_button)
        this.disconnectedContainer = findViewById(R.id.disconnected_container)
        this.disconnectedOverlay = findViewById(R.id.disconnected_overlay)
        this.showOfflineButton = findViewById(R.id.offline_tracks_button)
        this.playPause = findViewById(R.id.button_play_pause) as TextView
        this.currentTime = findViewById(R.id.current_time) as TextView
        this.totalTime = findViewById(R.id.total_time) as TextView
        this.seekbar = findViewById(R.id.seekbar) as SeekBar

        findViewById(R.id.button_prev).setOnClickListener { _: View -> playback?.prev() }

        findViewById(R.id.button_play_pause).setOnClickListener { _: View ->
            if (playback?.playbackState === PlaybackState.Stopped) {
                playback?.playAll()
            }
            else {
                playback?.pauseOrResume()
            }
        }

        findViewById(R.id.button_next).setOnClickListener { _: View -> playback?.next() }

        disconnectedButton?.setOnClickListener { _ -> getWebSocketService().reconnect() }

        seekbar?.setOnSeekBarChangeListener(object : SeekBar.OnSeekBarChangeListener {
            override fun onProgressChanged(seekBar: SeekBar, progress: Int, fromUser: Boolean) {
                if (fromUser) {
                    seekbarValue = progress
                    currentTime?.text = Duration.format(seekbarValue.toDouble())
                }
            }

            override fun onStartTrackingTouch(seekBar: SeekBar) {

            }

            override fun onStopTrackingTouch(seekBar: SeekBar) {
                if (seekbarValue != -1) {
                    playback?.seekTo(seekbarValue.toDouble())
                    seekbarValue = -1
                }
            }
        })

        findViewById(R.id.button_artists).setOnClickListener { _: View -> startActivity(CategoryBrowseActivity.getStartIntent(this, Messages.Category.ALBUM_ARTIST)) }

        findViewById(R.id.button_tracks).setOnClickListener { _: View -> startActivity(TrackListActivity.getStartIntent(this@MainActivity)) }

        findViewById(R.id.button_albums).setOnClickListener { _: View -> startActivity(AlbumBrowseActivity.getStartIntent(this@MainActivity)) }

        findViewById(R.id.button_play_queue).setOnClickListener { _ -> navigateToPlayQueue() }

        findViewById(R.id.metadata_container).setOnClickListener { _ ->
            if (playback?.queueCount ?: 0 > 0) {
                navigateToPlayQueue()
            }
        }

        disconnectedOverlay?.setOnClickListener { _ ->
            /* swallow input so user can't click on things while disconnected */
        }

        showOfflineButton?.setOnClickListener { _ -> onOfflineTracksSelected() }
    }

    private fun rebindUi() {
        if (this.playback == null) {
            throw IllegalStateException()
        }

        val playbackState = playback?.playbackState
        val streaming = prefs!!.getBoolean(Prefs.Key.STREAMING_PLAYBACK, Prefs.Default.STREAMING_PLAYBACK)
        val connected = getWebSocketService().state === WebSocketService.State.Connected
        val stopped = playbackState === PlaybackState.Stopped
        val playing = playbackState === PlaybackState.Playing
        val buffering = playbackState === PlaybackState.Buffering
        val showMetadataView = !stopped && (playback?.queueCount ?: 0) > 0

        /* bottom section: transport controls */
        this.playPause?.setText(if (playing || buffering) R.string.button_pause else R.string.button_play)

        this.connectedNotPlayingContainer?.visibility = if (connected && stopped) View.VISIBLE else View.GONE
        this.disconnectedOverlay?.visibility = if (connected || !stopped) View.GONE else View.VISIBLE

        val repeatMode = playback?.repeatMode
        val repeatChecked = repeatMode !== RepeatMode.None
        repeatCb?.text = getString(REPEAT_TO_STRING_ID[repeatMode] ?: R.string.unknown_value)
        repeatCb?.setCheckWithoutEvent(repeatChecked, this.repeatListener)

        this.shuffleCb?.text = getString(if (streaming) R.string.button_random else R.string.button_shuffle)
        shuffleCb?.setCheckWithoutEvent(playback?.isShuffled ?: false, shuffleListener)

        muteCb?.setCheckWithoutEvent(playback?.isMuted ?: false, muteListener)

        /* middle section: connected, disconnected, and metadata views */
        connectedNotPlayingContainer?.visibility = View.GONE
        disconnectedContainer?.visibility = View.GONE

        if (!showMetadataView) {
            metadataView?.hide()

            if (!connected) {
                disconnectedContainer?.visibility = View.VISIBLE
            }
            else if (stopped) {
                connectedNotPlayingContainer?.visibility = View.VISIBLE
            }
        }
        else {
            metadataView?.refresh()
        }
    }

    private fun clearUi() {
        metadataView?.clear()
        rebindUi()
    }

    private fun navigateToPlayQueue() {
        startActivity(PlayQueueActivity.getStartIntent(this@MainActivity, playback?.queuePosition ?: 0))
    }

    private fun scheduleUpdateTime(immediate: Boolean) {
        handler.removeCallbacks(updateTimeRunnable)
        handler.postDelayed(updateTimeRunnable, (if (immediate) 0 else 1000).toLong())
    }

    private val updateTimeRunnable = object: Runnable {
        override fun run() {
            val duration = playback?.duration ?: 0.0
            val current: Double = if (seekbarValue == -1) playback?.currentTime ?: 0.0 else seekbarValue.toDouble()

            currentTime?.text = Duration.format(current)
            totalTime?.text = Duration.format(duration)
            seekbar?.max = duration.toInt()
            seekbar?.progress = current.toInt()
            seekbar?.secondaryProgress = playback?.bufferedTime?.toInt() ?: 0

            var currentTimeColor = R.color.theme_foreground
            if (playback?.playbackState === PlaybackState.Paused) {
                currentTimeColor =
                    if (++blink % 2 == 0) R.color.theme_foreground
                    else R.color.theme_blink_foreground
            }

            currentTime?.setTextColor(ContextCompat.getColor(this@MainActivity, currentTimeColor))

            scheduleUpdateTime(false)
        }
    }

    private val muteListener = { _: CompoundButton, b: Boolean ->
        if (b != playback?.isMuted) {
            playback?.toggleMute()
        }
    }

    private val shuffleListener = { _: CompoundButton, b: Boolean ->
        if (b != playback?.isShuffled) {
            playback?.toggleShuffle()
        }
    }

    private fun onRepeatListener() {
        val currentMode = playback?.repeatMode

        var newMode = RepeatMode.None

        if (currentMode === RepeatMode.None) {
            newMode = RepeatMode.List
        }
        else if (currentMode === RepeatMode.List) {
            newMode = RepeatMode.Track
        }

        val checked = newMode !== RepeatMode.None
        repeatCb?.text = getString(REPEAT_TO_STRING_ID[newMode] ?: R.string.unknown_value)
        repeatCb?.setCheckWithoutEvent(checked, repeatListener)

        playback?.toggleRepeatMode()
    }

    private val repeatListener = { _: CompoundButton, _: Boolean ->
        onRepeatListener()
    }

    private val playbackEvents = { rebindUi() }

    private val serviceClient = object : WebSocketService.Client {
        override fun onStateChanged(newState: WebSocketService.State, oldState: WebSocketService.State) {
            if (newState === WebSocketService.State.Connected) {
                rebindUi()
            }
            else if (newState === WebSocketService.State.Disconnected) {
                clearUi()
            }
        }

        override fun onMessageReceived(message: SocketMessage) {}

        override fun onInvalidPassword() {
            val tag = InvalidPasswordDialogFragment.TAG
            if (supportFragmentManager.findFragmentByTag(tag) == null) {
                InvalidPasswordDialogFragment.newInstance().show(supportFragmentManager, tag)
            }
        }
    }

    class SwitchToOfflineTracksDialog : DialogFragment() {

        override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
            val dlg = AlertDialog.Builder(activity)
                .setTitle(R.string.main_switch_to_streaming_title)
                .setMessage(R.string.main_switch_to_streaming_message)
                .setNegativeButton(R.string.button_no, null)
                .setPositiveButton(R.string.button_yes) { _, _ -> (activity as MainActivity).onConfirmSwitchToOfflineTracks() }
                .create()

            dlg.setCancelable(false)
            return dlg
        }

        companion object {
            val TAG = "switch_to_offline_tracks_dialog"

            fun newInstance(): SwitchToOfflineTracksDialog {
                return SwitchToOfflineTracksDialog()
            }
        }
    }

    companion object {
        private var REPEAT_TO_STRING_ID: MutableMap<RepeatMode, Int> = mutableMapOf(
            RepeatMode.None to R.string.button_repeat_off,
            RepeatMode.List to R.string.button_repeat_list,
            RepeatMode.Track to R.string.button_repeat_track
        )

        fun getStartIntent(context: Context): Intent {
            return Intent(context, MainActivity::class.java)
                .addFlags(Intent.FLAG_ACTIVITY_SINGLE_TOP or Intent.FLAG_ACTIVITY_CLEAR_TOP)
        }
    }
}
