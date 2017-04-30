package io.casey.musikcube.remote.playback;

import java.util.HashSet;
import java.util.Set;

import io.casey.musikcube.remote.util.Preconditions;

public abstract class PlayerWrapper {
    private static final String TAG = "MediaPlayerWrapper";

    public enum State {
        Stopped,
        Preparing,
        Prepared,
        Playing,
        Paused,
        Error,
        Finished,
        Killing,
        Disposed
    }

    public interface OnStateChangedListener {
        void onStateChanged(PlayerWrapper mpw, State state);
    }

    private static Set<PlayerWrapper> activePlayers = new HashSet<>();
    private static float globalVolume = 1.0f;
    private static boolean globalMuted = false;

    public static void setGlobalVolume(final float volume) {
        Preconditions.throwIfNotOnMainThread();

        globalVolume = volume;
        for (final PlayerWrapper w : activePlayers) {
            w.updateVolume();
        }
    }

    public static float getGlobalVolume() {
        Preconditions.throwIfNotOnMainThread();
        return globalMuted ? 0 : globalVolume;
    }

    public static void setGlobalMute(final boolean muted) {
        Preconditions.throwIfNotOnMainThread();

        if (PlayerWrapper.globalMuted != muted) {
            PlayerWrapper.globalMuted = muted;

            for (final PlayerWrapper w : activePlayers) {
                w.updateVolume();
            }
        }
    }

    public static PlayerWrapper newInstance() {
        //return new MediaPlayerWrapper();
        return new ExoPlayerWrapper();
    }

    protected static void addActivePlayer(final PlayerWrapper player) {
        Preconditions.throwIfNotOnMainThread();
        activePlayers.add(player);
    }

    protected static void removeActivePlayer(final PlayerWrapper player) {
        Preconditions.throwIfNotOnMainThread();
        activePlayers.remove(player);
    }


    private OnStateChangedListener listener;
    private State state = State.Stopped;

    public abstract void play(final String uri);
    public abstract void prefetch(final String uri);
    public abstract void pause();
    public abstract void resume();
    public abstract void setPosition(int millis);
    public abstract int getPosition();
    public abstract int getDuration();
    public abstract void updateVolume();
    public abstract void setNextMediaPlayer(final PlayerWrapper wrapper);
    public abstract void dispose();

    public void setOnStateChangedListener(OnStateChangedListener listener) {
        Preconditions.throwIfNotOnMainThread();

        this.listener = listener;

        if (listener != null) {
            this.listener.onStateChanged(this, state);
        }
    }

    public final State getState() {
        return this.state;
    }

    protected void setState(final PlayerWrapper.State state) {
        if (this.state != state) {
            this.state = state;
            if (listener != null) {
                this.listener.onStateChanged(this, state);
            }
        }
    }
}
