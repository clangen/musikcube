package io.casey.musikcube.remote.playback;

import android.content.Context;

import org.json.JSONObject;

import java.util.HashSet;
import java.util.Set;

import io.casey.musikcube.remote.ui.model.TrackListSlidingWindow;
import io.casey.musikcube.remote.websocket.Messages;
import io.casey.musikcube.remote.websocket.SocketMessage;
import io.casey.musikcube.remote.websocket.WebSocketService;

public class RemotePlaybackService implements PlaybackService {
    private WebSocketService wss;

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

    private PlaybackState playbackState = PlaybackState.Unknown;
    private Set<EventListener> listeners = new HashSet<>();
    private RepeatMode repeatMode;
    private boolean shuffled;
    private boolean muted;
    private double volume;
    private int queueCount;
    private int queuePosition;
    private double duration;
    private double currentTime;
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
            }
        }
    }

    @Override
    public synchronized void disconnect(EventListener listener) {
        if (listener != null) {
            listeners.remove(listener);

            if (listeners.size() == 0) {
                wss.removeClient(client);
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
        return currentTime;
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
        playbackState = PlaybackState.Unknown;
        repeatMode = RepeatMode.None;
        shuffled = muted = false;
        volume = 0.0f;
        queueCount = queuePosition = 0;
        duration = currentTime = 0.0f;
        track = new JSONObject();
    }

    private boolean canHandle(SocketMessage socketMessage) {
        if (socketMessage == null) {
            return false;
        }

        final String name = socketMessage.getName();

        return
            name.equals(Messages.Broadcast.PlaybackOverviewChanged.toString()) ||
                name.equals(Messages.Request.GetPlaybackOverview.toString());
    }

    private boolean update(SocketMessage message) {
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
        repeatMode = RepeatMode.from(message.getStringOption(Key.REPEAT_MODE));
        shuffled = message.getBooleanOption(Key.SHUFFLED);
        muted = message.getBooleanOption(Key.MUTED);
        volume = message.getDoubleOption(Key.VOLUME);
        queueCount = message.getIntOption(Key.PLAY_QUEUE_COUNT);
        queuePosition = message.getIntOption(Key.PLAY_QUEUE_POSITION);
        duration = message.getDoubleOption(Key.PLAYING_DURATION);
        currentTime = message.getDoubleOption(Key.PLAYING_CURRENT_TIME);
        track = message.getJsonObjectOption(Key.PLAYING_TRACK, new JSONObject());

        notifyStateUpdated();

        return true;
    }

    private synchronized void notifyStateUpdated() {
        for (final EventListener listener : listeners) {
            listener.onStateUpdated();
        }
    }

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
            if (canHandle(message)) {
                update(message);
            }
        }

        @Override
        public void onInvalidPassword() {

        }
    };
}
