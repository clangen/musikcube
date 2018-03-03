package io.casey.musikcube.remote.service.system

import android.app.*
import android.content.*
import android.graphics.Bitmap
import android.graphics.drawable.Drawable
import android.media.AudioManager
import android.os.Build
import android.os.IBinder
import android.os.PowerManager
import android.support.v4.app.NotificationCompat
import android.support.v4.content.ContextCompat
import android.support.v4.media.MediaMetadataCompat
import android.support.v4.media.app.NotificationCompat.MediaStyle
import android.support.v4.media.session.MediaSessionCompat
import android.support.v4.media.session.PlaybackStateCompat
import android.util.Log
import android.view.KeyEvent
import com.bumptech.glide.load.engine.DiskCacheStrategy
import com.bumptech.glide.request.RequestOptions
import com.bumptech.glide.request.target.SimpleTarget
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
import io.casey.musikcube.remote.ui.shared.model.albumart.Size
import io.casey.musikcube.remote.util.Debouncer
import io.casey.musikcube.remote.util.Strings
import android.support.v4.app.NotificationCompat.Action as NotifAction
import io.casey.musikcube.remote.ui.shared.model.albumart.getUrl as getAlbumArtUrl

/**
 * a service used to interact with all of the system media-related components -- notifications,
 * lock screen controls, and headset events. also holds a partial wakelock to keep the system
 * from completely falling asleep during streaming playback.
 */
class SystemService : Service() {
    private var playback: StreamingPlaybackService? = null
    private var wakeLock: PowerManager.WakeLock? = null
    private var mediaSession: MediaSessionCompat? = null
    private var headsetHookPressCount = 0

    private lateinit var powerManager: PowerManager
    private lateinit var prefs: SharedPreferences

    private val albumArt = AlbumArt()
    private val sessionData = SessionMetadata()

    override fun onCreate() {
        RUNNING = true

        super.onCreate()

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            val channel = NotificationChannel(
                    NOTIFICATION_CHANNEL,
                    NOTIFICATION_CHANNEL,
                NotificationManager.IMPORTANCE_LOW)

            channel.enableVibration(false)
            channel.setSound(null, null)
            channel.lockscreenVisibility = Notification.VISIBILITY_PUBLIC

            val nm = getSystemService(Service.NOTIFICATION_SERVICE) as NotificationManager
            nm.deleteNotificationChannel(NOTIFICATION_CHANNEL)
            nm.createNotificationChannel(channel)
        }

        prefs = getSharedPreferences(Prefs.NAME, Context.MODE_PRIVATE)
        powerManager = getSystemService(Context.POWER_SERVICE) as PowerManager
        registerReceivers()

        wakeupNow()
    }

    override fun onDestroy() {
        RUNNING = false
        super.onDestroy()
        unregisterReceivers()
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
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

        if (wakeLock == null) {
            wakeLock = powerManager.newWakeLock(
                PowerManager.PARTIAL_WAKE_LOCK, "StreamingPlaybackService")

            wakeLock?.let {
                it.setReferenceCounted(false)
                it.acquire()
            }
        }

        if (sleeping) {
            playback?.connect(playbackListener)
            initMediaSession()
        }
    }

    private fun shutdownNow() {
        Log.d(TAG, "SystemService SHUT_DOWN")

        mediaSession?.release()
        mediaSession = null

        playback?.disconnect(playbackListener)
        playback = null

        wakeLock?.release()
        wakeLock = null

        stopSelf()
    }

    private fun sleepNow() {
        Log.d(TAG, "SystemService SLEEP")
        wakeLock?.release()
        wakeLock = null
        playback?.disconnect(playbackListener)
    }

    private fun initMediaSession() {
        val receiver = ComponentName(packageName, MediaButtonReceiver::class.java.name)

        mediaSession = MediaSessionCompat(this, "musikdroid.SystemService", receiver, null)

        mediaSession?.setFlags(
            MediaSessionCompat.FLAG_HANDLES_MEDIA_BUTTONS or
            MediaSessionCompat.FLAG_HANDLES_TRANSPORT_CONTROLS)

        mediaSession?.setCallback(mediaSessionCallback)

        updateMediaSessionPlaybackState()

        mediaSession?.isActive = true
    }

    private fun registerReceivers() {
        val filter = IntentFilter(AudioManager.ACTION_AUDIO_BECOMING_NOISY)
        registerReceiver(headsetUnpluggedReceiver, filter)
    }

    private fun unregisterReceivers() {
        try {
            unregisterReceiver(headsetUnpluggedReceiver)
        }
        catch (ex: Exception) {
            Log.e(TAG, "unable to unregister headset (un)plugged BroadcastReceiver")
        }
    }

    private fun updateMediaSessionPlaybackState() {
        var mediaSessionState = PlaybackStateCompat.STATE_STOPPED

        var duration = 0
        var playing: ITrack? = null

        if (playback != null) {
            when (playback?.state) {
                PlaybackState.Playing -> mediaSessionState = PlaybackStateCompat.STATE_PLAYING
                PlaybackState.Buffering -> mediaSessionState = PlaybackStateCompat.STATE_BUFFERING
                PlaybackState.Paused -> mediaSessionState = PlaybackStateCompat.STATE_PAUSED
                else -> { }
            }

            playing = playback!!.playingTrack
            duration = ((playback?.duration ?: 0.0) * 1000).toInt()
        }

        updateMediaSession(playing, duration)
        updateNotification(playing, mediaSessionState)

        mediaSession?.setPlaybackState(PlaybackStateCompat.Builder()
            .setState(mediaSessionState, 0, 0f)
            .setActions(MEDIA_SESSION_ACTIONS)
            .build())
    }

    private fun downloadAlbumArtIfNecessary(track: ITrack, duration: Int) {
        if (!albumArt.same(track) || (albumArt.request == null && albumArt.bitmap == null)) {
            if (track.artist.isNotBlank() && track.album.isNotBlank()) {
                Log.d(TAG, "download")

                val url = getAlbumArtUrl(track, Size.Mega)

                val originalRequest = GlideApp.with(applicationContext)
                        .asBitmap().load(url).apply(BITMAP_OPTIONS)

                albumArt.reset(track)
                albumArt.request = originalRequest
                albumArt.target = originalRequest.into(object : SimpleTarget<Bitmap>(Target.SIZE_ORIGINAL, Target.SIZE_ORIGINAL) {
                    override fun onResourceReady(bitmap: Bitmap?, transition: Transition<in Bitmap>?) {
                        /* make sure the instance's current request is the same as this request. it's
                        possible we had another download request come in before this one finished */
                        if (albumArt.request == originalRequest) {
                            albumArt.bitmap = bitmap
                            albumArt.request = null
                            updateMediaSession(track, duration)
                        }
                    }

                    override fun onLoadFailed(errorDrawable: Drawable?) {
                        if (albumArt.request == originalRequest) {
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

    private fun updateNotification(track: ITrack?, state: Int) {
        val contentIntent = PendingIntent.getActivity(
            applicationContext, 1, MainActivity.getStartIntent(this), 0)

        val title = fallback(track?.title, "-")
        val artist = fallback(track?.artist, "-")
        val album = fallback(track?.album, "-")

        val notification = NotificationCompat.Builder(this, NOTIFICATION_CHANNEL)
            .setSmallIcon(R.drawable.ic_notification)
            .setContentTitle(title)
            .setContentText(artist + " - " + album)
            .setContentIntent(contentIntent)
            .setUsesChronometer(false)
            .setVisibility(NotificationCompat.VISIBILITY_PUBLIC)
            .setOngoing(true)

        if (state == PlaybackStateCompat.STATE_STOPPED) {
            notification.addAction(action(
                android.R.drawable.ic_media_play,
                getString(R.string.button_play),
                    ACTION_NOTIFICATION_PLAY))

            notification.setStyle(MediaStyle()
                .setShowActionsInCompactView(0)
                .setMediaSession(mediaSession?.sessionToken))
        }
        else {
            if (state == PlaybackStateCompat.STATE_PAUSED) {
                notification.addAction(action(
                    android.R.drawable.ic_media_play,
                    getString(R.string.button_play),
                        ACTION_NOTIFICATION_PLAY))

                notification.addAction(action(
                    android.R.drawable.ic_menu_close_clear_cancel,
                    getString(R.string.button_close),
                        ACTION_NOTIFICATION_STOP))

                notification.setStyle(MediaStyle()
                    .setShowActionsInCompactView(0, 1)
                    .setMediaSession(mediaSession?.sessionToken))
            }
            else {
                notification.addAction(action(
                    android.R.drawable.ic_media_previous,
                    getString(R.string.button_prev),
                        ACTION_NOTIFICATION_PREV))

                notification.addAction(action(
                    android.R.drawable.ic_media_pause,
                    getString(R.string.button_pause),
                        ACTION_NOTIFICATION_PAUSE))

                notification.addAction(action(
                    android.R.drawable.ic_media_next,
                    getString(R.string.button_next),
                        ACTION_NOTIFICATION_NEXT))

                notification.setStyle(MediaStyle()
                    .setShowActionsInCompactView(0, 1, 2)
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

    private fun handlePlaybackAction(action: String): Boolean {
        if (this.playback != null && Strings.notEmpty(action)) {
            when (action) {
                ACTION_NOTIFICATION_NEXT -> {
                    this.playback?.next()
                    return true
                }

                ACTION_NOTIFICATION_PAUSE -> {
                    this.playback?.pause()
                    return true
                }

                ACTION_NOTIFICATION_PLAY -> {
                    this.playback?.resume()
                    return true
                }

                ACTION_NOTIFICATION_PREV -> {
                    this.playback?.prev()
                    return true
                }

                ACTION_NOTIFICATION_STOP -> {
                    this.playback?.stop()
                    shutdown()
                    return true
                }
            }
        }
        return false
    }

    private val headsetHookDebouncer = object : Debouncer<Void>(HEADSET_HOOK_DEBOUNCE_MS) {
        override fun onDebounced(last: Void?) {
            if (headsetHookPressCount == 1) {
                playback?.pauseOrResume()
            }
            else if (headsetHookPressCount == 2) {
                playback?.next()
            }
            else if (headsetHookPressCount > 2) {
                playback?.prev()
            }
            headsetHookPressCount = 0
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
                            playback?.pause()
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
            if (playback?.queueCount == 0) {
                playback?.playAll()
            }
            else {
                playback?.resume()
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
        var target: SimpleTarget<Bitmap>? = null
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
        private val TAG = "SystemService"
        private val NOTIFICATION_ID = 0xdeadbeef.toInt()
        private val NOTIFICATION_CHANNEL = "musikdroid"
        private val HEADSET_HOOK_DEBOUNCE_MS = 500L
        private val ACTION_NOTIFICATION_PLAY = "io.casey.musikcube.remote.NOTIFICATION_PLAY"
        private val ACTION_NOTIFICATION_PAUSE = "io.casey.musikcube.remote.NOTIFICATION_PAUSE"
        private val ACTION_NOTIFICATION_NEXT = "io.casey.musikcube.remote.NOTIFICATION_NEXT"
        private val ACTION_NOTIFICATION_PREV = "io.casey.musikcube.remote.NOTIFICATION_PREV"
        val ACTION_NOTIFICATION_STOP = "io.casey.musikcube.remote.PAUSE_SHUT_DOWN"
        var ACTION_WAKE_UP = "io.casey.musikcube.remote.WAKE_UP"
        var ACTION_SHUT_DOWN = "io.casey.musikcube.remote.SHUT_DOWN"
        var ACTION_SLEEP = "io.casey.musikcube.remote.SLEEP"
        var RUNNING = false

        private val BITMAP_OPTIONS = RequestOptions().diskCacheStrategy(DiskCacheStrategy.ALL)

        private val MEDIA_SESSION_ACTIONS =
            PlaybackStateCompat.ACTION_PLAY_PAUSE or
            PlaybackStateCompat.ACTION_SKIP_TO_NEXT or
            PlaybackStateCompat.ACTION_SKIP_TO_PREVIOUS or
            PlaybackStateCompat.ACTION_FAST_FORWARD or
            PlaybackStateCompat.ACTION_REWIND

        fun wakeup() {
            val c = Application.instance
            ContextCompat.startForegroundService(
                c, Intent(c, SystemService::class.java).setAction(ACTION_WAKE_UP))
        }

        fun shutdown() {
            if (RUNNING) {
                val c = Application.instance
                ContextCompat.startForegroundService(
                    c, Intent(c, SystemService::class.java).setAction(ACTION_SHUT_DOWN))
            }
        }

        fun sleep() {
            if (RUNNING) {
                val c = Application.instance
                ContextCompat.startForegroundService(
                    c, Intent(c, SystemService::class.java).setAction(ACTION_SLEEP))
            }
        }
    }
}
