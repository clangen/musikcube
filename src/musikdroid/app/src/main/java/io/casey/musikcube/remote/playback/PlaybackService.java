package io.casey.musikcube.remote.playback;

import io.casey.musikcube.remote.ui.model.TrackListSlidingWindow;

public interface PlaybackService {
    interface EventListener {
        void onStateUpdated();
    }

    void connect(final EventListener listener);
    void disconnect(final EventListener listener);

    void playAll();

    void playAll(final int index, final String filter);

    void play(
        final String category,
        final long categoryId,
        final int index,
        final String filter);

    void playAt(final int index);

    void pauseOrResume();

    void pause();
    void resume();
    void prev();
    void next();
    void stop();

    void volumeUp();
    void volumeDown();

    void seekForward();
    void seekBackward();
    void seekTo(double seconds);

    int getQueueCount();
    int getQueuePosition();

    double getVolume();
    double getDuration();
    double getCurrentTime();

    PlaybackState getPlaybackState();

    void toggleShuffle();
    boolean isShuffled();

    void toggleMute();
    boolean isMuted();

    void toggleRepeatMode();
    RepeatMode getRepeatMode();

    TrackListSlidingWindow.QueryFactory getPlaylistQueryFactory();

    String getTrackString(final String key, final String defaultValue);
    long getTrackLong(final String key, final long defaultValue);
}
