package io.casey.musikcube.remote.playback;

import android.content.Context;
import android.content.SharedPreferences;
import android.content.res.Resources;
import android.database.ContentObserver;
import android.media.AudioManager;
import android.os.Handler;
import android.os.Looper;
import android.provider.Settings;
import android.util.Log;

import org.json.JSONArray;
import org.json.JSONObject;

import java.net.URLEncoder;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Random;
import java.util.Set;

import io.casey.musikcube.remote.Application;
import io.casey.musikcube.remote.R;
import io.casey.musikcube.remote.ui.model.TrackListSlidingWindow;
import io.casey.musikcube.remote.util.Strings;
import io.casey.musikcube.remote.websocket.Messages;
import io.casey.musikcube.remote.websocket.Prefs;
import io.casey.musikcube.remote.websocket.SocketMessage;
import io.casey.musikcube.remote.websocket.WebSocketService;
import io.reactivex.Observable;
import io.reactivex.android.schedulers.AndroidSchedulers;

public class StreamingPlaybackService implements PlaybackService {
    private static final String TAG = "StreamingPlayback";
    private static final String REPEAT_MODE_PREF = "streaming_playback_repeat_mode";
    private static final int PREV_TRACK_GRACE_PERIOD_MILLIS = 3500;
    private static final int MAX_TRACK_METADATA_CACHE_SIZE = 50;
    private static final int PRECACHE_METADATA_SIZE = 10;
    private static final int PAUSED_SERVICE_SLEEP_DELAY_MS = 1000 * 60 * 5; /* 5 minutes */

    private WebSocketService wss;
    private Set<EventListener> listeners = new HashSet<>();
    private boolean shuffled, muted;
    private RepeatMode repeatMode = RepeatMode.None;
    private QueueParams params;
    private PlaybackContext context = new PlaybackContext();
    private PlaybackState state = PlaybackState.Stopped;
    private AudioManager audioManager;
    private SharedPreferences prefs;
    private int lastSystemVolume;
    private boolean pausedByTransientLoss = false;
    private Random random = new Random();
    private Handler handler = new Handler();

    private Map<Integer, JSONObject> trackMetadataCache = new LinkedHashMap<Integer, JSONObject>() {
        protected boolean removeEldestEntry(Map.Entry<Integer, JSONObject> eldest) {
            return size() >= MAX_TRACK_METADATA_CACHE_SIZE;
        }
    };

    private static class PlaybackContext {
        int queueCount;
        PlayerWrapper currentPlayer, nextPlayer;
        JSONObject currentMetadata, nextMetadata;
        int currentIndex = -1, nextIndex = -1;
        boolean nextPlayerScheduled;

        public void stopPlaybackAndReset() {
            reset(currentPlayer);
            reset(nextPlayer);
            nextPlayerScheduled = false;
            this.currentPlayer = this.nextPlayer = null;
            this.currentMetadata = this.nextMetadata = null;
            this.currentIndex = this.nextIndex = -1;
        }

        public void notifyNextTrackPrepared() {
            if (currentPlayer != null && nextPlayer != null) {
                currentPlayer.setNextMediaPlayer(nextPlayer);
                nextPlayerScheduled = true;
            }
        }

        public boolean advanceToNextTrack(
            final PlayerWrapper.OnStateChangedListener currentTrackListener)
        {
            boolean startedNext = false;

            if (nextMetadata != null && nextPlayer != null) {
                if (currentPlayer != null) {
                    currentPlayer.setOnStateChangedListener(null);
                    currentPlayer.dispose();
                }

                currentMetadata = nextMetadata;
                currentIndex = nextIndex;
                currentPlayer = nextPlayer;
                startedNext = true;
            }
            else {
                reset(currentPlayer);
                currentPlayer = null;
                currentMetadata = null;
                currentIndex = 0;
            }

            nextPlayer = null;
            nextMetadata = null;
            nextIndex = -1;
            nextPlayerScheduled = false;

            /* needs to be done after swapping current/next, otherwise event handlers
            will fire, and things may get cleaned up before we have a chance to start */
            if (startedNext) {
                currentPlayer.setOnStateChangedListener(currentTrackListener);
                currentPlayer.resume(); /* no-op if playing already */
            }

            return startedNext;
        }

        public void reset(final PlayerWrapper p) {
            if (p != null) {
                p.setOnStateChangedListener(null);
                p.dispose();

                if (p == nextPlayer) {
                    nextPlayerScheduled = false; /* uhh... */
                }
            }
        }
    }

    private static class QueueParams {
        final String category;
        final long categoryId;
        final String filter;

        public QueueParams(String filter) {
            this.filter = filter;
            this.categoryId = -1;
            this.category = null;
        }

        public QueueParams(String category, long categoryId, String filter) {
            this.category = category;
            this.categoryId = categoryId;
            this.filter = filter;
        }
    }

    public StreamingPlaybackService(final Context context) {
        this.wss = WebSocketService.getInstance(context.getApplicationContext());
        this.prefs = context.getSharedPreferences(Prefs.NAME, Context.MODE_PRIVATE);
        this.audioManager = (AudioManager) Application.getInstance().getSystemService(Context.AUDIO_SERVICE);
        this.lastSystemVolume = audioManager.getStreamVolume(AudioManager.STREAM_MUSIC);
        this.repeatMode = RepeatMode.from(this.prefs.getString(REPEAT_MODE_PREF, RepeatMode.None.toString()));
        this.audioManager = (AudioManager) context.getSystemService(Context.AUDIO_SERVICE);
        context.getContentResolver().registerContentObserver(Settings.System.CONTENT_URI, true, new SettingsContentObserver());
    }

    @Override
    public synchronized void connect(EventListener listener) {
        if (listener != null) {
            listeners.add(listener);
            if (listeners.size() == 1) {
                wss.addClient(wssClient);
            }
        }
    }

    @Override
    public synchronized void disconnect(EventListener listener) {
        if (listener != null) {
            listeners.remove(listener);
            if (listeners.size() == 0) {
                wss.removeClient(wssClient);
            }
        }
    }

    @Override
    public void playAll() {
        playAll(0, "");
    }

    @Override
    public void playAll(int index, String filter) {
        if (requestAudioFocus()) {
            trackMetadataCache.clear();
            loadQueueAndPlay(new QueueParams(filter), index);
        }
    }

    @Override
    public void play(String category, long categoryId, int index, String filter) {
        if (requestAudioFocus()) {
            trackMetadataCache.clear();
            loadQueueAndPlay(new QueueParams(category, categoryId, filter), index);
        }
    }

    @Override
    public void playAt(int index) {
        if (requestAudioFocus()) {
            this.context.stopPlaybackAndReset();
            loadQueueAndPlay(this.params, index);
        }
    }

    @Override
    public void pauseOrResume() {
        if (context.currentPlayer != null) {
            if (state == PlaybackState.Playing || state == PlaybackState.Buffering) {
                pause();
            }
            else {
                resume();
            }
        }
    }

    @Override
    public void pause() {
        if (state != PlaybackState.Paused) {
            schedulePausedSleep();
            killAudioFocus();

            if (context.currentPlayer != null) {
                context.currentPlayer.pause();
            }

            setState(PlaybackState.Paused);
        }
    }

    @Override
    public void resume() {
        if (requestAudioFocus()) {
            cancelScheduledPausedSleep();
            context.currentPlayer.resume();
            setState(PlaybackState.Playing);
            pausedByTransientLoss = false;
        }
    }

    @Override
    public void stop() {
        SystemService.shutdown();
        killAudioFocus();
        this.context.stopPlaybackAndReset();
        this.trackMetadataCache.clear();
        setState(PlaybackState.Stopped);
    }

    @Override
    public void prev() {
        if (requestAudioFocus()) {
            cancelScheduledPausedSleep();

            if (context.currentPlayer != null) {
                if (context.currentPlayer.getPosition() > PREV_TRACK_GRACE_PERIOD_MILLIS) {
                    context.currentPlayer.setPosition(0);
                    return;
                }
            }
            moveToPrevTrack();
        }
    }

    @Override
    public void next() {
        if (requestAudioFocus()) {
            cancelScheduledPausedSleep();
            moveToNextTrack(true);
        }
    }

    @Override
    public void volumeUp() {
        adjustVolume(getVolumeStep());
    }

    @Override
    public void volumeDown() {
        adjustVolume(-getVolumeStep());
    }

    @Override
    public void seekForward() {
        if (requestAudioFocus()) {
            if (context.currentPlayer != null) {
                context.currentPlayer.setPosition(context.currentPlayer.getPosition() + 5000);
            }
        }
    }

    @Override
    public void seekBackward() {
        if (requestAudioFocus()) {
            if (context.currentPlayer != null) {
                context.currentPlayer.setPosition(context.currentPlayer.getPosition() - 5000);
            }
        }
    }

    @Override
    public void seekTo(double seconds) {
        if (requestAudioFocus()) {
            if (context.currentPlayer != null) {
                context.currentPlayer.setPosition((int)(seconds * 1000));
            }
        }
    }

    @Override
    public int getQueueCount() {
        return context.queueCount;
    }

    @Override
    public int getQueuePosition() {
        return context.currentIndex;
    }

    @Override
    public double getVolume() {
        if (prefs.getBoolean(Prefs.Key.SOFTWARE_VOLUME, Prefs.Default.SOFTWARE_VOLUME)) {
            return PlayerWrapper.Companion.getVolume();
        }

        return getSystemVolume();
    }

    @Override
    public double getDuration() {
        if (context.currentPlayer != null) {
            return (context.currentPlayer.getDuration() / 1000.0);
        }

        return 0;
    }

    @Override
    public double getCurrentTime() {
        if (context.currentPlayer != null) {
            return (context.currentPlayer.getPosition() / 1000.0);
        }

        return 0;
    }

    @Override
    public PlaybackState getPlaybackState() {
        return this.state;
    }

    @Override
    public void toggleShuffle() {
        shuffled = !shuffled;
        invalidateAndPrefetchNextTrackMetadata();
        notifyEventListeners();
    }

    @Override
    public boolean isShuffled() {
        return shuffled;
    }

    @Override
    public void toggleMute() {
        muted = !muted;
        PlayerWrapper.Companion.setMute(muted);
        notifyEventListeners();
    }

    @Override
    public boolean isMuted() {
        return muted;
    }

    @Override
    public void toggleRepeatMode() {
        switch (repeatMode) {
            case None: repeatMode = RepeatMode.List; break;
            case List: repeatMode = RepeatMode.Track; break;
            default: repeatMode = RepeatMode.None;
        }
        this.prefs.edit().putString(REPEAT_MODE_PREF, repeatMode.toString()).apply();
        invalidateAndPrefetchNextTrackMetadata();
        notifyEventListeners();
    }

    @Override
    public RepeatMode getRepeatMode() {
        return repeatMode;
    }

    @Override
    public String getTrackString(String key, String defaultValue) {
        if (context.currentMetadata != null) {
            return context.currentMetadata.optString(key, defaultValue);
        }
        return defaultValue;
    }

    @Override
    public long getTrackLong(String key, long defaultValue) {
        if (context.currentMetadata != null) {
            return context.currentMetadata.optLong(key, defaultValue);
        }
        return defaultValue;
    }

    @Override
    public double getBufferedTime() {
        if (context.currentPlayer != null) {
            float percent = (float) context.currentPlayer.getBufferedPercent() / 100.0f;
            return percent * (float) context.currentPlayer.getDuration() / 1000.0f; /* ms -> sec */
        }
        return 0;
    }

    @Override
    public TrackListSlidingWindow.QueryFactory getPlaylistQueryFactory() {
        return this.queryFactory;
    }

    private void pauseTransient() {
        if (state != PlaybackState.Paused) {
            pausedByTransientLoss = true;
            setState(PlaybackState.Paused);
            context.currentPlayer.pause();
        }
    }

    private float getVolumeStep() {
        if (prefs.getBoolean(Prefs.Key.SOFTWARE_VOLUME, Prefs.Default.SOFTWARE_VOLUME)) {
            return 0.1f;
        }
        return 1.0f / getMaxSystemVolume();
    }

    private void adjustVolume(float delta) {
        if (muted) {
            toggleMute();
        }

        final boolean softwareVolume = prefs.getBoolean(Prefs.Key.SOFTWARE_VOLUME, Prefs.Default.SOFTWARE_VOLUME);
        float current = softwareVolume ? PlayerWrapper.Companion.getVolume() : getSystemVolume();

        current += delta;
        if (current > 1.0) current = 1.0f;
        if (current < 0.0) current = 0.0f;

        if (softwareVolume) {
            PlayerWrapper.Companion.setVolume(current);
        }
        else {
            final int actual = Math.round(current * getMaxSystemVolume());
            lastSystemVolume = actual;
            audioManager.setStreamVolume(AudioManager.STREAM_MUSIC, actual, 0);
        }

        notifyEventListeners();
    }

    private float getSystemVolume() {
        float current = audioManager.getStreamVolume(AudioManager.STREAM_MUSIC);
        return current / getMaxSystemVolume();
    }


    private float getMaxSystemVolume() {
        return audioManager.getStreamMaxVolume(AudioManager.STREAM_MUSIC);
    }

    private void killAudioFocus() {
        audioManager.abandonAudioFocus(audioFocusChangeListener);
    }

    private boolean requestAudioFocus() {
        return
            audioManager.requestAudioFocus(
                audioFocusChangeListener,
                AudioManager.STREAM_MUSIC,
                AudioManager.AUDIOFOCUS_GAIN) == AudioManager.AUDIOFOCUS_REQUEST_GRANTED;
    }

    private void moveToPrevTrack() {
        if (this.context.queueCount > 0) {
            loadQueueAndPlay(this.params, resolvePrevIndex(
                this.context.currentIndex, this.context.queueCount));
        }
    }

    private void moveToNextTrack(boolean userInitiated) {
        int index = context.currentIndex;
        if (!userInitiated && context.advanceToNextTrack(onCurrentPlayerStateChanged)) {
            notifyEventListeners();
            prefetchNextTrackMetadata();
        }
        else {
            /* failed! reset by loading as if the user selected the next track
            manually (this will automatically load the current and next tracks */
            final int next = resolveNextIndex(index, context.queueCount, userInitiated);
            if (next >= 0) {
                loadQueueAndPlay(params, next);
            }
            else {
                stop();
            }
        }
    }

    private PlayerWrapper.OnStateChangedListener onCurrentPlayerStateChanged = (mpw, playerState) -> {
        switch (playerState) {
            case Playing:
                setState(PlaybackState.Playing);
                prefetchNextTrackAudio();
                cancelScheduledPausedSleep();
                precacheTrackMetadata(context.currentIndex, PRECACHE_METADATA_SIZE);
                break;

            case Buffering:
                setState(PlaybackState.Buffering);
                break;

            case Paused:
                pause();
                break;

            case Error:
                pause();
                break;

            case Finished:
                if (state != PlaybackState.Paused) {
                    moveToNextTrack(false);
                }
                break;
        }
    };

    private PlayerWrapper.OnStateChangedListener onNextPlayerStateChanged = (mpw, playerState) -> {
        if (playerState == PlayerWrapper.State.Prepared) {
            if (mpw == context.nextPlayer) {
                context.notifyNextTrackPrepared();
            }
        }
    };

    private void setState(PlaybackState state) {
        if (this.state != state) {
            Log.d(TAG, "state = " + state);
            this.state = state;
            notifyEventListeners();
        }
    }

    private synchronized void notifyEventListeners() {
        for (final EventListener listener : listeners) {
            listener.onStateUpdated();
        }
    }

    private String getUri(final JSONObject track) {
        if (track != null) {
            final String externalId = track.optString("external_id", "");
            if (Strings.notEmpty(externalId)) {
                final String protocol = prefs.getBoolean(
                    Prefs.Key.SSL_ENABLED, Prefs.Default.SSL_ENABLED) ? "https" : "http";

                /* transcoding bitrate, if selected by the user */
                String bitrateQueryParam = "";
                final int bitrateIndex = prefs.getInt(
                    Prefs.Key.TRANSCODER_BITRATE_INDEX,
                    Prefs.Default.TRANSCODER_BITRATE_INDEX);

                if (bitrateIndex > 0) {
                    final Resources r = Application.getInstance().getResources();

                    bitrateQueryParam = String.format(
                        Locale.ENGLISH,
                        "?bitrate=%s",
                        r.getStringArray(R.array.transcode_bitrate_array)[bitrateIndex]);
                }

                return String.format(
                    Locale.ENGLISH,
                    "%s://%s:%d/audio/external_id/%s%s",
                    protocol,
                    prefs.getString(Prefs.Key.ADDRESS, Prefs.Default.ADDRESS),
                    prefs.getInt(Prefs.Key.AUDIO_PORT, Prefs.Default.AUDIO_PORT),
                    URLEncoder.encode(externalId),
                    bitrateQueryParam);
            }
        }
        return null;
    }

    private int resolvePrevIndex(final int currentIndex, final int count) {
        if (currentIndex - 1 < 0) {
            if (repeatMode == RepeatMode.List) {
                return count - 1;
            }
            return 0;
        }
        return currentIndex - 1;
    }

    private int resolveNextIndex(final int currentIndex, final int count, boolean userInitiated) {
        if (shuffled) { /* our shuffle is actually random for now. */
            if (count == 0) {
                return currentIndex;
            }

            int r = random.nextInt(count - 1);
            while (r == currentIndex) {
                r = random.nextInt(count - 1);
            }
            return r;
        }
        else if (!userInitiated && repeatMode == RepeatMode.Track) {
            return currentIndex;
        }
        else {
            if (currentIndex + 1 >= count) {
                if (repeatMode == RepeatMode.List) {
                    return 0;
                }
                else {
                    return -1;
                }
            }
            else {
                return currentIndex + 1;
            }
        }
    }

    private SocketMessage getMetadataQuery(int index) {
        return queryFactory.getRequeryMessage()
            .buildUpon()
            .removeOption(Messages.Key.COUNT_ONLY)
            .addOption(Messages.Key.LIMIT, 1)
            .addOption(Messages.Key.OFFSET, index)
            .build();
    }

    private Observable<SocketMessage> getCurrentAndNextTrackMessages(final PlaybackContext context, final int queueCount) {
        final List<Observable<SocketMessage>> tracks = new ArrayList<>();

        if (queueCount > 0) {
            if (trackMetadataCache.containsKey(context.currentIndex)) {
                context.currentMetadata = trackMetadataCache.get(context.currentIndex);
            }
            else {
                tracks.add(wss.send(getMetadataQuery(context.currentIndex), wssClient));
            }

            if (queueCount > 1) { /* let's prefetch the next track as well */
                context.nextIndex = resolveNextIndex(context.currentIndex, queueCount, false);

                if (context.nextIndex >= 0) {
                    if (trackMetadataCache.containsKey(context.nextIndex)) {
                        context.nextMetadata = trackMetadataCache.get(context.nextIndex);
                    }
                    else {
                        tracks.add(wss.send(getMetadataQuery(context.nextIndex), wssClient));
                    }
                }
            }
        }

        return Observable.concat(tracks);
    }

    private static Observable<Integer> getQueueCount(final PlaybackContext context, final SocketMessage message) {
        context.queueCount = message.getIntOption(Messages.Key.COUNT, 0);
        return Observable.just(context.queueCount);
    }

    private static JSONObject extractTrackFromMessage(final SocketMessage message) {
        final JSONArray data = message.getJsonArrayOption(Messages.Key.DATA);
        if (data.length() > 0) {
            return data.optJSONObject(0);
        }
        return null;
    }

    private void prefetchNextTrackAudio() {
        if (this.context.nextMetadata != null) {
            final String uri = getUri(this.context.nextMetadata);

            if (uri != null) {
                this.context.reset(this.context.nextPlayer);
                this.context.nextPlayer = PlayerWrapper.Companion.newInstance();
                this.context.nextPlayer.setOnStateChangedListener(onNextPlayerStateChanged);
                this.context.nextPlayer.prefetch(uri);
            }
        }
    }

    private void invalidateAndPrefetchNextTrackMetadata() {
        if (context.queueCount > 0) {
            if (context.nextMetadata != null) {
                context.reset(context.nextPlayer);
                context.nextMetadata = null;
                context.nextPlayer = null;
                context.nextIndex = -1;

                if (context.currentPlayer != null) {
                    context.currentPlayer.setNextMediaPlayer(null);
                }
            }

            prefetchNextTrackMetadata();
        }
    }

    private void prefetchNextTrackMetadata() {
        if (context.nextMetadata == null) {
            final QueueParams params = this.params;

            final int nextIndex = resolveNextIndex(context.currentIndex, context.queueCount, false);

            if (trackMetadataCache.containsKey(nextIndex)) {
                context.nextMetadata = trackMetadataCache.get(nextIndex);
                context.nextIndex = nextIndex;
                prefetchNextTrackAudio();
            }
            else if (nextIndex >= 0) {
                final int currentIndex = context.currentIndex;
                this.wss.send(getMetadataQuery(nextIndex), this.wssClient)
                    .observeOn(AndroidSchedulers.mainThread())
                    .subscribeOn(AndroidSchedulers.mainThread())
                    .map(StreamingPlaybackService::extractTrackFromMessage)
                    .doOnNext(track -> {
                        if (params == this.params && context.currentIndex == currentIndex) {
                            if (context.nextMetadata == null) {
                                context.nextIndex = nextIndex;
                                context.nextMetadata = track;
                                prefetchNextTrackAudio();
                            }
                        }
                    })
                    .doOnError(error -> {
                        Log.e(TAG, "failed to prefetch next track!", error);
                    })
                    .subscribe();
            }
        }
    }

    private void loadQueueAndPlay(final QueueParams params, int startIndex) {
        setState(PlaybackState.Buffering);

        cancelScheduledPausedSleep();
        SystemService.wakeup();

        this.pausedByTransientLoss = false;
        this.context.stopPlaybackAndReset();
        final PlaybackContext context = new PlaybackContext();
        this.context = context;
        context.currentIndex = startIndex;

        this.params = params;
        final SocketMessage countMessage = queryFactory.getRequeryMessage();

        this.wss.send(countMessage, this.wssClient)
            .observeOn(AndroidSchedulers.mainThread())
            .subscribeOn(AndroidSchedulers.mainThread())
            .flatMap(response -> getQueueCount(context, response))
            .concatMap(count -> getCurrentAndNextTrackMessages(context, count))
            .map(StreamingPlaybackService::extractTrackFromMessage)
            .doOnNext(track -> {
                if (context.currentMetadata == null) {
                    context.currentMetadata = track;
                }
                else {
                    context.nextMetadata = track;
                }
            })
            .doOnComplete(() -> {
                if (this.params == params && this.context == context) {
                    notifyEventListeners();

                    final String uri = getUri(this.context.currentMetadata);
                    if (uri != null) {
                        this.context.currentPlayer = PlayerWrapper.Companion.newInstance();
                        this.context.currentPlayer.setOnStateChangedListener(onCurrentPlayerStateChanged);
                        this.context.currentPlayer.play(uri);
                    }
                }
            })
            .doOnError(error -> {
                Log.e(TAG, "failed to load track to play!", error);
                setState(PlaybackState.Stopped);
            })
            .subscribe();
    }

    private void cancelScheduledPausedSleep() {
        SystemService.wakeup();
        handler.removeCallbacks(pauseServiceSleepRunnable);
    }

    private void schedulePausedSleep() {
        handler.postDelayed(pauseServiceSleepRunnable, PAUSED_SERVICE_SLEEP_DELAY_MS);
    }

    private void precacheTrackMetadata(final int start, final int count) {
        final QueueParams params = this.params;
        final SocketMessage query = queryFactory.getPageAroundMessage(start, count);

        this.wss.send(query, this.wssClient)
            .observeOn(AndroidSchedulers.mainThread())
            .subscribeOn(AndroidSchedulers.mainThread())
            .doOnNext(response -> {
                if (params == this.params) {
                    final JSONArray data = response.getJsonArrayOption(Messages.Key.DATA);
                    for (int i = 0; i < data.length(); i++) {
                        trackMetadataCache.put(start + i, data.getJSONObject(i));
                    }
                }
            })
            .doOnError(error -> {
                Log.e(TAG, "failed to prefetch track metadata!", error);
            })
            .subscribe();
    }

    private TrackListSlidingWindow.QueryFactory queryFactory = new TrackListSlidingWindow.QueryFactory() {
        @Override
        public SocketMessage getRequeryMessage() {
            if (params != null) {
                if (Strings.notEmpty(params.category) && params.categoryId >= 0) {
                    return SocketMessage.Builder
                        .request(Messages.Request.QueryTracksByCategory)
                        .addOption(Messages.Key.CATEGORY, params.category)
                        .addOption(Messages.Key.ID, params.categoryId)
                        .addOption(Messages.Key.FILTER, params.filter)
                        .addOption(Messages.Key.COUNT_ONLY, true)
                        .build();
                }
                else {
                    return SocketMessage.Builder
                        .request(Messages.Request.QueryTracks)
                        .addOption(Messages.Key.FILTER, params.filter)
                        .addOption(Messages.Key.COUNT_ONLY, true)
                        .build();
                }
            }

            return null;
        }

        @Override
        public SocketMessage getPageAroundMessage(int offset, int limit) {
            if (params != null) {
                if (Strings.notEmpty(params.category) && params.categoryId >= 0) {
                    return SocketMessage.Builder
                        .request(Messages.Request.QueryTracksByCategory)
                        .addOption(Messages.Key.CATEGORY, params.category)
                        .addOption(Messages.Key.ID, params.categoryId)
                        .addOption(Messages.Key.FILTER, params.filter)
                        .addOption(Messages.Key.LIMIT, limit)
                        .addOption(Messages.Key.OFFSET, offset)
                        .build();
                }
                else {
                    return SocketMessage.Builder
                        .request(Messages.Request.QueryTracks)
                        .addOption(Messages.Key.FILTER, params.filter)
                        .addOption(Messages.Key.LIMIT, limit)
                        .addOption(Messages.Key.OFFSET, offset)
                        .build();
                }
            }

            return null;
        }
    };

    private WebSocketService.Client wssClient = new WebSocketService.Client() {
        @Override
        public void onStateChanged(WebSocketService.State newState, WebSocketService.State oldState) {

        }

        @Override
        public void onMessageReceived(SocketMessage message) {

        }

        @Override
        public void onInvalidPassword() {

        }
    };

    private Runnable pauseServiceSleepRunnable = () -> SystemService.sleep();

    private AudioManager.OnAudioFocusChangeListener audioFocusChangeListener = (flag) -> {
        switch (flag) {
            case AudioManager.AUDIOFOCUS_GAIN:
            case AudioManager.AUDIOFOCUS_GAIN_TRANSIENT:
            case AudioManager.AUDIOFOCUS_GAIN_TRANSIENT_EXCLUSIVE:
            case AudioManager.AUDIOFOCUS_GAIN_TRANSIENT_MAY_DUCK:
                PlayerWrapper.Companion.unduck();
                if (pausedByTransientLoss) {
                    pausedByTransientLoss = false;
                    resume();
                }
                break;

            case AudioManager.AUDIOFOCUS_LOSS:
                killAudioFocus();
                pause();
                break;

            case AudioManager.AUDIOFOCUS_LOSS_TRANSIENT:
                switch (getPlaybackState()) {
                    case Playing:
                    case Buffering:
                        pauseTransient();
                        break;
                }
                break;

            case AudioManager.AUDIOFOCUS_LOSS_TRANSIENT_CAN_DUCK:
                PlayerWrapper.Companion.duck();
                break;
        }
    };

    private class SettingsContentObserver extends ContentObserver {
        public SettingsContentObserver() {
            super(new Handler(Looper.getMainLooper()));
        }

        @Override
        public boolean deliverSelfNotifications() {
            return false;
        }

        @Override
        public void onChange(boolean selfChange) {
            super.onChange(selfChange);

            int currentVolume = audioManager.getStreamVolume(AudioManager.STREAM_MUSIC);
            if (currentVolume != lastSystemVolume) {
                lastSystemVolume = currentVolume;
                notifyEventListeners();
            }
        }
    }
}
