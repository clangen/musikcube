package io.casey.musikcube.remote.playback;

import android.content.Context;
import android.os.Handler;

import org.json.JSONObject;

import java.util.HashSet;
import java.util.Set;

import io.casey.musikcube.remote.ui.model.TrackListSlidingWindow;
import io.casey.musikcube.remote.websocket.Messages;
import io.casey.musikcube.remote.websocket.SocketMessage;
import io.casey.musikcube.remote.websocket.WebSocketService;

public class RemotePlaybackService implements PlaybackService {
    private static final double NANOSECONDS_PER_SECOND = 1000000000.0;
    private static final long SYNC_TIME_INTERVAL_MS = 5000L;

    private interface Key {
        String STATE = "state";
        String REPEAT_MODE = "repeat_mode";
        String VOLUME = "volume";
        String SHUFFLED = "shuffled";
        String MUTED = "muted";
        String PLAY_QUEUE_COUNT = "track_count";
        String PLAY_QUEUE_POSITION = "play_queue_position";
        String PLAYING_DURATION = "playing_duration";
        String PLAYING_CURRENT_TIME = "playing_current_time";
        String PLAYING_TRACK = "playing_track";
    }

    /**
     * an annoying little class that maintains and updates state that estimates
     * the currently playing time. remember, here we're a remote control, so we
     * don't know the exact position of the play head! we update every 5 seconds
     * and estimate.
     */
    private static class EstimatedPosition {
        double lastTime = 0.0, pauseTime = 0.0;
        long trackId = -1;
        long queryTime = 0;

        double get(final JSONObject track) {
            if (track != null && track.optLong(Metadata.Track.ID, -1L) == trackId && trackId != -1) {
                if (pauseTime != 0) {
                    return pauseTime;
                }
                else {
                    return estimatedTime();
                }
            }
            return 0;
        }

        void update(final SocketMessage message) {
            queryTime = System.nanoTime();
            lastTime = message.getDoubleOption(Messages.Key.PLAYING_CURRENT_TIME, 0);
            trackId = message.getLongOption(Messages.Key.ID, -1);
        }

        void pause() {
            pauseTime = estimatedTime();
        }

        void resume() {
            lastTime = pauseTime;
            queryTime = System.nanoTime();
            pauseTime = 0.0;
        }

        void update(final double time, final long id) {
            queryTime = System.nanoTime();
            lastTime = time;
            trackId = id;

            if (pauseTime != 0) {
                pauseTime = time; /* ehh... */
            }
        }

        void reset() {
            lastTime = pauseTime = 0.0;
            queryTime = System.nanoTime();
            trackId = -1;
        }

        double estimatedTime() {
            final long diff = System.nanoTime() - queryTime;
            final double seconds = (double) diff / NANOSECONDS_PER_SECOND;
            return lastTime + seconds;
        }
    }

    private Handler handler = new Handler();
    private WebSocketService wss;
    private EstimatedPosition currentTime = new EstimatedPosition();
    private PlaybackState playbackState = PlaybackState.Stopped;
    private Set<EventListener> listeners = new HashSet<>();
    private RepeatMode repeatMode;
    private boolean shuffled;
    private boolean muted;
    private double volume;
    private int queueCount;
    private int queuePosition;
    private double duration;
    private JSONObject track = new JSONObject();

    public RemotePlaybackService(final Context context) {
        wss = WebSocketService.getInstance(context.getApplicationContext());
        reset();
    }

    @Override
    public void playAll() {
        playAll(0, "");
    }

    @Override
    public void playAll(final int index, final String filter) {
        wss.send(SocketMessage.Builder
            .request(Messages.Request.PlayAllTracks)
            .addOption(Messages.Key.INDEX, index)
            .addOption(Messages.Key.FILTER, filter)
            .build());
    }

    @Override
    public void play(String category, long categoryId, int index, String filter) {
        wss.send(SocketMessage.Builder
            .request(Messages.Request.PlayTracksByCategory)
            .addOption(Messages.Key.CATEGORY, category)
            .addOption(Messages.Key.ID, categoryId)
            .addOption(Messages.Key.INDEX, index)
            .addOption(Messages.Key.FILTER, filter)
            .build());
    }

    @Override
    public void prev() {
        wss.send(SocketMessage.Builder.request(Messages.Request.Previous).build());
    }

    @Override
    public void pauseOrResume() {
        wss.send(SocketMessage.Builder.request(
            Messages.Request.PauseOrResume).build());
    }

    @Override
    public void pause() {
        if (playbackState != PlaybackState.Paused) {
            pauseOrResume();
        }
    }

    @Override
    public void resume() {
        if (playbackState != PlaybackState.Playing) {
            pauseOrResume();
        }
    }

    @Override
    public void stop() {
        /* nothing for now */
    }

    @Override
    public void next() {
        wss.send(SocketMessage.Builder.request(Messages.Request.Next).build());
    }

    @Override
    public void playAt(int index) {
        wss.send(SocketMessage
            .Builder.request(Messages.Request.PlayAtIndex)
            .addOption(Messages.Key.INDEX, index)
            .build());
    }

    @Override
    public void volumeUp() {
        wss.send(SocketMessage.Builder
            .request(Messages.Request.SetVolume)
            .addOption(Messages.Key.RELATIVE, Messages.Value.UP)
            .build());
    }

    @Override
    public void volumeDown() {
        wss.send(SocketMessage.Builder
            .request(Messages.Request.SetVolume)
            .addOption(Messages.Key.RELATIVE, Messages.Value.DOWN)
            .build());
    }

    @Override
    public void seekForward() {
        wss.send(SocketMessage.Builder
            .request(Messages.Request.SeekRelative)
            .addOption(Messages.Key.DELTA, 5.0f).build());
    }

    @Override
    public void seekBackward() {
        wss.send(SocketMessage.Builder
            .request(Messages.Request.SeekRelative)
            .addOption(Messages.Key.DELTA, -5.0f).build());
    }

    @Override
    public void seekTo(double seconds) {
        wss.send(SocketMessage.Builder
            .request(Messages.Request.SeekTo)
            .addOption(Messages.Key.POSITION, seconds).build());

        currentTime.update(seconds, currentTime.trackId);
    }

    @Override
    public int getQueueCount() {
        return queueCount;
    }

    @Override
    public int getQueuePosition() {
        return queuePosition;
    }

    @Override
    public PlaybackState getPlaybackState() {
        return playbackState;
    }

    @Override
    public RepeatMode getRepeatMode() {
        return repeatMode;
    }

    @Override
    public synchronized void connect(EventListener listener) {
        if (listener != null) {
            listeners.add(listener);

            if (listeners.size() == 1) {
                wss.addClient(client);
                scheduleTimeSyncMessage();
            }
        }
    }

    @Override
    public synchronized void disconnect(EventListener listener) {
        if (listener != null) {
            listeners.remove(listener);

            if (listeners.size() == 0) {
                wss.removeClient(client);
                handler.removeCallbacks(syncTimeRunnable);
            }
        }
    }

    @Override
    public void toggleShuffle() {
        wss.send(SocketMessage.Builder
            .request(Messages.Request.ToggleShuffle).build());
    }

    @Override
    public void toggleMute() {
        wss.send(SocketMessage.Builder
            .request(Messages.Request.ToggleMute).build());
    }

    @Override
    public void toggleRepeatMode() {
        wss.send(SocketMessage.Builder
            .request(Messages.Request.ToggleRepeat)
            .build());
    }

    @Override
    public boolean isShuffled() {
        return shuffled;
    }

    @Override
    public boolean isMuted() {
        return muted;
    }

    @Override
    public double getVolume() {
        return volume;
    }

    @Override
    public double getDuration() {
        return duration;
    }

    @Override
    public double getCurrentTime() {
        return currentTime.get(track);
    }

    @Override
    public double getBufferedTime() {
        return getDuration();
    }

    @Override
    public String getTrackString(String key, String defaultValue) {
        if (track.has(key)) {
            return track.optString(key, defaultValue);
        }
        return defaultValue;
    }

    @Override
    public long getTrackLong(String key, long defaultValue) {
        if (track.has(key)) {
            return track.optLong(key, defaultValue);
        }
        return defaultValue;
    }

    @Override
    public TrackListSlidingWindow.QueryFactory getPlaylistQueryFactory() {
        return queryFactory;
    }

    private void reset() {
        playbackState = PlaybackState.Stopped;
        repeatMode = RepeatMode.None;
        shuffled = muted = false;
        volume = 0.0f;
        queueCount = queuePosition = 0;
        track = new JSONObject();
        currentTime.reset();
    }

    private boolean isPlaybackOverviewMessage(SocketMessage socketMessage) {
        if (socketMessage == null) {
            return false;
        }

        final String name = socketMessage.getName();

        return
            name.equals(Messages.Broadcast.PlaybackOverviewChanged.toString()) ||
            name.equals(Messages.Request.GetPlaybackOverview.toString());
    }

    private boolean updatePlaybackOverview(SocketMessage message) {
        if (message == null) {
            reset();
            return false;
        }

        final String name = message.getName();

        if (!name.equals(Messages.Broadcast.PlaybackOverviewChanged.toString()) &&
            !name.equals(Messages.Request.GetPlaybackOverview.toString()))
        {
            throw new IllegalArgumentException("invalid message!");
        }

        playbackState = PlaybackState.from(message.getStringOption(Key.STATE));

        switch (playbackState) {
            case Paused:
                currentTime.pause();
                break;
            case Playing:
                currentTime.resume();
                scheduleTimeSyncMessage();
                break;
        }

        repeatMode = RepeatMode.from(message.getStringOption(Key.REPEAT_MODE));
        shuffled = message.getBooleanOption(Key.SHUFFLED);
        muted = message.getBooleanOption(Key.MUTED);
        volume = message.getDoubleOption(Key.VOLUME);
        queueCount = message.getIntOption(Key.PLAY_QUEUE_COUNT);
        queuePosition = message.getIntOption(Key.PLAY_QUEUE_POSITION);
        duration = message.getDoubleOption(Key.PLAYING_DURATION);
        track = message.getJsonObjectOption(Key.PLAYING_TRACK, new JSONObject());

        if (track != null) {
            currentTime.update(
                message.getDoubleOption(Key.PLAYING_CURRENT_TIME, -1),
                track.optLong(Metadata.Track.ID, -1));
        }

        notifyStateUpdated();

        return true;
    }

    private synchronized void notifyStateUpdated() {
        for (final EventListener listener : listeners) {
            listener.onStateUpdated();
        }
    }

    private void scheduleTimeSyncMessage() {
        handler.removeCallbacks(syncTimeRunnable);

        if (getPlaybackState() == PlaybackState.Playing) {
            handler.postDelayed(syncTimeRunnable, SYNC_TIME_INTERVAL_MS);
        }
    }

    private final Runnable syncTimeRunnable = () -> {
        if (this.wss.hasClient(this.client)) {
            this.wss.send(SocketMessage.Builder
                .request(Messages.Request.GetCurrentTime).build());
        }
    };

    private final TrackListSlidingWindow.QueryFactory queryFactory
        = new TrackListSlidingWindow.QueryFactory() {
        @Override
        public SocketMessage getRequeryMessage() {
            return SocketMessage.Builder
                .request(Messages.Request.QueryPlayQueueTracks)
                .addOption(Messages.Key.COUNT_ONLY, true)
                .build();
        }

        @Override
        public SocketMessage getPageAroundMessage(int offset, int limit) {
            return SocketMessage.Builder
                .request(Messages.Request.QueryPlayQueueTracks)
                .addOption(Messages.Key.OFFSET, offset)
                .addOption(Messages.Key.LIMIT, limit)
                .build();
        }

        @Override
        public boolean connectionRequired() {
            return true;
        }
    };

    private WebSocketService.Client client = new WebSocketService.Client() {
        @Override
        public void onStateChanged(WebSocketService.State newState, WebSocketService.State oldState) {
            if (newState == WebSocketService.State.Connected) {
                wss.send(SocketMessage.Builder.request(
                    Messages.Request.GetPlaybackOverview.toString()).build());
            }
            else if (newState == WebSocketService.State.Disconnected) {
                reset();
                notifyStateUpdated();
            }
        }

        @Override
        public void onMessageReceived(SocketMessage message) {
            if (isPlaybackOverviewMessage(message)) {
                updatePlaybackOverview(message);
            }
            else if (Messages.Request.GetCurrentTime.is(message.getName())) {
                currentTime.update(message);
                scheduleTimeSyncMessage();
            }
        }

        @Override
        public void onInvalidPassword() {

        }
    };
}
