package io.casey.musikcube.remote.playback;

import android.app.PendingIntent;
import android.app.Service;
import android.content.BroadcastReceiver;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.SharedPreferences;
import android.graphics.Bitmap;
import android.media.AudioManager;
import android.os.Handler;
import android.os.IBinder;
import android.os.PowerManager;
import android.support.annotation.Nullable;
import android.support.v4.media.MediaMetadataCompat;
import android.support.v4.media.session.MediaSessionCompat;
import android.support.v4.media.session.PlaybackStateCompat;
import android.support.v7.app.NotificationCompat;
import android.util.Log;
import android.view.KeyEvent;

import com.bumptech.glide.Glide;
import com.bumptech.glide.load.engine.DiskCacheStrategy;
import com.bumptech.glide.request.animation.GlideAnimation;
import com.bumptech.glide.request.target.SimpleTarget;
import com.bumptech.glide.request.target.Target;

import io.casey.musikcube.remote.Application;
import io.casey.musikcube.remote.MainActivity;
import io.casey.musikcube.remote.R;
import io.casey.musikcube.remote.ui.model.AlbumArtModel;
import io.casey.musikcube.remote.util.Debouncer;
import io.casey.musikcube.remote.util.Strings;
import io.casey.musikcube.remote.websocket.Prefs;

/* basically a stub service that exists to keep a connection active to the
StreamingPlaybackService, which keeps music playing. TODO: should also hold
a partial wakelock to keep the radio from going to sleep. */
public class SystemService extends Service {
    private static final String TAG = "SystemService";
    private static final int NOTIFICATION_ID = 0xdeadbeef;
    private static final int HEADSET_HOOK_DEBOUNCE_MS = 500;
    private static final String ACTION_NOTIFICATION_PLAY = "io.casey.musikcube.remote.NOTIFICATION_PLAY";
    private static final String ACTION_NOTIFICATION_PAUSE = "io.casey.musikcube.remote.NOTIFICATION_PAUSE";
    private static final String ACTION_NOTIFICATION_NEXT = "io.casey.musikcube.remote.NOTIFICATION_NEXT";
    private static final String ACTION_NOTIFICATION_PREV = "io.casey.musikcube.remote.NOTIFICATION_PREV";
    public static final String ACTION_NOTIFICATION_STOP = "io.casey.musikcube.remote.PAUSE_SHUT_DOWN";
    public static String ACTION_WAKE_UP = "io.casey.musikcube.remote.WAKE_UP";
    public static String ACTION_SHUT_DOWN = "io.casey.musikcube.remote.SHUT_DOWN";
    public static String ACTION_SLEEP = "io.casey.musikcube.remote.SLEEP";

    private final static long MEDIA_SESSION_ACTIONS =
        PlaybackStateCompat.ACTION_PLAY_PAUSE |
        PlaybackStateCompat.ACTION_SKIP_TO_NEXT |
        PlaybackStateCompat.ACTION_SKIP_TO_PREVIOUS |
        PlaybackStateCompat.ACTION_FAST_FORWARD |
        PlaybackStateCompat.ACTION_REWIND;

    private Handler handler = new Handler();
    private SharedPreferences prefs;
    private StreamingPlaybackService playback;
    private PowerManager.WakeLock wakeLock;
    private PowerManager powerManager;
    private MediaSessionCompat mediaSession;
    private int headsetHookPressCount = 0;

    private AlbumArtModel albumArtModel = AlbumArtModel.empty();
    private Bitmap albumArt = null;
    private SimpleTarget<Bitmap> albumArtRequest;

    public static void wakeup() {
        final Context c = Application.getInstance();
        c.startService(new Intent(c, SystemService.class).setAction(ACTION_WAKE_UP));
    }

    public static void shutdown() {
        final Context c = Application.getInstance();
        c.startService(new Intent(c, SystemService.class).setAction(ACTION_SHUT_DOWN));
    }

    public static void sleep() {
        final Context c = Application.getInstance();
        c.startService(new Intent(c, SystemService.class).setAction(ACTION_SLEEP));
    }

    @Override
    public void onCreate() {
        super.onCreate();
        prefs = this.getSharedPreferences(Prefs.NAME, Context.MODE_PRIVATE);
        powerManager = (PowerManager) getSystemService(POWER_SERVICE);
        registerReceivers();
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        recycleAlbumArt();
        unregisterReceivers();
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        if (intent != null) {
            final String action = intent.getAction();
            if (ACTION_WAKE_UP.equals(action)) {
                wakeupNow();
            }
            else if (ACTION_SHUT_DOWN.equals(action)) {
                shutdownNow();
            }
            else if (ACTION_SLEEP.equals(action)) {
               sleepNow();
            }
            else if (handlePlaybackAction(action)) {
                wakeupNow();
                return super.onStartCommand(intent, flags, startId);
            }
        }

        return super.onStartCommand(intent, flags, startId);
    }

    @Nullable
    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }

    private void wakeupNow() {
        Log.d(TAG, "SystemService WAKE_UP");

        final boolean sleeping = (playback == null || wakeLock == null);

        if (playback == null) {
            playback = PlaybackServiceFactory.streaming(this);
        }

        if (wakeLock == null) {
            wakeLock = powerManager.newWakeLock(
                    PowerManager.PARTIAL_WAKE_LOCK, "StreamingPlaybackService");

            wakeLock.setReferenceCounted(false);
            wakeLock.acquire();
        }

        if (sleeping) {
            playback.connect(listener);
            initMediaSession();
        }
    }

    private void shutdownNow() {
        Log.d(TAG, "SystemService SHUT_DOWN");

        if (mediaSession != null) {
            mediaSession.release();
        }

        if (playback != null) {
            playback.disconnect(listener);
            playback = null;
        }

        if (wakeLock != null) {
            wakeLock.release();
            wakeLock = null;
        }

        stopSelf();
    }

    private void sleepNow() {
        Log.d(TAG, "SystemService SLEEP");

        if (wakeLock != null) {
            wakeLock.release();
            wakeLock = null;
        }

        if (playback != null) {
            playback.disconnect(listener);
        }
    }

    private void initMediaSession() {
        ComponentName receiver = new ComponentName(getPackageName(), MediaButtonReceiver.class.getName());

        mediaSession = new MediaSessionCompat(this, "musikdroid.SystemService", receiver, null);

        mediaSession.setFlags(
            MediaSessionCompat.FLAG_HANDLES_MEDIA_BUTTONS |
            MediaSessionCompat.FLAG_HANDLES_TRANSPORT_CONTROLS);

        mediaSession.setCallback(mediaSessionCallback);

        updateMediaSessionPlaybackState();
        mediaSession.setActive(true);
    }

    private void registerReceivers() {
        final IntentFilter filter = new IntentFilter(AudioManager.ACTION_AUDIO_BECOMING_NOISY);
        registerReceiver(headsetUnpluggedReceiver, filter);
    }

    private void unregisterReceivers() {
        try {
            unregisterReceiver(headsetUnpluggedReceiver);
        }
        catch (Exception ex) {
            Log.e(TAG, "unable to unregister headset (un)plugged BroadcastReceiver");
        }
    }

    private void updateMediaSessionPlaybackState() {
        int mediaSessionState = PlaybackStateCompat.STATE_STOPPED;

        String title = "-";
        String album = "-";
        String artist = "-";
        int duration = 0;

        if (playback != null) {
            switch (playback.getPlaybackState()) {
                case Playing:
                    mediaSessionState =  PlaybackStateCompat.STATE_PLAYING;
                    break;
                case Buffering:
                    mediaSessionState = PlaybackStateCompat.STATE_BUFFERING;
                    break;
                case Paused:
                    mediaSessionState = PlaybackStateCompat.STATE_PAUSED;
                    break;
            }

            title = playback.getTrackString(Metadata.Track.TITLE, "-");
            album = playback.getTrackString(Metadata.Track.ALBUM, "-");
            artist = playback.getTrackString(Metadata.Track.ARTIST, "-");
            duration = (int) (playback.getDuration() * 1000);
        }

        updateMetadata(title, artist, album, null, duration);
        updateNotification(title, artist, album, mediaSessionState);

        mediaSession.setPlaybackState(new PlaybackStateCompat.Builder()
            .setState(mediaSessionState, 0, 0)
            .setActions(MEDIA_SESSION_ACTIONS)
            .build());
    }

    private synchronized void recycleAlbumArt() {
        if (albumArt != null) {
            //albumArt.recycle();
            albumArt = null;
        }
    }

    private void downloadAlbumArt(final String title, final String artist, final String album, final int duration) {
        recycleAlbumArt();

        albumArtModel = new AlbumArtModel(title, artist, album, AlbumArtModel.Size.Mega, (info, url) -> {
            if (albumArtModel.is(artist, album)) {
                handler.post(() -> {
                    if (albumArtRequest != null && albumArtRequest.getRequest() != null) {
                        albumArtRequest.getRequest().clear();
                    }

                    albumArtRequest = Glide
                        .with(getApplicationContext())
                        .load(url)
                        .asBitmap()
                        .diskCacheStrategy(DiskCacheStrategy.ALL)
                        .into(new SimpleTarget<Bitmap>(Target.SIZE_ORIGINAL, Target.SIZE_ORIGINAL) {
                            @Override
                            public void onResourceReady(final Bitmap bitmap, GlideAnimation glideAnimation) {
                                albumArtRequest = null;
                                if (albumArtModel.is(artist, album)) {
                                    albumArt = bitmap;
                                    updateMetadata(title, artist, album, bitmap, duration);
                                }
                            }
                        });
                });
            }
        });

        albumArtModel.fetch();
    }

    private void updateMetadata(final String title, final String artist, final String album, Bitmap image, final int duration) {
        boolean albumArtEnabledInSettings = this.prefs.getBoolean(
            Prefs.Key.ALBUM_ART_ENABLED, Prefs.Default.ALBUM_ART_ENABLED);

        if (albumArtEnabledInSettings) {
            if (!"-".equals(artist) && !"-".equals(album) && !albumArtModel.is(artist, album)) {
                downloadAlbumArt(title, artist, album, duration);
            }
            else if (albumArtModel.is(artist, album)) {
                if (image == null && Strings.notEmpty(albumArtModel.getUrl())) {
                    /* lookup may have failed -- try again. if the fetch is already in
                    progress this will be a no-op */
                    albumArtModel.fetch();
                }

                image = albumArt;
            }
            else {
                recycleAlbumArt();
            }
        }

        mediaSession.setMetadata(new MediaMetadataCompat.Builder()
            .putString(MediaMetadataCompat.METADATA_KEY_ARTIST, artist)
            .putString(MediaMetadataCompat.METADATA_KEY_ALBUM, album)
            .putString(MediaMetadataCompat.METADATA_KEY_TITLE, title)
            .putLong(MediaMetadataCompat.METADATA_KEY_DURATION, duration)
            .putBitmap(MediaMetadataCompat.METADATA_KEY_ALBUM_ART, image)
            .build());
    }

    private void updateNotification(final String title, final String artist, final String album, final int state) {
        final PendingIntent contentIntent = PendingIntent.getActivity(
            getApplicationContext(), 1, MainActivity.getStartIntent(this), 0);

        android.support.v4.app.NotificationCompat.Builder notification = new NotificationCompat.Builder(this)
            .setSmallIcon(R.mipmap.ic_notification)
            .setContentTitle(title)
            .setContentText(artist + " - " + album)
            .setContentIntent(contentIntent)
            .setUsesChronometer(false)
            //.setLargeIcon(albumArtBitmap))
            .setVisibility(NotificationCompat.VISIBILITY_PUBLIC)
            .setOngoing(true);

        if (state == PlaybackStateCompat.STATE_STOPPED) {
            notification.addAction(action(
                android.R.drawable.ic_media_play,
                getString(R.string.button_play),
                ACTION_NOTIFICATION_PLAY));

            notification.setStyle(new NotificationCompat.MediaStyle()
                .setShowActionsInCompactView(0)
                .setMediaSession(mediaSession.getSessionToken()));
        }
        else {
            if (state == PlaybackStateCompat.STATE_PAUSED) {
                notification.addAction(action(
                    android.R.drawable.ic_media_play,
                    getString(R.string.button_play),
                    ACTION_NOTIFICATION_PLAY));

                notification.addAction(action(
                    android.R.drawable.ic_menu_close_clear_cancel,
                    getString(R.string.button_close),
                    ACTION_NOTIFICATION_STOP));

                notification.setStyle(new NotificationCompat.MediaStyle()
                    .setShowActionsInCompactView(0, 1)
                    .setMediaSession(mediaSession.getSessionToken()));
            }
            else {
                notification.addAction(action(
                    android.R.drawable.ic_media_previous,
                    getString(R.string.button_prev),
                    ACTION_NOTIFICATION_PREV));

                notification.addAction(action(
                    android.R.drawable.ic_media_pause,
                    getString(R.string.button_pause),
                    ACTION_NOTIFICATION_PAUSE));

                notification.addAction(action(
                    android.R.drawable.ic_media_next,
                    getString(R.string.button_next),
                    ACTION_NOTIFICATION_NEXT));

                notification.setStyle(new NotificationCompat.MediaStyle()
                    .setShowActionsInCompactView(0, 1, 2)
                    .setMediaSession(mediaSession.getSessionToken()));
            }
        }

        startForeground(NOTIFICATION_ID, notification.build());
    }

    private NotificationCompat.Action action(int icon, String title, String intentAction) {
        Intent intent = new Intent(getApplicationContext(), SystemService.class);
        intent.setAction(intentAction);
        PendingIntent pendingIntent = PendingIntent.getService(getApplicationContext(), 1, intent, 0);
        return new NotificationCompat.Action.Builder(icon, title, pendingIntent).build();
    }

    private boolean handlePlaybackAction(final String action) {
        if (this.playback != null && Strings.notEmpty(action)) {
            switch (action) {
                case ACTION_NOTIFICATION_NEXT:
                    this.playback.next();
                    return true;

                case ACTION_NOTIFICATION_PAUSE:
                    this.playback.pause();
                    break;

                case ACTION_NOTIFICATION_PLAY:
                    this.playback.resume();
                    return true;

                case ACTION_NOTIFICATION_PREV:
                    this.playback.prev();
                    return true;

                case ACTION_NOTIFICATION_STOP:
                    this.playback.stop();
                    SystemService.shutdown();
                    return true;
            }
        }
        return false;
    }

    private Debouncer<Void> headsetHookDebouncer = new Debouncer<Void>(HEADSET_HOOK_DEBOUNCE_MS) {
        @Override
        protected void onDebounced(Void caller) {
            if (headsetHookPressCount == 1) {
                playback.pauseOrResume();
            }
            else if (headsetHookPressCount == 2) {
                playback.next();
            }
            else if (headsetHookPressCount > 2) {
                playback.prev();
            }
            headsetHookPressCount = 0;
        }
    };

    private MediaSessionCompat.Callback mediaSessionCallback = new MediaSessionCompat.Callback() {
        @Override
        public boolean onMediaButtonEvent(Intent mediaButtonEvent) {
            if (Intent.ACTION_MEDIA_BUTTON.equals(mediaButtonEvent.getAction())) {
                final KeyEvent event = mediaButtonEvent.getParcelableExtra(Intent.EXTRA_KEY_EVENT);
                if (event == null) {
                    return super.onMediaButtonEvent(mediaButtonEvent);
                }

                final int keycode = event.getKeyCode();
                final int action = event.getAction();
                if (event.getRepeatCount() == 0 && action == KeyEvent.ACTION_DOWN) {
                    switch (keycode) {
                        case KeyEvent.KEYCODE_HEADSETHOOK:
                            ++headsetHookPressCount;
                            headsetHookDebouncer.call();
                            return true;
                        case KeyEvent.KEYCODE_MEDIA_STOP:
                            playback.pause();
                            SystemService.shutdown();
                            return true;
                        case KeyEvent.KEYCODE_MEDIA_PLAY_PAUSE:
                            playback.pauseOrResume();
                            return true;
                        case KeyEvent.KEYCODE_MEDIA_NEXT:
                            playback.next();
                            return true;
                        case KeyEvent.KEYCODE_MEDIA_PREVIOUS:
                            playback.prev();
                            return true;
                        case KeyEvent.KEYCODE_MEDIA_PAUSE:
                            playback.pause();
                            return true;
                        case KeyEvent.KEYCODE_MEDIA_PLAY:
                            playback.resume();
                            return true;
                    }
                }
            }
            return super.onMediaButtonEvent(mediaButtonEvent);
        }

        @Override
        public void onPlay() {
            super.onPlay();
            if (playback.getQueueCount() == 0) {
                playback.playAll();
            }
            else {
                playback.resume();
            }
        }

        @Override
        public void onPause() {
            super.onPause();
            playback.pause();
        }

        @Override
        public void onSkipToNext() {
            super.onSkipToNext();
            playback.next();
        }

        @Override
        public void onSkipToPrevious() {
            super.onSkipToPrevious();
            playback.prev();
        }

        @Override
        public void onFastForward() {
            super.onFastForward();
            playback.seekForward();
        }

        @Override
        public void onRewind() {
            super.onRewind();
            playback.seekBackward();
        }
    };

    private PlaybackService.EventListener listener = () -> {
        updateMediaSessionPlaybackState();
    };

    private BroadcastReceiver headsetUnpluggedReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            if (intent.getAction().equals(AudioManager.ACTION_AUDIO_BECOMING_NOISY)) {
                if (playback != null) {
                    playback.pause();
                }
            }
        }
    };
}
