package io.casey.musikcube.remote

import android.app.Dialog
import android.content.Context
import android.content.Intent
import android.content.SharedPreferences
import android.net.Uri
import android.os.Bundle
import android.os.Handler
import android.support.v4.app.DialogFragment
import android.support.v7.app.AlertDialog
import android.view.LayoutInflater
import android.view.Menu
import android.view.MenuItem
import android.view.View
import android.widget.CheckBox
import android.widget.CompoundButton
import android.widget.SeekBar
import android.widget.TextView
import io.casey.musikcube.remote.data.IDataProvider
import io.casey.musikcube.remote.playback.PlaybackService
import io.casey.musikcube.remote.playback.PlaybackState
import io.casey.musikcube.remote.playback.RepeatMode
import io.casey.musikcube.remote.ui.activity.*
import io.casey.musikcube.remote.ui.extension.getColorCompat
import io.casey.musikcube.remote.ui.extension.setCheckWithoutEvent
import io.casey.musikcube.remote.ui.extension.showSnackbar
import io.casey.musikcube.remote.ui.fragment.InvalidPasswordDialogFragment
import io.casey.musikcube.remote.ui.model.UpdateCheck
import io.casey.musikcube.remote.ui.view.MainMetadataView
import io.casey.musikcube.remote.util.Duration
import io.casey.musikcube.remote.websocket.Messages
import io.casey.musikcube.remote.websocket.Prefs
import io.casey.musikcube.remote.websocket.SocketMessage
import io.casey.musikcube.remote.websocket.WebSocketService

class MainActivity : WebSocketActivityBase() {
    private val handler = Handler()
    private lateinit var prefs: SharedPreferences
    private var playback: PlaybackService? = null

    private var updateCheck: UpdateCheck = UpdateCheck()
    private var seekbarValue = -1
    private var blink = 0

    /* views */
    private lateinit var mainLayout: View
    private lateinit var metadataView: MainMetadataView
    private lateinit var playPause: TextView
    private lateinit var currentTime: TextView
    private lateinit var totalTime: TextView
    private lateinit var connectedNotPlayingContainer: View
    private lateinit var disconnectedButton: View
    private lateinit var showOfflineButton: View
    private lateinit var disconnectedContainer: View
    private lateinit var shuffleCb: CheckBox
    private lateinit var muteCb: CheckBox
    private lateinit var repeatCb: CheckBox
    private lateinit var disconnectedOverlay: View
    private lateinit var seekbar: SeekBar
    /* end views */

    override fun onCreate(savedInstanceState: Bundle?) {
        component.inject(this)

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
        metadataView.onPause()
        unbindCheckboxEventListeners()
        handler.removeCallbacks(updateTimeRunnable)
    }

    override fun onResume() {
        super.onResume()
        playback = playbackService
        metadataView.onResume()
        bindCheckBoxEventListeners()
        rebindUi()
        scheduleUpdateTime(true)
        runUpdateCheck()
        initObservers()
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
            if (streaming) R.drawable.ic_toolbar_streaming else R.drawable.ic_toolbar_remote)

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

    override val playbackServiceEventListener: (() -> Unit)?
        get() = playbackEvents

    private fun initObservers() {
        disposables.add(dataProvider.observeConnection().subscribe(
            { states ->
                when (states.first) {
                    IDataProvider.State.Connected -> rebindUi()
                    IDataProvider.State.Disconnected -> clearUi()
                    else -> { }
                }
            }, { /* error */ }))

        disposables.add(dataProvider.observeAuthFailure().subscribe(
            {
                val tag = InvalidPasswordDialogFragment.TAG
                if (supportFragmentManager.findFragmentByTag(tag) == null) {
                    InvalidPasswordDialogFragment.newInstance().show(supportFragmentManager, tag)
                }
            }, { /* error */ }))
    }

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
        get() = prefs.getBoolean(
            Prefs.Key.STREAMING_PLAYBACK,
            Prefs.Default.STREAMING_PLAYBACK)

    private fun togglePlaybackService() {
        val streaming = isStreamingSelected

        if (streaming) {
            playback?.stop()
        }

        prefs.edit().putBoolean(Prefs.Key.STREAMING_PLAYBACK, !streaming)?.apply()

        val messageId = if (streaming)
            R.string.snackbar_remote_enabled
        else
            R.string.snackbar_streaming_enabled

        showSnackbar(mainLayout, messageId)

        reloadPlaybackService()
        playback = playbackService

        invalidateOptionsMenu()
        rebindUi()
    }

    private fun bindCheckBoxEventListeners() {
        shuffleCb.setOnCheckedChangeListener(shuffleListener)
        muteCb.setOnCheckedChangeListener(muteListener)
        repeatCb.setOnCheckedChangeListener(repeatListener)
    }

    /* onRestoreInstanceState() calls setChecked(), which has the side effect of
    running these callbacks. this screws up state, especially for the repeat checkbox */
    private fun unbindCheckboxEventListeners() {
        shuffleCb.setOnCheckedChangeListener(null)
        muteCb.setOnCheckedChangeListener(null)
        repeatCb.setOnCheckedChangeListener(null)
    }

    private fun bindEventListeners() {
        mainLayout = findViewById(R.id.activity_main)
        metadataView = findViewById(R.id.main_metadata_view)
        shuffleCb = findViewById(R.id.check_shuffle)
        muteCb = findViewById(R.id.check_mute)
        repeatCb = findViewById(R.id.check_repeat)
        connectedNotPlayingContainer = findViewById(R.id.connected_not_playing)
        disconnectedButton = findViewById(R.id.disconnected_button)
        disconnectedContainer = findViewById(R.id.disconnected_container)
        disconnectedOverlay = findViewById(R.id.disconnected_overlay)
        showOfflineButton = findViewById(R.id.offline_tracks_button)
        playPause = findViewById(R.id.button_play_pause)
        currentTime = findViewById(R.id.current_time)
        totalTime = findViewById(R.id.total_time)
        seekbar = findViewById(R.id.seekbar)

        findViewById<View>(R.id.button_prev).setOnClickListener { _: View -> playback?.prev() }

        findViewById<View>(R.id.button_play_pause).setOnClickListener { _: View ->
            if (playback?.playbackState === PlaybackState.Stopped) {
                playback?.playAll()
            }
            else {
                playback?.pauseOrResume()
            }
        }

        findViewById<View>(R.id.button_next).setOnClickListener { _: View -> playback?.next() }

        disconnectedButton.setOnClickListener { _ -> getWebSocketService().reconnect() }

        seekbar.setOnSeekBarChangeListener(object : SeekBar.OnSeekBarChangeListener {
            override fun onProgressChanged(seekBar: SeekBar, progress: Int, fromUser: Boolean) {
                if (fromUser) {
                    seekbarValue = progress
                    currentTime.text = Duration.format(seekbarValue.toDouble())
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

        findViewById<View>(R.id.button_artists).setOnClickListener { _: View ->
            startActivity(CategoryBrowseActivity
                .getStartIntent(this, Messages.Category.ALBUM_ARTIST))
        }

        findViewById<View>(R.id.button_tracks).setOnClickListener { _: View ->
            startActivity(TrackListActivity.getStartIntent(this@MainActivity))
        }

        findViewById<View>(R.id.button_albums).setOnClickListener { _: View ->
            startActivity(AlbumBrowseActivity.getStartIntent(this@MainActivity))
        }

        findViewById<View>(R.id.button_play_queue).setOnClickListener { _ -> navigateToPlayQueue() }

        findViewById<View>(R.id.metadata_container).setOnClickListener { _ ->
            if (playback?.queueCount ?: 0 > 0) {
                navigateToPlayQueue()
            }
        }

        disconnectedOverlay.setOnClickListener { _ ->
            /* swallow input so user can't click on things while disconnected */
        }

        showOfflineButton.setOnClickListener { _ -> onOfflineTracksSelected() }
    }

    private fun rebindUi() {
        if (playback == null) {
            throw IllegalStateException()
        }

        val playbackState = playback?.playbackState
        val streaming = prefs.getBoolean(Prefs.Key.STREAMING_PLAYBACK, Prefs.Default.STREAMING_PLAYBACK)
        val connected = getWebSocketService().state === WebSocketService.State.Connected
        val stopped = playbackState === PlaybackState.Stopped
        val playing = playbackState === PlaybackState.Playing
        val buffering = playbackState === PlaybackState.Buffering
        val showMetadataView = !stopped && (playback?.queueCount ?: 0) > 0

        /* bottom section: transport controls */
        playPause.setText(if (playing || buffering) R.string.button_pause else R.string.button_play)

        connectedNotPlayingContainer.visibility = if (connected && stopped) View.VISIBLE else View.GONE
        disconnectedOverlay.visibility = if (connected || !stopped) View.GONE else View.VISIBLE

        val repeatMode = playback?.repeatMode
        val repeatChecked = repeatMode !== RepeatMode.None

        repeatCb.text = getString(REPEAT_TO_STRING_ID[repeatMode] ?: R.string.unknown_value)
        repeatCb.setCheckWithoutEvent(repeatChecked, this.repeatListener)
        shuffleCb.text = getString(if (streaming) R.string.button_random else R.string.button_shuffle)
        shuffleCb.setCheckWithoutEvent(playback?.isShuffled ?: false, shuffleListener)
        muteCb.setCheckWithoutEvent(playback?.isMuted ?: false, muteListener)

        /* middle section: connected, disconnected, and metadata views */
        connectedNotPlayingContainer.visibility = View.GONE
        disconnectedContainer.visibility = View.GONE

        if (!showMetadataView) {
            metadataView.hide()

            if (!connected) {
                disconnectedContainer.visibility = View.VISIBLE
            }
            else if (stopped) {
                connectedNotPlayingContainer.visibility = View.VISIBLE
            }
        }
        else {
            metadataView.refresh()
        }
    }

    private fun clearUi() {
        metadataView.clear()
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

            currentTime.text = Duration.format(current)
            totalTime.text = Duration.format(duration)
            seekbar.max = duration.toInt()
            seekbar.progress = current.toInt()
            seekbar.secondaryProgress = playback?.bufferedTime?.toInt() ?: 0

            var currentTimeColor = R.color.theme_foreground
            if (playback?.playbackState === PlaybackState.Paused) {
                currentTimeColor =
                    if (++blink % 2 == 0) R.color.theme_foreground
                    else R.color.theme_blink_foreground
            }

            currentTime.setTextColor(getColorCompat(currentTimeColor))

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
        repeatCb.text = getString(REPEAT_TO_STRING_ID[newMode] ?: R.string.unknown_value)
        repeatCb.setCheckWithoutEvent(checked, repeatListener)

        playback?.toggleRepeatMode()
    }

    private fun runUpdateCheck() {
        if (!UpdateAvailableDialog.displayed) {
            updateCheck.run { required, version, url ->
                if (!isPaused() && required) {
                    val suppressed = prefs.getString(Prefs.Key.UPDATE_DIALOG_SUPPRESSED_VERSION, "")
                    if (!UpdateAvailableDialog.displayed && suppressed != version) {
                        val tag = UpdateAvailableDialog.TAG
                        if (supportFragmentManager.findFragmentByTag(tag) == null) {
                            UpdateAvailableDialog.newInstance(version, url).show(supportFragmentManager, tag)
                            UpdateAvailableDialog.displayed = true
                        }
                    }
                }
            }
        }
    }

    private val repeatListener = { _: CompoundButton, _: Boolean ->
        onRepeatListener()
    }

    private val playbackEvents = { rebindUi() }

    class UpdateAvailableDialog: DialogFragment() {
        override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
            val inflater = LayoutInflater.from(activity)
            val view = inflater.inflate(R.layout.dialog_checkbox, null)
            val checkbox = view.findViewById<CheckBox>(R.id.checkbox)
            checkbox.setText(R.string.update_check_dont_ask_again)

            val version = arguments?.getString(EXTRA_VERSION)
            val url = arguments?.getString(EXTRA_URL)

            val silence: () -> Unit = {
                val prefs = activity.getSharedPreferences(Prefs.NAME, Context.MODE_PRIVATE)
                prefs.edit().putString(Prefs.Key.UPDATE_DIALOG_SUPPRESSED_VERSION, version).apply()
            }

            val dlg = AlertDialog.Builder(activity)
                .setTitle(R.string.update_check_dialog_title)
                .setMessage(getString(R.string.update_check_dialog_message, version))
                .setNegativeButton(R.string.button_no, { _, _ ->
                    if (checkbox.isChecked) {
                        silence()
                    }
                })
                .setPositiveButton(R.string.button_yes) { _, _ ->
                    if (checkbox.isChecked) {
                        silence()
                    }

                    try {
                        startActivity(Intent(Intent.ACTION_VIEW, Uri.parse(url)))
                    }
                    catch (ex: Exception) {
                    }
                }
                .setCancelable(false)
                .create()

            dlg.setView(view)
            dlg.setCancelable(false)
            dlg.setCanceledOnTouchOutside(false)

            return dlg
        }

        companion object {
            val TAG = "update_available_dialog"
            val EXTRA_VERSION = "extra_version"
            val EXTRA_URL = "extra_url"

            var displayed: Boolean = false

            fun newInstance(version: String, url: String): UpdateAvailableDialog {
                val args = Bundle()
                args.putString(EXTRA_VERSION, version)
                args.putString(EXTRA_URL, url)
                val dialog = UpdateAvailableDialog()
                dialog.arguments = args
                return dialog
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
