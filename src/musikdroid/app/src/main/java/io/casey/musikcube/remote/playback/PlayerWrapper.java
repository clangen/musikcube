package io.casey.musikcube.remote.playback;

import android.util.Log;

import java.util.HashSet;
import java.util.Set;

import io.casey.musikcube.remote.util.Preconditions;

public abstract class PlayerWrapper {
    private static final String TAG = "MediaPlayerWrapper";
    private static final float DUCK_COEF = 0.2f; /* volume = 20% when ducked */
    private static final float DUCK_NONE = -1.0f;

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
    private static float preDuckGlobalVolume = DUCK_NONE;

    public static void duck() {
        Preconditions.throwIfNotOnMainThread();

        if (preDuckGlobalVolume == DUCK_NONE) {
            final float lastVolume = globalVolume;
            setGlobalVolume(globalVolume * DUCK_COEF);
            preDuckGlobalVolume = lastVolume;
        }
    }

    public static void unduck() {
        Preconditions.throwIfNotOnMainThread();

        if (preDuckGlobalVolume != DUCK_NONE) {
            final float temp = preDuckGlobalVolume;
            preDuckGlobalVolume = DUCK_NONE;
            setGlobalVolume(temp);
        }
    }

    public static void setGlobalVolume(float volume) {
        Preconditions.throwIfNotOnMainThread();

        if (preDuckGlobalVolume != DUCK_NONE) {
            preDuckGlobalVolume = volume;
            volume = volume * DUCK_COEF;
        }

        if (volume != globalVolume) {
            globalVolume = volume;
            for (final PlayerWrapper w : activePlayers) {
                w.updateVolume();
            }
        }
    }

    public static float getGlobalVolume() {
        Preconditions.throwIfNotOnMainThread();

        if (globalMuted) {
            return 0;
        }

        return (preDuckGlobalVolume == DUCK_NONE) ? globalVolume : preDuckGlobalVolume;
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
