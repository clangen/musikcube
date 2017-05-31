package io.casey.musikcube.remote.playback;

import android.content.Context;
import android.net.Uri;

import com.google.android.exoplayer2.ExoPlaybackException;
import com.google.android.exoplayer2.ExoPlayer;
import com.google.android.exoplayer2.ExoPlayerFactory;
import com.google.android.exoplayer2.PlaybackParameters;
import com.google.android.exoplayer2.SimpleExoPlayer;
import com.google.android.exoplayer2.Timeline;
import com.google.android.exoplayer2.extractor.DefaultExtractorsFactory;
import com.google.android.exoplayer2.extractor.ExtractorsFactory;
import com.google.android.exoplayer2.source.ExtractorMediaSource;
import com.google.android.exoplayer2.source.MediaSource;
import com.google.android.exoplayer2.source.TrackGroupArray;
import com.google.android.exoplayer2.trackselection.AdaptiveTrackSelection;
import com.google.android.exoplayer2.trackselection.DefaultTrackSelector;
import com.google.android.exoplayer2.trackselection.TrackSelection;
import com.google.android.exoplayer2.trackselection.TrackSelectionArray;
import com.google.android.exoplayer2.trackselection.TrackSelector;
import com.google.android.exoplayer2.upstream.DataSource;
import com.google.android.exoplayer2.upstream.DefaultBandwidthMeter;
import com.google.android.exoplayer2.upstream.DefaultDataSourceFactory;
import com.google.android.exoplayer2.util.Util;

import io.casey.musikcube.remote.Application;
import io.casey.musikcube.remote.util.Preconditions;

public class ExoPlayerWrapper extends PlayerWrapper {
    private DataSource.Factory datasources;
    private ExtractorsFactory extractors;
    private MediaSource source;
    private SimpleExoPlayer player;
    private boolean prefetch;
    private Context context;
    private long lastPosition = -1;

    public ExoPlayerWrapper() {
        this.context = Application.getInstance();
        final DefaultBandwidthMeter bandwidth = new DefaultBandwidthMeter();
        final TrackSelection.Factory trackFactory = new AdaptiveTrackSelection.Factory(bandwidth);
        final TrackSelector trackSelector = new DefaultTrackSelector(trackFactory);
        this.player = ExoPlayerFactory.newSimpleInstance(this.context, trackSelector);
        this.extractors = new DefaultExtractorsFactory();
        this.player.addListener(eventListener);
        this.datasources = new DefaultDataSourceFactory(context, Util.getUserAgent(context, "musikdroid"));
    }

    @Override
    public void play(String uri) {
        Preconditions.throwIfNotOnMainThread();

        if (!dead()) {
            final String proxyUri = StreamProxy.getProxyUrl(context, uri);
            this.source = new ExtractorMediaSource(Uri.parse(proxyUri), datasources, extractors, null, null);
            this.player.setPlayWhenReady(true);
            this.player.prepare(this.source);
            addActivePlayer(this);
            setState(State.Preparing);
        }
    }

    @Override
    public void prefetch(String uri) {
        Preconditions.throwIfNotOnMainThread();

        if (!dead()) {
            this.prefetch = true;
            final String proxyUri = StreamProxy.getProxyUrl(context, uri);
            this.source = new ExtractorMediaSource(Uri.parse(proxyUri), datasources, extractors, null, null);
            this.player.setPlayWhenReady(false);
            this.player.prepare(this.source);
            addActivePlayer(this);
            setState(State.Preparing);
        }
    }

    @Override
    public void pause() {
        Preconditions.throwIfNotOnMainThread();

        this.prefetch = true;

        if (this.getState() == State.Playing) {
            this.player.setPlayWhenReady(false);
            setState(State.Paused);
        }
    }

    @Override
    public void resume() {
        Preconditions.throwIfNotOnMainThread();

        switch (this.getState()) {
            case Paused:
            case Prepared:
                this.player.setPlayWhenReady(true);
                setState(State.Playing);
                break;

            case Error:
                this.player.setPlayWhenReady(this.lastPosition == -1);
                this.player.prepare(this.source);
                setState(State.Preparing);
                break;
        }

        this.prefetch = false;
    }

    @Override
    public void setPosition(int millis) {
        Preconditions.throwIfNotOnMainThread();

        this.lastPosition = -1;
        if (this.player.getPlaybackState() != ExoPlayer.STATE_IDLE) {
            if (this.player.isCurrentWindowSeekable()) {
                this.player.seekTo(millis);
            }
        }
    }

    @Override
    public int getPosition() {
        Preconditions.throwIfNotOnMainThread();

        return (int) this.player.getCurrentPosition();
    }

    @Override
    public int getDuration() {
        Preconditions.throwIfNotOnMainThread();

        return (int) this.player.getDuration();
    }

    @Override
    public void updateVolume() {
        Preconditions.throwIfNotOnMainThread();

        this.player.setVolume(getGlobalVolume());
    }

    @Override
    public void setNextMediaPlayer(PlayerWrapper wrapper) {
        Preconditions.throwIfNotOnMainThread();
    }

    @Override
    public void dispose() {
        Preconditions.throwIfNotOnMainThread();

        if (!dead()) {
            setState(State.Killing);
            removeActivePlayer(this);
            if (this.player != null) {
                this.player.setPlayWhenReady(false);
                this.player.removeListener(eventListener);
                this.player.stop();
                this.player.release();
            }
            setState(State.Disposed);
        }
    }

    @Override
    public void setOnStateChangedListener(OnStateChangedListener listener) {
        super.setOnStateChangedListener(listener);
    }

    private boolean dead() {
        final State state = getState();
        return (state == State.Killing || state == State.Disposed);
    }

    private ExoPlayer.EventListener eventListener = new ExoPlayer.EventListener() {
        @Override
        public void onTimelineChanged(Timeline timeline, Object manifest) {
        }

        @Override
        public void onTracksChanged(TrackGroupArray trackGroups, TrackSelectionArray trackSelections) {

        }

        @Override
        public void onLoadingChanged(boolean isLoading) {

        }

        @Override
        public void onPlayerStateChanged(boolean playWhenReady, int playbackState) {
            Preconditions.throwIfNotOnMainThread();

            if (playbackState == ExoPlayer.STATE_READY) {
                if (dead()) {
                    dispose();
                }
                else {
                    setState(State.Prepared);

                    player.setVolume(getGlobalVolume());

                    if (lastPosition != -1) {
                        player.seekTo(lastPosition);
                        lastPosition = -1;
                    }

                    if (!prefetch) {
                        player.setPlayWhenReady(true);
                        setState(State.Playing);
                    }
                    else {
                        setState(State.Paused);
                    }
                }
            }
            else if (playbackState == ExoPlayer.STATE_ENDED) {
                setState(State.Finished);
                dispose();
            }
        }

        @Override
        public void onPlayerError(ExoPlaybackException error) {
            Preconditions.throwIfNotOnMainThread();

            lastPosition = player.getCurrentPosition();

            switch (getState()) {
                case Preparing:
                case Prepared:
                case Playing:
                case Paused:
                    setState(State.Error);
                    break;
            }
        }

        @Override
        public void onPositionDiscontinuity() {

        }

        @Override
        public void onPlaybackParametersChanged(PlaybackParameters playbackParameters) {

        }
    };
}
