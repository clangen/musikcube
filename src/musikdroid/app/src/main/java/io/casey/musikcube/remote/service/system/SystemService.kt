package io.casey.musikcube.remote.service.system

import android.app.*
import android.content.*
import android.graphics.Bitmap
import android.graphics.drawable.Drawable
import android.media.AudioManager
import android.os.Build
import android.os.IBinder
import android.os.PowerManager
import android.support.v4.media.MediaMetadataCompat
import android.support.v4.media.session.MediaSessionCompat
import android.support.v4.media.session.PlaybackStateCompat
import android.util.Log
import android.view.KeyEvent
import androidx.core.app.NotificationCompat
import androidx.core.content.ContextCompat
import com.bumptech.glide.load.engine.DiskCacheStrategy
import com.bumptech.glide.request.FutureTarget
import com.bumptech.glide.request.RequestFutureTarget
import com.bumptech.glide.request.RequestOptions
import com.bumptech.glide.request.target.Target
import com.bumptech.glide.request.transition.Transition
import io.casey.musikcube.remote.Application
import io.casey.musikcube.remote.R
import io.casey.musikcube.remote.injection.GlideApp
import io.casey.musikcube.remote.injection.GlideRequest
import io.casey.musikcube.remote.service.playback.Playback
import io.casey.musikcube.remote.service.playback.PlaybackServiceFactory
import io.casey.musikcube.remote.service.playback.PlaybackState
import io.casey.musikcube.remote.service.playback.impl.streaming.StreamingPlaybackService
import io.casey.musikcube.remote.service.websocket.model.ITrack
import io.casey.musikcube.remote.ui.home.activity.MainActivity
import io.casey.musikcube.remote.ui.settings.constants.Prefs
import io.casey.musikcube.remote.ui.shared.extension.fallback
import io.casey.musikcube.remote.ui.shared.util.Size
import io.casey.musikcube.remote.util.Debouncer
import androidx.core.app.NotificationCompat.Action as NotifAction
import io.casey.musikcube.remote.ui.shared.util.AlbumArtLookup.getUrl as getAlbumArtUrl

/**
 * a service used to interact with all of the system media-related components -- notifications,
 * lock screen controls, and headset events. also holds a partial wakelock to keep the system
 * from completely falling asleep during streaming playback.
 */
class SystemService : Service() {
    enum class State { Dead, Active, Sleeping }

    private var playback: StreamingPlaybackService? = null
    private var wakeLock: PowerManager.WakeLock? = null
    private var mediaSession: MediaSessionCompat? = null
    private var headsetHookPressCount = 0

    /* if we pause via headset on some devices, and unpause immediately after,
    the runtime will erroneously issue a second KEYCODE_MEDIA_PAUSE command,
    instead of KEYCODE_MEDIA_RESUME. to work around this, if we pause from a
    headset, we flip this bit for a couple seconds, which will be used as a
    hint that we should resume if we get another KEYCODE_MEDIA_PAUSE. */
    private var headsetDoublePauseHack = false

    private lateinit var powerManager: PowerManager
    private lateinit var prefs: SharedPreferences

    private val albumArt = AlbumArt()
    private val sessionData = SessionMetadata()

    override fun onCreate() {
        Log.d(TAG, "onCreate")

        state = State.Sleeping

        super.onCreate()

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            val channel = NotificationChannel(
                NOTIFICATION_CHANNEL,
                NOTIFICATION_CHANNEL,
                NotificationManager.IMPORTANCE_LOW)

            channel.enableVibration(false)
            channel.setSound(null, null)
            channel.lockscreenVisibility = Notification.VISIBILITY_PUBLIC

            notificationManager.deleteNotificationChannel(NOTIFICATION_CHANNEL)
            notificationManager.createNotificationChannel(channel)
        }

        prefs = getSharedPreferences(Prefs.NAME, Context.MODE_PRIVATE)
        powerManager = getSystemService(Context.POWER_SERVICE) as PowerManager
        registerReceivers()

        wakeupNow()
    }

    override fun onDestroy() {
        Log.d(TAG, "onDestroy")

        state = State.Dead
        super.onDestroy()
        releaseWakeLock()
        unregisterReceivers()
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            notificationManager.deleteNotificationChannel(NOTIFICATION_CHANNEL)
        }
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        Log.d(TAG, "onStartCommand")

        if (intent != null && intent.action != null) {
            when (intent.action) {
                ACTION_WAKE_UP -> wakeupNow()
                ACTION_SHUT_DOWN -> shutdownNow()
                ACTION_SLEEP -> sleepNow()
                else -> {
                    if (handlePlaybackAction(intent.action)) {
                        wakeupNow()
                    }
                }
            }
        }

        return super.onStartCommand(intent, flags, startId)
    }

    override fun onBind(intent: Intent): IBinder? = null

    private fun wakeupNow() {
        Log.d(TAG, "SystemService WAKE_UP")

        val sleeping = playback == null || wakeLock == null

        if (playback == null) {
            playback = PlaybackServiceFactory.streaming(this)
        }

        acquireWakeLock()

        if (sleeping) {
            playback?.connect(playbackListener)
        }

        checkInitMediaSession()
        updateMediaSessionPlaybackState()

        state = State.Active
    }

    private fun shutdownNow() {
        Log.d(TAG, "SystemService SHUT_DOWN")

        deinitMediaSession()

        playback?.disconnect(playbackListener)
        playback = null

        releaseWakeLock()

        stopSelf()
    }

    private fun sleepNow() {
        Log.d(TAG, "SystemService SLEEP")
        releaseWakeLock()
        playback?.disconnect(playbackListener)
        state = State.Sleeping
    }

    private fun acquireWakeLock() {
        if (wakeLock == null) {
            wakeLock = powerManager.newWakeLock(
                PowerManager.PARTIAL_WAKE_LOCK, "$SESSION_TAG:")

            wakeLock?.let {
                it.setReferenceCounted(false)
                it.acquire()
                wakeLockAcquireTime = System.nanoTime()
            }
        }
    }

    private fun releaseWakeLock() {
        if (wakeLock != null) {
            wakeLock?.release()
            wakeLock = null
            totalWakeLockTime += (System.nanoTime() - wakeLockAcquireTime)
            wakeLockAcquireTime = -1L
        }
    }

    private fun checkInitMediaSession() {
        if (mediaSession == null || mediaSession?.isActive != true) {
            deinitMediaSession()

            val receiver = ComponentName(
                packageName, MediaButtonReceiver::class.java.name)

            mediaSession = MediaSessionCompat(
                this, SESSION_TAG, receiver, null)
                .apply {
                    this.setCallback(mediaSessionCallback)
                    this.isActive = true
                }
        }
    }

    private fun deinitMediaSession() {
        mediaSession?.release()
        mediaSession = null
    }

    private fun registerReceivers() {
        val filter = IntentFilter(AudioManager.ACTION_AUDIO_BECOMING_NOISY)
        registerReceiver(headsetUnpluggedReceiver, filter)
    }

    private fun unregisterReceivers() =
        try {
            unregisterReceiver(headsetUnpluggedReceiver)
        }
        catch (ex: Exception) {
            Log.e(TAG, "unable to unregister headset (un)plugged BroadcastReceiver")
        }

    private fun updateMediaSessionPlaybackState() {
        mediaSession?.let { session ->
            var mediaSessionState = PlaybackStateCompat.STATE_STOPPED
            var duration = 0
            var playing: ITrack? = null

            playback?.let { pb ->
                mediaSessionState = when (pb.state) {
                    PlaybackState.Playing -> PlaybackStateCompat.STATE_PLAYING
                    PlaybackState.Buffering -> PlaybackStateCompat.STATE_BUFFERING
                    /* HACK: if we return STATE_PAUSED, the playback controls will disappear
                    from the lock screen! wtf? we don't want that, so we return
                    STATE_BUFFERING instead, which seems to keep the play controls visible,
                    and also display the "PLAY" button like the user expects. sigh */
                    PlaybackState.Paused -> PlaybackStateCompat.STATE_BUFFERING
                    PlaybackState.Stopped -> PlaybackStateCompat.STATE_STOPPED
                }
                playing = pb.playingTrack
                duration = (pb.duration * 1000).toInt()
            }

            updateMediaSession(playing, duration)
            updateNotification(playing, playback?.state ?: PlaybackState.Stopped)

            session.setPlaybackState(PlaybackStateCompat.Builder()
                .setState(mediaSessionState, 0, 0f)
                .setActions(MEDIA_SESSION_ACTIONS)
                .build())
        }
    }

    private fun downloadAlbumArtIfNecessary(track: ITrack, duration: Int) {
        if (!albumArt.same(track) || (albumArt.request == null && albumArt.bitmap == null)) {
            if (track.artist.isNotBlank() && track.album.isNotBlank()) {
                Log.d(TAG, "downloading album art")

                val url = getAlbumArtUrl(track, Size.Mega)

                val request = GlideApp.with(applicationContext)
                    .asBitmap()
                    .load(url)
                    .apply(BITMAP_OPTIONS)

                albumArt.reset(track)
                albumArt.request = request
                albumArt.target = request.into(
                    object: RequestFutureTarget<Bitmap>(Target.SIZE_ORIGINAL, Target.SIZE_ORIGINAL) {
                        override fun onResourceReady(bitmap: Bitmap, transition: Transition<in Bitmap>?) {
                            /* make sure the instance's current request is the same as this request. it's
                            possible we had another download request come in before this one finished */
                            if (albumArt.request == request) {
                                albumArt.bitmap = bitmap
                                albumArt.request = null
                                updateMediaSession(track, duration)
                            }
                        }

                        override fun onLoadFailed(errorDrawable: Drawable?) {
                            if (albumArt.request == request) {
                                albumArt.request = null
                            }
                        }
                    })
            }
        }
        else {
            Log.d(TAG, "downloadAlbumArt already in flight")
        }
    }

    private fun updateMediaSession(track: ITrack?, duration: Int) {
        var currentImage: Bitmap? = null

        if (track != null) {
            downloadAlbumArtIfNecessary(track, duration)
            currentImage = albumArt.bitmap
        }

        if (!sessionData.matches(track, currentImage)) {
            sessionData.update(track, currentImage, duration)
            mediaSessionDebouncer.call()
        }
    }

    /* some versions of the OS seem to have problems if we update the lockscreen
    artwork more quickly than twice a second. this can happen if we just loaded
    a new track but don't have artwork yet, but artwork comes back right away.
    this debouncer ensures we wait at least half a second between updates */
    private val mediaSessionDebouncer = object: Debouncer<Unit>(500) {
        override fun onDebounced(last: Unit?) {
            val track = sessionData.track
            mediaSession?.setMetadata(MediaMetadataCompat.Builder()
                .putString(MediaMetadataCompat.METADATA_KEY_ARTIST, track?.artist ?: "-")
                .putString(MediaMetadataCompat.METADATA_KEY_ALBUM, track?.album ?: "-")
                .putString(MediaMetadataCompat.METADATA_KEY_TITLE, track?.title ?: "-")
                .putLong(MediaMetadataCompat.METADATA_KEY_DURATION, sessionData.duration.toLong())
                .putBitmap(MediaMetadataCompat.METADATA_KEY_ALBUM_ART, sessionData.bitmap)
                .build())
        }
    }

    private fun updateNotification(track: ITrack?, state: PlaybackState) {
        val contentIntent = PendingIntent.getActivity(
            applicationContext, 1, MainActivity.getStartIntent(this), 0)

        val title = fallback(track?.title, "-")
        val artist = fallback(track?.artist, "-")
        val album = fallback(track?.album, "-")

        val notification = NotificationCompat.Builder(this, NOTIFICATION_CHANNEL)
            .setSmallIcon(R.drawable.ic_notification)
            .setContentTitle(title)
            .setContentText("$artist - $album")
            .setContentIntent(contentIntent)
            .setUsesChronometer(false)
            .setVisibility(NotificationCompat.VISIBILITY_PUBLIC)
            .setOngoing(true)

        if (state == PlaybackState.Stopped) {
            notification.addAction(action(
                android.R.drawable.ic_media_play,
                getString(R.string.button_play), ACTION_NOTIFICATION_PLAY))

            notification.setStyle(androidx.media.app.NotificationCompat.MediaStyle()
                .setShowActionsInCompactView(0)
                .setMediaSession(mediaSession?.sessionToken))
        }
        else {
            if (state == PlaybackState.Playing) {
                notification.addAction(action(
                    android.R.drawable.ic_media_previous,
                    getString(R.string.button_prev), ACTION_NOTIFICATION_PREV))

                notification.addAction(action(
                    android.R.drawable.ic_media_pause,
                    getString(R.string.button_pause), ACTION_NOTIFICATION_PAUSE))

                notification.addAction(action(
                    android.R.drawable.ic_media_next,
                    getString(R.string.button_next), ACTION_NOTIFICATION_NEXT))

                notification.setStyle(androidx.media.app.NotificationCompat.MediaStyle()
                    .setShowActionsInCompactView(0, 1, 2)
                    .setMediaSession(mediaSession?.sessionToken))
            }
            else {
                notification.addAction(action(
                    android.R.drawable.ic_media_play,
                    getString(R.string.button_play), ACTION_NOTIFICATION_PLAY))

                notification.addAction(action(
                    android.R.drawable.ic_menu_close_clear_cancel,
                    getString(R.string.button_close), ACTION_NOTIFICATION_STOP))

                notification.setStyle(androidx.media.app.NotificationCompat.MediaStyle()
                    .setShowActionsInCompactView(0, 1)
                    .setMediaSession(mediaSession?.sessionToken))
            }
        }

        startForeground(NOTIFICATION_ID, notification.build())
    }

    private fun action(icon: Int, title: String, intentAction: String): NotifAction {
        val intent = Intent(applicationContext, SystemService::class.java)
        intent.action = intentAction
        val pendingIntent = PendingIntent.getService(applicationContext, 1, intent, 0)
        return NotifAction.Builder(icon, title, pendingIntent).build()
    }

    private fun handlePlaybackAction(action: String?): Boolean {
        this.playback?.let {
            when (action) {
                ACTION_NOTIFICATION_NEXT -> {
                    it.next()
                    return true
                }
                ACTION_NOTIFICATION_PAUSE -> {
                    it.pause()
                    return true
                }
                ACTION_NOTIFICATION_PLAY -> {
                    it.resume()
                    return true
                }
                ACTION_NOTIFICATION_PREV -> {
                    it.prev()
                    return true
                }
                ACTION_NOTIFICATION_STOP -> {
                    it.stop()
                    shutdown()
                    return true
                }
                else -> {
                }
            }
        }

        return false
    }

    private val headsetHookDebouncer =
            object: Debouncer<Void>(HEADSET_HOOK_DEBOUNCE_MS) {
        override fun onDebounced(last: Void?) {
            playback?.let {
                when (headsetHookPressCount) {
                    1 -> it.pauseOrResume()
                    2 -> it.next()
                    3 -> it.prev()
                }
            }
            headsetHookPressCount = 0
        }
    }

    private val notificationManager: NotificationManager
        get() = getSystemService(NOTIFICATION_SERVICE) as NotificationManager

    private val headsetDoublePauseHackDebouncer =
            object: Debouncer<Void>(HEADSET_DOUBLE_PAUSE_HACK_DEBOUNCE_MS) {
        override fun onDebounced(last: Void?) {
            headsetDoublePauseHack = false
        }
    }

    private val mediaSessionCallback = object : MediaSessionCompat.Callback() {
        override fun onMediaButtonEvent(mediaButtonEvent: Intent?): Boolean {
            if (Intent.ACTION_MEDIA_BUTTON == mediaButtonEvent?.action) {
                val event = mediaButtonEvent.getParcelableExtra<KeyEvent>(Intent.EXTRA_KEY_EVENT) ?: return super.onMediaButtonEvent(mediaButtonEvent)

                val keycode = event.keyCode
                val action = event.action
                if (event.repeatCount == 0 && action == KeyEvent.ACTION_DOWN) {
                    when (keycode) {
                        KeyEvent.KEYCODE_HEADSETHOOK -> {
                            ++headsetHookPressCount
                            headsetHookDebouncer.call()
                            return true
                        }
                        KeyEvent.KEYCODE_MEDIA_STOP -> {
                            playback?.pause()
                            shutdown()
                            return true
                        }
                        KeyEvent.KEYCODE_MEDIA_PLAY_PAUSE -> {
                            playback?.pauseOrResume()
                            return true
                        }
                        KeyEvent.KEYCODE_MEDIA_NEXT -> {
                            playback?.next()
                            return true
                        }
                        KeyEvent.KEYCODE_MEDIA_PREVIOUS -> {
                            playback?.prev()
                            return true
                        }
                        KeyEvent.KEYCODE_MEDIA_PAUSE -> {
                            if (headsetDoublePauseHack) {
                                playback?.resume()
                                headsetDoublePauseHack = false
                            }
                            else {
                                playback?.pause()
                                headsetDoublePauseHack = true
                                headsetDoublePauseHackDebouncer.call()
                            }
                            return true
                        }
                        KeyEvent.KEYCODE_MEDIA_PLAY -> {
                            playback?.resume()
                            return true
                        }
                    }
                }
            }
            return false
        }

        override fun onPlay() {
            playback?.let {
                when (it.queueCount == 0) {
                    true -> it.playAll()
                    false -> it.resume()
                }
            }
        }

        override fun onPause() {
            playback?.pause()
        }

        override fun onSkipToNext() {
            playback?.next()
        }

        override fun onSkipToPrevious() {
            playback?.prev()
        }

        override fun onFastForward() {
            playback?.seekForward()
        }

        override fun onRewind() {
            playback?.seekBackward()
        }
    }

    private val playbackListener = {
        /* freaking sigh... */
        if (playback?.state == PlaybackState.Playing) {
            headsetDoublePauseHack = false
        }
        updateMediaSessionPlaybackState()
    }

    private val headsetUnpluggedReceiver = object : BroadcastReceiver() {
        override fun onReceive(context: Context, intent: Intent) {
            if (intent.action == AudioManager.ACTION_AUDIO_BECOMING_NOISY) {
                playback?.let { pb ->
                    val switchOnDisconnect = prefs.getBoolean(
                        Prefs.Key.TRANSFER_TO_SERVER_ON_HEADSET_DISCONNECT,
                        Prefs.Default.TRANSFER_TO_SERVER_ON_HEADSET_DISCONNECT)

                    val isPlaying =
                        (pb.state == PlaybackState.Playing) ||
                        (pb.state == PlaybackState.Buffering)

                    pb.pause()

                    if (switchOnDisconnect && isPlaying) {
                        Playback.transferPlayback(this@SystemService, Playback.SwitchMode.Transfer)
                    }
                }
            }
        }
    }

    private class AlbumArt {
        var target: FutureTarget<Bitmap>? = null
        var request: GlideRequest<Bitmap>? = null
        var bitmap: Bitmap? = null
        var track: ITrack? = null

        fun reset(t: ITrack? = null) {
            bitmap = null
            request = null
            target = null
            track = t
        }

        fun same(other: ITrack?): Boolean =
            track != null && other != null && other.externalId == track?.externalId
    }

    private class SessionMetadata {
        var track: ITrack? = null
        var bitmap: Bitmap? = null
        var duration: Int = 0

        fun update(otherTrack: ITrack?, otherBitmap: Bitmap?, otherDuration: Int) {
            track = otherTrack
            bitmap = otherBitmap
            duration = otherDuration
        }

        fun matches(otherTrack: ITrack?, otherBitmap: Bitmap?): Boolean {
            return (track != null && otherTrack != null) &&
                otherTrack.externalId == track?.externalId &&
                bitmap === otherBitmap
        }
    }

    companion object {
        private const val TAG = "SystemService"
        private const val SESSION_TAG = "musikdroid.SystemService"
        private const val NOTIFICATION_ID = 0xdeadbeef.toInt()
        private const val NOTIFICATION_CHANNEL = "musikdroid"
        private const val HEADSET_HOOK_DEBOUNCE_MS = 500L
        private const val HEADSET_DOUBLE_PAUSE_HACK_DEBOUNCE_MS = 3500L
        private const val ACTION_NOTIFICATION_PLAY = "io.casey.musikcube.remote.NOTIFICATION_PLAY"
        private const val ACTION_NOTIFICATION_PAUSE = "io.casey.musikcube.remote.NOTIFICATION_PAUSE"
        private const val ACTION_NOTIFICATION_NEXT = "io.casey.musikcube.remote.NOTIFICATION_NEXT"
        private const val ACTION_NOTIFICATION_PREV = "io.casey.musikcube.remote.NOTIFICATION_PREV"
        const val ACTION_NOTIFICATION_STOP = "io.casey.musikcube.remote.PAUSE_SHUT_DOWN"
        var ACTION_WAKE_UP = "io.casey.musikcube.remote.WAKE_UP"
        var ACTION_SHUT_DOWN = "io.casey.musikcube.remote.SHUT_DOWN"
        var ACTION_SLEEP = "io.casey.musikcube.remote.SLEEP"

        var state = State.Dead
            private set

        val isWakeLockActive: Boolean
            get() { return wakeLockAcquireTime >= 0L }

        val wakeLockTime: Double
            get() {
                val current = when (wakeLockAcquireTime > -1L) {
                    true -> System.nanoTime() - wakeLockAcquireTime
                    false -> 0L
                }
                return (totalWakeLockTime + current).toDouble() / 1_000_000_000.0
            }

        private val BITMAP_OPTIONS = RequestOptions().diskCacheStrategy(DiskCacheStrategy.ALL)

        private const val MEDIA_SESSION_ACTIONS =
            PlaybackStateCompat.ACTION_PLAY_PAUSE or
            PlaybackStateCompat.ACTION_SKIP_TO_NEXT or
            PlaybackStateCompat.ACTION_SKIP_TO_PREVIOUS or
            PlaybackStateCompat.ACTION_FAST_FORWARD or
            PlaybackStateCompat.ACTION_REWIND

        private var wakeLockAcquireTime = -1L
        private var totalWakeLockTime = 0L

        fun wakeup() {
            val c = Application.instance
            ContextCompat.startForegroundService(
                c, Intent(c, SystemService::class.java).setAction(ACTION_WAKE_UP))
        }

        fun shutdown() {
            if (state != State.Dead) {
                val c = Application.instance
                ContextCompat.startForegroundService(
                    c, Intent(c, SystemService::class.java).setAction(ACTION_SHUT_DOWN))
            }
        }

        fun sleep() {
            if (state != State.Dead) {
                val c = Application.instance
                ContextCompat.startForegroundService(
                    c, Intent(c, SystemService::class.java).setAction(ACTION_SLEEP))
            }
        }
    }
}
