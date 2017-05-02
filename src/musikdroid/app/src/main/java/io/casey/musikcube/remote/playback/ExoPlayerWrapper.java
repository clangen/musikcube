package io.casey.musikcube.remote.playback;

import android.content.Context;
import android.content.SharedPreferences;
import android.net.Uri;

import com.google.android.exoplayer2.ExoPlaybackException;
import com.google.android.exoplayer2.ExoPlayer;
import com.google.android.exoplayer2.ExoPlayerFactory;
import com.google.android.exoplayer2.PlaybackParameters;
import com.google.android.exoplayer2.SimpleExoPlayer;
import com.google.android.exoplayer2.Timeline;
import com.google.android.exoplayer2.ext.okhttp.OkHttpDataSourceFactory;
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

import java.io.File;

import io.casey.musikcube.remote.Application;
import io.casey.musikcube.remote.util.NetworkUtil;
import okhttp3.Cache;
import okhttp3.OkHttpClient;

public class ExoPlayerWrapper extends PlayerWrapper {
    private static OkHttpClient audioStreamClient = null;
    private static boolean certValidationDisabled = false;

    private DefaultBandwidthMeter bandwidth;
    private DataSource.Factory datasources;
    private ExtractorsFactory extractors;
    private MediaSource source;
    private SimpleExoPlayer player;
    private boolean prefetch;
    private Context context;
    private SharedPreferences prefs;

    public ExoPlayerWrapper() {
        this.context = Application.getInstance();
        this.prefs = context.getSharedPreferences("prefs", Context.MODE_PRIVATE);
        this.bandwidth = new DefaultBandwidthMeter();
        final TrackSelection.Factory trackFactory = new AdaptiveTrackSelection.Factory(bandwidth);
        final TrackSelector trackSelector = new DefaultTrackSelector(trackFactory);
        this.player = ExoPlayerFactory.newSimpleInstance(this.context, trackSelector);
        this.extractors = new DefaultExtractorsFactory();
        this.player.addListener(eventListener);

        synchronized (ExoPlayerWrapper.class) {
            final boolean disabled = this.prefs.getBoolean("cert_validation_disabled", false);
            if (disabled != certValidationDisabled) {
                audioStreamClient = null;
                certValidationDisabled = disabled;
            }
        }
    }

    private void initDataSourceFactory(final String uri) {
        final Context context = Application.getInstance();

        synchronized (ExoPlayerWrapper.class) {
            if (audioStreamClient == null) {
                final File path = new File(context.getExternalCacheDir(), "audio");

                OkHttpClient.Builder builder = new OkHttpClient.Builder()
                    .cache(new Cache(path, 1048576 * 256)); /* 256 meg cache */

                if (certValidationDisabled) {
                    NetworkUtil.disableCertificateValidation(builder);
                }

                audioStreamClient = builder.build();
            }
        }

        if (uri.startsWith("http")) {
            this.datasources = new OkHttpDataSourceFactory(
                audioStreamClient,
                Util.getUserAgent(context, "musikdroid"),
                bandwidth);
        }
        else {
            this.datasources = new DefaultDataSourceFactory(
                context, Util.getUserAgent(context, "musikdroid"));
        }
    }

    @Override
    public void play(String uri) {
        initDataSourceFactory(uri);
        this.source = new ExtractorMediaSource(Uri.parse(uri), datasources, extractors, null, null);
        this.player.setPlayWhenReady(true);
        this.player.prepare(this.source);
        addActivePlayer(this);
        setState(State.Preparing);
    }

    @Override
    public void prefetch(String uri) {
        initDataSourceFactory(uri);
        this.prefetch = true;
        this.source = new ExtractorMediaSource(Uri.parse(uri), datasources, extractors, null, null);
        this.player.setPlayWhenReady(false);
        this.player.prepare(this.source);
        addActivePlayer(this);
        setState(State.Preparing);
    }

    @Override
    public void pause() {
        this.prefetch = true;

        if (this.getState() == State.Playing) {
            this.player.setPlayWhenReady(false);
            setState(State.Paused);
        }
    }

    @Override
    public void resume() {
        if (this.getState() == State.Paused || this.getState() == State.Prepared) {
            this.player.setPlayWhenReady(true);
            setState(State.Playing);
        }

        this.prefetch = false;
    }

    @Override
    public void setPosition(int millis) {
        if (this.player.getPlaybackState() != ExoPlayer.STATE_IDLE) {
            this.player.seekTo(millis);
        }
    }

    @Override
    public int getPosition() {
        return (int) this.player.getCurrentPosition();
    }

    @Override
    public int getDuration() {
        return (int) this.player.getDuration();
    }

    @Override
    public void updateVolume() {
        this.player.setVolume(getGlobalVolume());
    }

    @Override
    public void setNextMediaPlayer(PlayerWrapper wrapper) {

    }

    @Override
    public void dispose() {
        if (getState() != State.Disposed) {
            removeActivePlayer(this);
            setState(State.Killing);
            this.player.stop();
            this.player.release();
            setState(State.Disposed);
        }
    }

    @Override
    public void setOnStateChangedListener(OnStateChangedListener listener) {
        super.setOnStateChangedListener(listener);
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
            if (playbackState == ExoPlayer.STATE_READY) {
                setState(State.Prepared);

                player.setVolume(getGlobalVolume());

                if (!prefetch) {
                    player.setPlayWhenReady(true);
                    setState(State.Playing);
                }
                else {
                    setState(State.Paused);
                }
            }
            else if (playbackState == ExoPlayer.STATE_ENDED) {
                setState(State.Finished);
                dispose();
            }
        }

        @Override
        public void onPlayerError(ExoPlaybackException error) {
            switch (getState()) {
                case Preparing:
                case Prepared:
                case Playing:
                case Paused:
                setState(State.Error);
                dispose();
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
