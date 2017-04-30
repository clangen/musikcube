package io.casey.musikcube.remote.playback;

import android.media.AudioManager;
import android.media.MediaPlayer;
import android.net.Uri;
import android.os.PowerManager;
import android.util.Log;

import java.io.IOException;

import io.casey.musikcube.remote.Application;
import io.casey.musikcube.remote.util.Preconditions;

public class MediaPlayerWrapper extends PlayerWrapper {
    private static final String TAG = "MediaPlayerWrapper";

    private MediaPlayer player = new MediaPlayer();
    private int seekTo;
    private boolean prefetching;

    @Override
    public void play(final String uri) {
        Preconditions.throwIfNotOnMainThread();

        try {
            setState(State.Preparing);
            player.setDataSource(Application.getInstance(), Uri.parse(uri));
            player.setAudioStreamType(AudioManager.STREAM_MUSIC);
            player.setOnPreparedListener(onPrepared);
            player.setOnErrorListener(onError);
            player.setOnCompletionListener(onCompleted);
            player.setWakeMode(Application.getInstance(), PowerManager.PARTIAL_WAKE_LOCK);
            player.prepareAsync();
        }
        catch (IOException e) {
            Log.e(TAG, "setDataSource failed: " + e.toString());
        }
    }

    @Override
    public void prefetch(final String uri) {
        Preconditions.throwIfNotOnMainThread();

        this.prefetching = true;
        play(uri);
    }

    @Override
    public void pause() {
        Preconditions.throwIfNotOnMainThread();

        if (isPreparedOrPlaying()) {
            player.pause();
            setState(State.Paused);
        }
    }

    @Override
    public void setPosition(int millis) {
        Preconditions.throwIfNotOnMainThread();

        if (isPreparedOrPlaying()) {
            this.player.seekTo(millis);
            this.seekTo = 0;
        }
        else {
            this.seekTo = millis;
        }
    }

    @Override
    public int getPosition() {
        Preconditions.throwIfNotOnMainThread();

        if (isPreparedOrPlaying()) {
            return this.player.getCurrentPosition();
        }
        return 0;
    }

    @Override
    public int getDuration() {
        Preconditions.throwIfNotOnMainThread();

        if (isPreparedOrPlaying()) {
            return this.player.getDuration();
        }
        return 0;
    }

    @Override
    public void resume() {
        Preconditions.throwIfNotOnMainThread();

        final State state = getState();
        if (state == State.Prepared || state == State.Paused) {
            player.start();
            setState(State.Playing);
        }
        else {
            prefetching = false;
        }
    }

    @Override
    public void setNextMediaPlayer(final PlayerWrapper wrapper) {
        Preconditions.throwIfNotOnMainThread();

        if (isPreparedOrPlaying()) {
            try {
                this.player.setNextMediaPlayer(wrapper != null ? ((MediaPlayerWrapper) wrapper).player : null);
            }
            catch (IllegalStateException ex) {
                Log.d(TAG, "invalid state for setNextMediaPlayer");
            }
        }
    }

    @Override
    public void updateVolume() {
        Preconditions.throwIfNotOnMainThread();

        final State state = getState();
        if (state != State.Preparing && state != State.Disposed) {
            final float volume = getGlobalVolume();
            player.setVolume(volume, volume);
        }
    }

    private boolean isPreparedOrPlaying() {
        final State state = getState();
        return state == State.Playing || state == State.Prepared;
    }

    public void dispose() {
        Preconditions.throwIfNotOnMainThread();

        removeActivePlayer(this);

        if (getState() != State.Preparing) {
            try {
                this.player.setNextMediaPlayer(null);
            }
            catch (IllegalStateException ex) {
                Log.d(TAG, "failed to setNextMediaPlayer(null)");
            }

            try {
                this.player.stop();
            }
            catch (IllegalStateException ex) {
                Log.d(TAG, "failed to stop()");
            }

            try {
                this.player.reset();
            }
            catch (IllegalStateException ex) {
                Log.d(TAG, "failed to reset()");
            }

            this.player.release();

            setOnStateChangedListener(null);
            setState(State.Disposed);
        }
        else {
            setState(State.Killing);
        }
    }

    private MediaPlayer.OnPreparedListener onPrepared = (mediaPlayer) -> {
        if (this.getState() == State.Killing) {
            dispose();
        }
        else {
            final float volume = getGlobalVolume();
            player.setVolume(volume, volume);

            addActivePlayer(this);

            if (prefetching) {
                setState(State.Prepared);
            }
            else {
                this.player.start();

                if (this.seekTo != 0) {
                    setPosition(this.seekTo);
                }

                setState(State.Playing);
            }

            this.prefetching = false;
        }
    };

    private MediaPlayer.OnErrorListener onError = (player, what, extra) -> {
        setState(State.Error);
        dispose();
        return true;
    };

    private MediaPlayer.OnCompletionListener onCompleted = (mp) -> {
        setState(State.Finished);
        dispose();
    };
}
