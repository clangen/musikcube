package io.casey.musikcube.remote.ui.home.activity

import android.app.Dialog
import android.content.Context
import android.content.Intent
import android.graphics.Color
import android.net.Uri
import android.os.Bundle
import android.os.Handler
import android.view.*
import android.widget.*
import androidx.appcompat.app.AlertDialog
import androidx.core.content.ContextCompat
import androidx.fragment.app.DialogFragment
import com.wooplr.spotlight.SpotlightView
import io.casey.musikcube.remote.R
import io.casey.musikcube.remote.service.playback.Playback
import io.casey.musikcube.remote.service.playback.PlaybackState
import io.casey.musikcube.remote.service.playback.RepeatMode
import io.casey.musikcube.remote.service.playback.impl.remote.Metadata
import io.casey.musikcube.remote.service.websocket.WebSocketService
import io.casey.musikcube.remote.service.websocket.model.IMetadataProxy
import io.casey.musikcube.remote.ui.home.fragment.InvalidPasswordDialogFragment
import io.casey.musikcube.remote.ui.home.view.MainMetadataView
import io.casey.musikcube.remote.ui.navigation.Navigate
import io.casey.musikcube.remote.ui.settings.activity.RemoteSettingsActivity
import io.casey.musikcube.remote.ui.settings.activity.SettingsActivity
import io.casey.musikcube.remote.ui.settings.constants.Prefs
import io.casey.musikcube.remote.ui.shared.activity.BaseActivity
import io.casey.musikcube.remote.ui.shared.extension.getColorCompat
import io.casey.musikcube.remote.ui.shared.extension.setCheckWithoutEvent
import io.casey.musikcube.remote.ui.shared.extension.showSnackbar
import io.casey.musikcube.remote.ui.shared.extension.toolbar
import io.casey.musikcube.remote.ui.shared.mixin.MetadataProxyMixin
import io.casey.musikcube.remote.ui.shared.mixin.PlaybackMixin
import io.casey.musikcube.remote.ui.shared.util.Duration
import io.casey.musikcube.remote.ui.shared.util.UpdateCheck

class MainActivity : BaseActivity() {
    private val handler = Handler()
    private var updateCheck: UpdateCheck = UpdateCheck()
    private var seekbarValue = -1
    private var blink = 0

    private lateinit var data: MetadataProxyMixin
    private lateinit var playback: PlaybackMixin

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
        data = mixin(MetadataProxyMixin())
        playback = mixin(PlaybackMixin { rebindUi() })

        super.onCreate(savedInstanceState)

        prefs = this.getSharedPreferences(Prefs.NAME, Context.MODE_PRIVATE)

        setContentView(R.layout.main_activity)

        bindEventListeners()

        if (!data.wss.hasValidConnection()) {
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
        metadataView.onResume()
        bindCheckBoxEventListeners()
        rebindUi()
        scheduleUpdateTime(true)
        runUpdateCheck()
        initObservers()
        registerLayoutListener()
    }

    override fun onStart() {
        super.onStart()
        rebindUi()
    }

    override fun onCreateOptionsMenu(menu: Menu): Boolean {
        menuInflater.inflate(R.menu.main_menu, menu)
        return true
    }

    override fun onPrepareOptionsMenu(menu: Menu): Boolean {
        val streaming = isStreamingSelected

        val remoteToggle = menu.findItem(R.id.action_remote_toggle)

        val drawable = getDrawable(
            if (streaming) R.drawable.ic_toolbar_streaming
            else R.drawable.ic_toolbar_remote)


        drawable?.setTint(ContextCompat.getColor(this, R.color.theme_foreground))

        remoteToggle.actionView?.findViewById<ImageView>(R.id.icon)?.setImageDrawable(drawable)

        remoteToggle.actionView?.setOnClickListener {
            togglePlaybackService()
        }

        remoteToggle.actionView?.setOnLongClickListener {
            showPlaybackTogglePopup()
            true
        }

        return super.onPrepareOptionsMenu(menu)
    }

    private fun showPlaybackTogglePopup() {
        val toolbarButton = findViewById<View>(R.id.action_remote_toggle)
        val popup = PopupMenu(this, toolbarButton)

        val menuId =
            if (isStreamingSelected) R.menu.transfer_to_server_menu
            else R.menu.transfer_from_server_menu

        popup.inflate(menuId)

        popup.setOnMenuItemClickListener {
            when(it.itemId) {
                R.id.menu_switch_seamless -> togglePlaybackService(Playback.SwitchMode.Transfer)
                R.id.menu_switch_copy -> togglePlaybackService(Playback.SwitchMode.Copy)
                else -> { }
            }
            true
        }

        popup.show()
    }

    override fun onOptionsItemSelected(item: MenuItem): Boolean {
        if (item.itemId == R.id.action_remote_toggle) {
            if (playback.service.state != PlaybackState.Stopped) {
                showPlaybackTogglePopup()
            }
            else {
                togglePlaybackService()
            }
            return true
        }

        return super.onOptionsItemSelected(item)
    }

    private fun initObservers() {
        disposables.add(data.provider.observeState().subscribe(
            { states ->
                when (states.first) {
                    IMetadataProxy.State.Connected -> {
                        rebindUi()
                        checkShowSpotlight()
                        checkShowApiMismatch()
                    }
                    IMetadataProxy.State.Disconnected -> {
                        clearUi()
                    }
                    else -> {
                    }
                }
            }, { /* error */ }))

        disposables.add(data.provider.observeAuthFailure().subscribe(
            {
                val tag = InvalidPasswordDialogFragment.TAG
                if (supportFragmentManager.findFragmentByTag(tag) == null) {
                    InvalidPasswordDialogFragment.newInstance().show(supportFragmentManager, tag)
                }
            }, { /* error */ }))
    }

    private fun onOfflineTracksSelected() {
        if (isStreamingSelected) {
            Navigate.toBrowse(this, Metadata.Category.OFFLINE)
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

    private fun togglePlaybackService(mode: Playback.SwitchMode = Playback.SwitchMode.Swap) {
        val isStreaming = isStreamingSelected
        prefs.edit().putBoolean(Prefs.Key.STREAMING_PLAYBACK, !isStreaming)?.apply()

        val messageId = if (isStreaming)
            R.string.snackbar_remote_enabled
        else
            R.string.snackbar_streaming_enabled

        showSnackbar(mainLayout, messageId)

        Playback.transferPlayback(this, playback, mode)

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

        findViewById<View>(R.id.button_prev).setOnClickListener {
            playback.service.prev()
        }

        findViewById<View>(R.id.button_play_pause).setOnClickListener {
            when (playback.service.state === PlaybackState.Stopped) {
                true -> playback.service.playAll()
                else -> playback.service.pauseOrResume()
            }
        }

        findViewById<View>(R.id.button_next).setOnClickListener {
            playback.service.next()
        }

        disconnectedButton.setOnClickListener {
            data.wss.reconnect()
        }

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
                    playback.service.seekTo(seekbarValue.toDouble())
                    seekbarValue = -1
                }
            }
        })

        findViewById<View>(R.id.button_albums).setOnClickListener {
            Navigate.toBrowse(this, Metadata.Category.ALBUM)
        }

        findViewById<View>(R.id.button_artists).setOnClickListener {
            Navigate.toBrowse(this, Metadata.Category.ALBUM_ARTIST)
        }

        findViewById<View>(R.id.button_tracks).setOnClickListener {
            Navigate.toBrowse(this, Metadata.Category.TRACKS)
        }

        findViewById<View>(R.id.button_playlists).setOnClickListener {
            Navigate.toBrowse(this, Metadata.Category.PLAYLISTS)
        }

        findViewById<View>(R.id.button_play_queue).setOnClickListener {
            navigateToPlayQueue()
        }

        findViewById<View>(R.id.metadata_container).setOnClickListener {
            when (playback.service.queueCount > 0) {
                true -> navigateToPlayQueue()
                else -> Navigate.toBrowse(this)
            }
        }

        findViewById<View>(R.id.middle_container).setOnClickListener {
            if (data.wss.state == WebSocketService.State.Connected) {
                Navigate.toBrowse(this)
            }
        }

        disconnectedOverlay.setOnClickListener {
            /* swallow input so user can't click on things while disconnected */
        }

        showOfflineButton.setOnClickListener {
            onOfflineTracksSelected()
        }

        toolbar?.let { tb ->
            tb.navigationIcon = getDrawable(R.drawable.ic_menu)
            tb.setNavigationOnClickListener {
                val popup = PopupMenu(this, tb)
                popup.inflate(R.menu.left_menu)

                val connected = data.wss.state === WebSocketService.State.Connected
                popup.menu.findItem(R.id.action_categories).isEnabled = connected
                popup.menu.findItem(R.id.action_remote_manage).isEnabled = connected

                popup.setOnMenuItemClickListener {
                    when(it.itemId) {
                        R.id.action_settings -> {
                            startActivity(SettingsActivity.getStartIntent(this))
                        }
                        R.id.action_categories -> {
                            Navigate.toAllCategories(this)
                        }
                        R.id.action_offline_tracks -> {
                            onOfflineTracksSelected()
                        }
                        R.id.action_remote_manage -> {
                            startActivity(RemoteSettingsActivity.getStartIntent(this))
                        }
                    }
                    true
                }
                popup.show()
            }
        }
    }

    private fun rebindUi() {
        val playbackState = playback.service.state
        val streaming = prefs.getBoolean(Prefs.Key.STREAMING_PLAYBACK, Prefs.Default.STREAMING_PLAYBACK)
        val connected = data.wss.state === WebSocketService.State.Connected
        val stopped = playbackState === PlaybackState.Stopped
        val playing = playbackState === PlaybackState.Playing
        val buffering = playbackState === PlaybackState.Buffering
        val showMetadataView = !stopped && (playback.service.queueCount) > 0

        /* bottom section: transport controls */
        playPause.setText(if (playing || buffering) R.string.button_pause else R.string.button_play)

        connectedNotPlayingContainer.visibility = if (connected && stopped) View.VISIBLE else View.GONE
        disconnectedOverlay.visibility = if (connected || !stopped) View.GONE else View.VISIBLE

        val repeatMode = playback.service.repeatMode
        val repeatChecked = repeatMode !== RepeatMode.None

        repeatCb.text = getString(REPEAT_TO_STRING_ID[repeatMode] ?: R.string.unknown_value)
        repeatCb.setCheckWithoutEvent(repeatChecked, this.repeatListener)
        shuffleCb.text = getString(if (streaming) R.string.button_random else R.string.button_shuffle)
        shuffleCb.setCheckWithoutEvent(playback.service.shuffled, shuffleListener)
        muteCb.setCheckWithoutEvent(playback.service.muted, muteListener)

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

    private fun registerLayoutListener() {
        window.decorView.viewTreeObserver.addOnGlobalLayoutListener(
            object : ViewTreeObserver.OnGlobalLayoutListener {
                override fun onGlobalLayout() {
                    val toolbarButton = findViewById<View>(R.id.action_remote_toggle)
                    if (toolbarButton != null && data.provider.state == IMetadataProxy.State.Connected) {
                        checkShowSpotlight()
                        window.decorView.viewTreeObserver.removeOnGlobalLayoutListener(this)
                    }
                }
            })
    }

    private fun checkShowSpotlight() {
        /* sometimes the spotlight animation doesn't play properly; let's try to delay it
        for a bit to make it more reliable */
        handler.postDelayed({
            val toolbarButton = findViewById<View>(R.id.action_remote_toggle)
            if (!paused && !spotlightDisplayed && toolbarButton != null) {
                SpotlightView.Builder(this@MainActivity)
                    .introAnimationDuration(400)
                    .enableRevealAnimation(true)
                    .performClick(true)
                    .fadeinTextDuration(400)
                    .headingTvColor(getColorCompat(R.color.color_accent))
                    .headingTvSize(24)
                    .headingTvText(getString(R.string.spotlight_playback_mode_title))
                    .subHeadingTvColor(Color.parseColor("#ffffff"))
                    .subHeadingTvSize(14)
                    .subHeadingTvText(getString(R.string.spotlight_playback_mode_message))
                    .maskColor(Color.parseColor("#dc000000"))
                    .target(toolbarButton)
                    .lineAnimDuration(400)
                    .lineAndArcColor(getColorCompat(R.color.color_primary))
                    .dismissOnTouch(true)
                    .dismissOnBackPress(true)
                    .enableDismissAfterShown(true)
                    .usageId(SPOTLIGHT_STREAMING_ID)
                    .show()

                spotlightDisplayed = true
            }
        }, 1000)
    }

    private fun checkShowApiMismatch() {
        if (!apiMismatchDisplayed && data.wss.shouldUpgrade()) {
            val tag = ApiMismatchDialog.TAG
            if (supportFragmentManager.findFragmentByTag(tag) == null) {
                ApiMismatchDialog.newInstance().show(supportFragmentManager, tag)
            }
            apiMismatchDisplayed = true
        }
    }

    private fun clearUi() {
        metadataView.clear()
        rebindUi()
    }

    private fun navigateToPlayQueue() {
        Navigate.toPlayQueue(playback.service.queuePosition, this)
    }

    private fun scheduleUpdateTime(immediate: Boolean) {
        handler.removeCallbacks(updateTimeRunnable)
        handler.postDelayed(updateTimeRunnable, (if (immediate) 0 else 1000).toLong())
    }

    private val updateTimeRunnable = Runnable {
        val duration = playback.service.duration
        val current: Double = if (seekbarValue == -1) playback.service.currentTime else seekbarValue.toDouble()

        currentTime.text = Duration.format(current)
        totalTime.text = Duration.format(duration)
        seekbar.max = duration.toInt()
        seekbar.progress = current.toInt()
        seekbar.secondaryProgress = playback.service.bufferedTime.toInt()

        var currentTimeColor = R.color.theme_foreground
        if (playback.service.state === PlaybackState.Paused) {
            currentTimeColor =
                if (++blink % 2 == 0) R.color.theme_foreground
                else R.color.theme_blink_foreground
        }

        currentTime.setTextColor(getColorCompat(currentTimeColor))

        scheduleUpdateTime(false)
    }

    private val muteListener = { _: CompoundButton, b: Boolean ->
        if (b != playback.service.muted) {
            playback.service.toggleMute()
        }
    }

    private val shuffleListener = { _: CompoundButton, b: Boolean ->
        if (b != playback.service.shuffled) {
            playback.service.toggleShuffle()
        }
    }

    private fun onRepeatListener() {
        val currentMode = playback.service.repeatMode

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

        playback.service.toggleRepeatMode()
    }

    private fun runUpdateCheck() {
        if (!UpdateAvailableDialog.displayed) {
            updateCheck.run { required, version, url ->
                if (!paused && required) {
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

    class UpdateAvailableDialog: DialogFragment() {
        override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
            val activity = this.activity!!
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
                .setNegativeButton(R.string.button_no) { _, _ ->
                    if (checkbox.isChecked) {
                        silence()
                    }
                }
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
            const val TAG = "update_available_dialog"
            const val EXTRA_VERSION = "extra_version"
            const val EXTRA_URL = "extra_url"

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

    class SwitchToOfflineTracksDialog: DialogFragment() {
        override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
            val dlg = AlertDialog.Builder(activity!!)
                .setTitle(R.string.main_switch_to_streaming_title)
                .setMessage(R.string.main_switch_to_streaming_message)
                .setNegativeButton(R.string.button_no, null)
                .setPositiveButton(R.string.button_yes) { _, _ -> (activity as MainActivity).onConfirmSwitchToOfflineTracks() }
                .create()

            dlg.setCancelable(false)
            return dlg
        }

        companion object {
            const val TAG = "switch_to_offline_tracks_dialog"
            fun newInstance(): SwitchToOfflineTracksDialog = SwitchToOfflineTracksDialog()
        }
    }

    class ApiMismatchDialog: DialogFragment() {
        override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
            val dlg = AlertDialog.Builder(activity!!)
                .setTitle(R.string.main_api_mismatch_title)
                .setMessage(R.string.main_api_mismatch_message)
                .setNegativeButton(R.string.button_close, null)
                .setPositiveButton(R.string.main_api_mismatch_releases_page) { _, _ ->
                    try {
                        startActivity(Intent(Intent.ACTION_VIEW, Uri.parse(DEFAULT_UPGRADE_URL)))
                    }
                    catch (ex: Exception) {
                    }
                }
                .create()

            dlg.setCancelable(false)
            return dlg
        }

        companion object {
            const val TAG = "api_mismatch_dialog"
            fun newInstance(): ApiMismatchDialog = ApiMismatchDialog()
        }
    }

    companion object {
        private const val SPOTLIGHT_STREAMING_ID = "streaming_mode"
        private const val DEFAULT_UPGRADE_URL = "https://github.com/clangen/musikcube/releases/"
        private var spotlightDisplayed = false
        private var apiMismatchDisplayed = false

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
