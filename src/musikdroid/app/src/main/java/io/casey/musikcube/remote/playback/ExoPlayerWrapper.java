package io.casey.musikcube.remote.playback;

import android.content.Context;
import android.content.SharedPreferences;
import android.net.Uri;
import android.util.Base64;

import com.danikula.videocache.CacheListener;
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
import io.casey.musikcube.remote.util.Preconditions;
import io.casey.musikcube.remote.websocket.Prefs;
import okhttp3.Cache;
import okhttp3.OkHttpClient;
import okhttp3.Request;

public class ExoPlayerWrapper extends PlayerWrapper {
    private static OkHttpClient audioStreamHttpClient = null;

    private final SharedPreferences prefs;
    private DataSource.Factory datasources;
    private ExtractorsFactory extractors;
    private MediaSource source;
    private SimpleExoPlayer player;
    private boolean prefetch;
    private Context context;
    private long lastPosition = -1;
    private int percentAvailable = 0;
    private String originalUri, resolvedUri;
    private boolean transcoding;

    private void initHttpClient(final String uri) {
        if (StreamProxy.ENABLED) {
            return;
        }

        synchronized (ExoPlayerWrapper.class) {
            if (audioStreamHttpClient == null) {
                final File path = new File(context.getExternalCacheDir(), "audio");

                int diskCacheIndex = prefs.getInt(
                    Prefs.Key.DISK_CACHE_SIZE_INDEX, Prefs.Default.DISK_CACHE_SIZE_INDEX);

                if (diskCacheIndex < 0 || diskCacheIndex > StreamProxy.CACHE_SETTING_TO_BYTES.size()) {
                    diskCacheIndex = 0;
                }

                final OkHttpClient.Builder builder = new OkHttpClient.Builder()
                    .cache(new Cache(path, StreamProxy.CACHE_SETTING_TO_BYTES.get(diskCacheIndex)))
                    .addInterceptor((chain) -> {
                        Request request = chain.request();
                        final String userPass = "default:" + prefs.getString(Prefs.Key.PASSWORD, Prefs.Default.PASSWORD);
                        final String encoded = Base64.encodeToString(userPass.getBytes(), Base64.NO_WRAP);
                        request = request.newBuilder().addHeader("Authorization", "Basic " + encoded).build();
                        return chain.proceed(request);
                    });

                if (prefs.getBoolean(Prefs.Key.CERT_VALIDATION_DISABLED, Prefs.Default.CERT_VALIDATION_DISABLED)) {
                    NetworkUtil.disableCertificateValidation(builder);
                }

                audioStreamHttpClient = builder.build();
            }
        }

        if (uri.startsWith("http")) {
            this.datasources = new OkHttpDataSourceFactory(
                audioStreamHttpClient,
                Util.getUserAgent(context, "musikdroid"),
                new DefaultBandwidthMeter());
        }
        else {
            this.datasources = new DefaultDataSourceFactory(
                context, Util.getUserAgent(context, "musikdroid"));
        }
    }

    public ExoPlayerWrapper() {
        this.context = Application.getInstance();
        final DefaultBandwidthMeter bandwidth = new DefaultBandwidthMeter();
        final TrackSelection.Factory trackFactory = new AdaptiveTrackSelection.Factory(bandwidth);
        final TrackSelector trackSelector = new DefaultTrackSelector(trackFactory);
        this.player = ExoPlayerFactory.newSimpleInstance(this.context, trackSelector);
        this.extractors = new DefaultExtractorsFactory();
        this.player.addListener(eventListener);
        this.datasources = new DefaultDataSourceFactory(context, Util.getUserAgent(context, "musikdroid"));
        this.prefs = Application.getInstance().getSharedPreferences(Prefs.NAME, Context.MODE_PRIVATE);
        this.transcoding = this.prefs.getInt(Prefs.Key.TRANSCODER_BITRATE_INDEX, 0) != 0;
    }

    @Override
    public void play(String uri) {
        Preconditions.throwIfNotOnMainThread();

        if (!dead()) {
            initHttpClient(uri);
            this.originalUri = uri;
            this.resolvedUri = StreamProxy.getProxyUrl(context, uri);
            addCacheListener();
            this.source = new ExtractorMediaSource(Uri.parse(resolvedUri), datasources, extractors, null, null);
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
            this.originalUri = uri;
            this.prefetch = true;
            this.resolvedUri = StreamProxy.getProxyUrl(context, uri);
            addCacheListener();
            this.source = new ExtractorMediaSource(Uri.parse(resolvedUri), datasources, extractors, null, null);
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
                long offset = millis;

                /* if we're transcoding we don't want to seek arbitrarily because it may put
                a lot of pressure on the backend. just allow seeking up to what we currently
                have buffered! */
                if (transcoding && percentAvailable != 100) {
                    /* give ourselves 2% wiggle room! */
                    float percent = (float) Math.max(0, percentAvailable - 2) / 100.0f;
                    long totalMs = this.player.getDuration();
                    long available = (long) ((float) totalMs * percent);
                    offset = Math.min(millis, available);
                }

                this.player.seekTo(offset);
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
    public int getBufferedPercent() {
        return transcoding ? percentAvailable : 100;
    }

    @Override
    public void dispose() {
        Preconditions.throwIfNotOnMainThread();

        if (!dead()) {
            setState(State.Killing);
            removeActivePlayer(this);
            removeCacheListener();
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

    private void addCacheListener() {
        if (StreamProxy.ENABLED) {
            if (StreamProxy.isCached(this.originalUri)) {
                percentAvailable = 100;
            }
            else {
                StreamProxy.registerCacheListener(this.cacheListener, this.originalUri);
            }
        }
        else {
            percentAvailable = 100;
        }
    }

    private void removeCacheListener() {
        if (StreamProxy.ENABLED) {
            StreamProxy.unregisterCacheListener(this.cacheListener);
        }
    }

    private CacheListener cacheListener = (file, uri, percent) -> {
        //Log.e("CLCLCL", String.format("%d", percent));
        percentAvailable = percent;
    };

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

            if (playbackState == ExoPlayer.STATE_BUFFERING) {
                setState(State.Buffering);
            }
            else if (playbackState == ExoPlayer.STATE_READY) {
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
