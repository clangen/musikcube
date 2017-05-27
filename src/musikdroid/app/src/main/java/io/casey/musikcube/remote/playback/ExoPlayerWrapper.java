package io.casey.musikcube.remote.playback;

import android.content.Context;
import android.content.SharedPreferences;
import android.net.Uri;
import android.util.Base64;

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
import com.google.android.exoplayer2.upstream.HttpDataSource;
import com.google.android.exoplayer2.util.Util;

import java.io.File;
import java.io.IOException;
import java.util.HashMap;
import java.util.Map;

import io.casey.musikcube.remote.Application;
import io.casey.musikcube.remote.util.NetworkUtil;
import io.casey.musikcube.remote.util.Preconditions;
import io.casey.musikcube.remote.websocket.Prefs;
import okhttp3.Cache;
import okhttp3.Interceptor;
import okhttp3.OkHttpClient;
import okhttp3.Request;
import okhttp3.Response;

public class ExoPlayerWrapper extends PlayerWrapper {
    private static OkHttpClient audioStreamHttpClient = null;

    private static final long BYTES_PER_MEGABYTE = 1048576L;
    private static final long BYTES_PER_GIGABYTE = 1073741824L;
    private static final Map<Integer, Long> CACHE_SETTING_TO_BYTES;

    static {
        CACHE_SETTING_TO_BYTES = new HashMap<>();
        CACHE_SETTING_TO_BYTES.put(0, BYTES_PER_MEGABYTE * 32);
        CACHE_SETTING_TO_BYTES.put(1, BYTES_PER_GIGABYTE / 2);
        CACHE_SETTING_TO_BYTES.put(2, BYTES_PER_GIGABYTE);
        CACHE_SETTING_TO_BYTES.put(3, BYTES_PER_GIGABYTE * 2);
        CACHE_SETTING_TO_BYTES.put(4, BYTES_PER_GIGABYTE * 3);
        CACHE_SETTING_TO_BYTES.put(5, BYTES_PER_GIGABYTE * 4);
    }

    private DefaultBandwidthMeter bandwidth;
    private DataSource.Factory datasources;
    private ExtractorsFactory extractors;
    private MediaSource source;
    private SimpleExoPlayer player;
    private boolean prefetch;
    private Context context;
    private SharedPreferences prefs;

    public static void invalidateSettings() {
        synchronized (ExoPlayerWrapper.class) {
            audioStreamHttpClient = null;
        }
    }

    public ExoPlayerWrapper() {
        this.context = Application.getInstance();
        this.prefs = context.getSharedPreferences(Prefs.NAME, Context.MODE_PRIVATE);
        this.bandwidth = new DefaultBandwidthMeter();
        final TrackSelection.Factory trackFactory = new AdaptiveTrackSelection.Factory(bandwidth);
        final TrackSelector trackSelector = new DefaultTrackSelector(trackFactory);
        this.player = ExoPlayerFactory.newSimpleInstance(this.context, trackSelector);
        this.extractors = new DefaultExtractorsFactory();
        this.player.addListener(eventListener);
    }

    private void initHttpClient(final String uri) {
        final Context context = Application.getInstance();

        synchronized (ExoPlayerWrapper.class) {
            if (audioStreamHttpClient == null) {
                final SharedPreferences prefs = ExoPlayerWrapper.this.prefs;
                final File path = new File(context.getExternalCacheDir(), "audio");

                int diskCacheIndex = this.prefs.getInt(
                    Prefs.Key.DISK_CACHE_SIZE_INDEX, Prefs.Default.DISK_CACHE_SIZE_INDEX);

                if (diskCacheIndex < 0 || diskCacheIndex > CACHE_SETTING_TO_BYTES.size()) {
                    diskCacheIndex = 0;
                }

                final OkHttpClient.Builder builder = new OkHttpClient.Builder()
                    .cache(new Cache(path, CACHE_SETTING_TO_BYTES.get(diskCacheIndex)))
                    .addInterceptor((chain) -> {
                        Request request = chain.request();
                        final String userPass = "default:" + prefs.getString(Prefs.Key.PASSWORD, Prefs.Default.PASSWORD);
                        final String encoded = Base64.encodeToString(userPass.getBytes(), Base64.NO_WRAP);
                        request = request.newBuilder().addHeader("Authorization", "Basic " + encoded).build();
                        return chain.proceed(request);
                    });

                if (this.prefs.getBoolean(Prefs.Key.CERT_VALIDATION_DISABLED, Prefs.Default.CERT_VALIDATION_DISABLED)) {
                    NetworkUtil.disableCertificateValidation(builder);
                }

                audioStreamHttpClient = builder.build();
            }
        }

        if (uri.startsWith("http")) {
            this.datasources = new OkHttpDataSourceFactory(
                audioStreamHttpClient,
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
        Preconditions.throwIfNotOnMainThread();

        if (!dead()) {
            initHttpClient(uri);
            this.source = new ExtractorMediaSource(Uri.parse(uri), datasources, extractors, null, null);
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
            initHttpClient(uri);
            this.prefetch = true;
            this.source = new ExtractorMediaSource(Uri.parse(uri), datasources, extractors, null, null);
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
                this.player.setPlayWhenReady(true);
                this.player.prepare(this.source);
                setState(State.Preparing);
                break;
        }

        this.prefetch = false;
    }

    @Override
    public void setPosition(int millis) {
        Preconditions.throwIfNotOnMainThread();

        if (this.player.getPlaybackState() != ExoPlayer.STATE_IDLE) {
            this.player.seekTo(millis);
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

            /* if we're transcoding the size of the response will be inexact, so the player
            will try to pick up the last few bytes and be left with an HTTP 416. if that happens,
            and we're towards the end of the track, just move to the next one */
           if (error.getCause() instanceof HttpDataSource.InvalidResponseCodeException) {
                final HttpDataSource.InvalidResponseCodeException ex
                    = (HttpDataSource.InvalidResponseCodeException) error.getCause();

                if (ex.responseCode == 416) {
                    if (Math.abs(getDuration() - getPosition()) < 2000) {
                        setState(State.Finished);
                        dispose();
                        return;
                    }
                    else {
                        player.setPlayWhenReady(false);
                        setPosition(0);
                    }
                }
            }

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
