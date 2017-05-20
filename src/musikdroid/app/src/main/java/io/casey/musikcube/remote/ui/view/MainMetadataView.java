package io.casey.musikcube.remote.ui.view;

import android.content.Context;
import android.content.SharedPreferences;
import android.graphics.Color;
import android.os.Handler;
import android.support.annotation.AttrRes;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.text.SpannableStringBuilder;
import android.text.TextPaint;
import android.text.method.LinkMovementMethod;
import android.text.style.ClickableSpan;
import android.text.style.ForegroundColorSpan;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewPropertyAnimator;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.TextView;

import com.bumptech.glide.Glide;
import com.bumptech.glide.load.engine.DiskCacheStrategy;
import com.bumptech.glide.load.resource.drawable.GlideDrawable;
import com.bumptech.glide.request.RequestListener;
import com.bumptech.glide.request.target.Target;

import org.json.JSONArray;
import org.json.JSONObject;

import io.casey.musikcube.remote.R;
import io.casey.musikcube.remote.playback.Metadata;
import io.casey.musikcube.remote.playback.PlaybackService;
import io.casey.musikcube.remote.playback.PlaybackState;
import io.casey.musikcube.remote.ui.activity.AlbumBrowseActivity;
import io.casey.musikcube.remote.ui.activity.TrackListActivity;
import io.casey.musikcube.remote.ui.model.AlbumArtModel;
import io.casey.musikcube.remote.ui.util.Views;
import io.casey.musikcube.remote.util.Strings;
import io.casey.musikcube.remote.websocket.Messages;
import io.casey.musikcube.remote.websocket.Prefs;
import io.casey.musikcube.remote.websocket.SocketMessage;
import io.casey.musikcube.remote.websocket.WebSocketService;

public class MainMetadataView extends FrameLayout {
    private Handler handler = new Handler();
    private PlaybackService playback;
    private WebSocketService wss = null;
    private SharedPreferences prefs;

    private boolean isPaused = true;

    private TextView title, artist, album, volume;
    private TextView titleWithArt, artistAndAlbumWithArt, volumeWithArt;
    private View mainTrackMetadataWithAlbumArt, mainTrackMetadataNoAlbumArt;
    private View buffering, bufferingWithArt;
    private ImageView albumArtImageView;

    private ViewPropertyAnimator metadataAnim1, metadataAnim2;
    private enum DisplayMode { Artwork, NoArtwork, Stopped }
    private AlbumArtModel albumArtModel = AlbumArtModel.empty();
    private DisplayMode lastDisplayMode = DisplayMode.Stopped;
    private String lastArtworkUrl = null;

    public MainMetadataView(@NonNull Context context) {
        super(context);
        init();
    }

    public MainMetadataView(@NonNull Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
        init();
    }

    public MainMetadataView(@NonNull Context context, @Nullable AttributeSet attrs, @AttrRes int defStyleAttr) {
        super(context, attrs, defStyleAttr);
        init();
    }

    public void onResume(final PlaybackService playback) {
        this.wss.addClient(wssClient);
        this.playback = playback;
        isPaused = false;
    }

    public void onPause() {
        this.wss.removeClient(wssClient);
        isPaused = true;
    }

    public void clear() {
        albumArtModel = AlbumArtModel.empty();
        updateAlbumArt();
    }

    public void hide() {
        setVisibility(View.GONE);
    }

    public void refresh() {
        setVisibility(View.VISIBLE);

        final boolean buffering = playback.getPlaybackState() == PlaybackState.Buffering;

        final String artist = playback.getTrackString(Metadata.Track.ARTIST, "");
        final String album = playback.getTrackString(Metadata.Track.ALBUM, "");
        final String title = playback.getTrackString(Metadata.Track.TITLE, "");
        final String volume = getString(R.string.status_volume, Math.round(playback.getVolume() * 100));

        this.title.setText(Strings.empty(title) ? getString(buffering ? R.string.buffering : R.string.unknown_title) : title);
        this.artist.setText(Strings.empty(artist) ? getString(buffering ? R.string.buffering : R.string.unknown_artist) : artist);
        this.album.setText(Strings.empty(album) ? getString(buffering ? R.string.buffering : R.string.unknown_album) : album);
        this.volume.setText(volume);

        this.rebindAlbumArtistWithArtTextView();
        this.titleWithArt.setText(Strings.empty(title) ? getString(buffering ? R.string.buffering : R.string.unknown_title) : title);
        this.volumeWithArt.setText(volume);

        this.buffering.setVisibility(buffering ? View.VISIBLE : View.GONE);
        this.bufferingWithArt.setVisibility(buffering ? View.VISIBLE : View.GONE);

        boolean albumArtEnabledInSettings = this.prefs.getBoolean(
            Prefs.Key.ALBUM_ART_ENABLED, Prefs.Default.ALBUM_ART_ENABLED);

        if (!albumArtEnabledInSettings || Strings.empty(artist) || Strings.empty(album)) {
            this.albumArtModel = AlbumArtModel.empty();
            setMetadataDisplayMode(DisplayMode.NoArtwork);
        }
        else {
            if (!this.albumArtModel.is(artist, album)) {
                this.albumArtModel.destroy();

                this.albumArtModel = new AlbumArtModel(
                    title, artist, album, AlbumArtModel.Size.Mega, albumArtRetrieved);
            }
            updateAlbumArt();
        }
    }

    private String getString(int resId) {
        return getContext().getString(resId);
    }

    private String getString(int resId, Object... args) {
        return getContext().getString(resId, args);
    }

    private void setMetadataDisplayMode(DisplayMode mode) {
        lastDisplayMode = mode;

        if (metadataAnim1 != null) {
            metadataAnim1.cancel();
            metadataAnim2.cancel();
        }

        if (mode == DisplayMode.Stopped) {
            albumArtImageView.setImageDrawable(null);
            metadataAnim1 = Views.animateAlpha(mainTrackMetadataWithAlbumArt, 0.0f);
            metadataAnim2 = Views.animateAlpha(mainTrackMetadataNoAlbumArt, 0.0f);
        }
        else if (mode == DisplayMode.Artwork) {
            metadataAnim1 = Views.animateAlpha(mainTrackMetadataWithAlbumArt, 1.0f);
            metadataAnim2 = Views.animateAlpha(mainTrackMetadataNoAlbumArt, 0.0f);
        }
        else {
            albumArtImageView.setImageDrawable(null);
            metadataAnim1 = Views.animateAlpha(mainTrackMetadataWithAlbumArt, 0.0f);
            metadataAnim2 = Views.animateAlpha(mainTrackMetadataNoAlbumArt, 1.0f);
        }
    }

    private void rebindAlbumArtistWithArtTextView() {
        final boolean buffering = playback.getPlaybackState() == PlaybackState.Buffering;

        final String artist = playback.getTrackString(
            Metadata.Track.ARTIST, getString(buffering ? R.string.buffering : R.string.unknown_artist));

        final String album = playback.getTrackString(
            Metadata.Track.ALBUM, getString(buffering ? R.string.buffering : R.string.unknown_album));

        final ForegroundColorSpan albumColor =
            new ForegroundColorSpan(getResources().getColor(R.color.theme_orange));

        final ForegroundColorSpan artistColor =
            new ForegroundColorSpan(getResources().getColor(R.color.theme_yellow));

        final SpannableStringBuilder builder =
            new SpannableStringBuilder().append(album).append(" - ").append(artist);

        final ClickableSpan albumClickable = new ClickableSpan() {
            @Override
            public void onClick(View widget) {
                navigateToCurrentAlbum();
            }

            @Override
            public void updateDrawState(TextPaint ds) {
            }
        };

        final ClickableSpan artistClickable = new ClickableSpan() {
            @Override
            public void onClick(View widget) {
                navigateToCurrentArtist();
            }

            @Override
            public void updateDrawState(TextPaint ds) {
            }
        };

        int artistOffset = album.length() + 3;

        builder.setSpan(albumColor, 0, album.length(), 0);
        builder.setSpan(albumClickable, 0, album.length(), 0);
        builder.setSpan(artistColor, artistOffset, artistOffset + artist.length(), 0);
        builder.setSpan(artistClickable, artistOffset, artistOffset + artist.length(), 0);

        this.artistAndAlbumWithArt.setMovementMethod(LinkMovementMethod.getInstance());
        this.artistAndAlbumWithArt.setHighlightColor(Color.TRANSPARENT);
        this.artistAndAlbumWithArt.setText(builder);
    }

    private void updateAlbumArt() {
        if (playback.getPlaybackState() == PlaybackState.Stopped) {
            setMetadataDisplayMode(DisplayMode.NoArtwork);
        }

        final String url = albumArtModel.getUrl();

        if (Strings.empty(url)) {
            this.lastArtworkUrl = null;
            albumArtModel.fetch();
            setMetadataDisplayMode(DisplayMode.NoArtwork);
        }
        else if (!url.equals(lastArtworkUrl) || lastDisplayMode == DisplayMode.Stopped) {
            final int loadId = albumArtModel.getId();
            this.lastArtworkUrl = url;

            Glide.with(getContext())
                .load(url)
                .diskCacheStrategy(DiskCacheStrategy.ALL)
                .listener(new RequestListener<String, GlideDrawable>() {
                    @Override
                    public boolean onException(Exception e, String model, Target<GlideDrawable> target, boolean first) {
                        setMetadataDisplayMode(DisplayMode.NoArtwork);
                        lastArtworkUrl = null;
                        return false;
                    }

                    @Override
                    public boolean onResourceReady(GlideDrawable resource, String model, Target<GlideDrawable> target, boolean memory, boolean first) {
                        if (!isPaused) {
                            preloadNextImage();
                        }

                        /* if the loadId doesn't match the current id, then the image was
                        loaded for a different song. throw it away. */
                        if (albumArtModel.getId() != loadId) {
                            return true;
                        }
                        else {
                            setMetadataDisplayMode(DisplayMode.Artwork);
                            return false;
                        }
                    }
                })
                .into(albumArtImageView);
        }
        else {
            setMetadataDisplayMode(lastDisplayMode);
        }
    }


    private void preloadNextImage() {
        final SocketMessage request = SocketMessage.Builder
            .request(Messages.Request.QueryPlayQueueTracks)
            .addOption(Messages.Key.OFFSET, this.playback.getQueuePosition() + 1)
            .addOption(Messages.Key.LIMIT, 1)
            .build();

        this.wss.send(request, wssClient, (response) -> {
            final JSONArray data = response.getJsonArrayOption(Messages.Key.DATA, new JSONArray());
            if (data.length() > 0) {
                JSONObject track = data.optJSONObject(0);
                final String artist = track.optString(Metadata.Track.ARTIST, "");
                final String album = track.optString(Metadata.Track.ALBUM, "");

                if (!albumArtModel.is(artist, album)) {
                    new AlbumArtModel("", artist, album, AlbumArtModel.Size.Mega, (info, url) -> {
                        int width = albumArtImageView.getWidth();
                        int height = albumArtImageView.getHeight();
                        Glide.with(getContext()).load(url).downloadOnly(width, height);
                    }).fetch();
                }
            }
        });
    }

    private void init() {
        this.prefs = getContext().getSharedPreferences(Prefs.NAME, Context.MODE_PRIVATE);
        this.wss = WebSocketService.getInstance(getContext());

        final View child = LayoutInflater.from(getContext())
            .inflate(R.layout.main_metadata, this, false);

        addView(child, ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT);

        this.title = (TextView) findViewById(R.id.track_title);
        this.artist = (TextView) findViewById(R.id.track_artist);
        this.album = (TextView) findViewById(R.id.track_album);
        this.volume = (TextView) findViewById(R.id.volume);
        this.buffering = findViewById(R.id.buffering);

        this.titleWithArt = (TextView) findViewById(R.id.with_art_track_title);
        this.artistAndAlbumWithArt = (TextView) findViewById(R.id.with_art_artist_and_album);
        this.volumeWithArt = (TextView) findViewById(R.id.with_art_volume);
        this.bufferingWithArt = findViewById(R.id.with_art_buffering);

        this.mainTrackMetadataWithAlbumArt = findViewById(R.id.main_track_metadata_with_art);
        this.mainTrackMetadataNoAlbumArt = findViewById(R.id.main_track_metadata_without_art);
        this.albumArtImageView = (ImageView) findViewById(R.id.album_art);

        /* these will get faded in as appropriate */
        this.mainTrackMetadataNoAlbumArt.setAlpha(0.0f);
        this.mainTrackMetadataWithAlbumArt.setAlpha(0.0f);

        this.album.setOnClickListener((view) -> navigateToCurrentAlbum());
        this.artist.setOnClickListener((view) -> navigateToCurrentArtist());
    }

    private void navigateToCurrentArtist() {
        final long artistId = playback.getTrackLong(Metadata.Track.ARTIST_ID, -1);
        if (artistId != -1) {
            final String artistName = playback.getTrackString(Metadata.Track.ARTIST, "");
            getContext().startActivity(AlbumBrowseActivity.getStartIntent(
                getContext(), Messages.Category.ARTIST, artistId, artistName));
        }
    }

    private void navigateToCurrentAlbum() {
        final long albumId = playback.getTrackLong(Metadata.Track.ALBUM_ID, -1);
        if (albumId != -1) {
            final String albumName = playback.getTrackString(Metadata.Track.ALBUM, "");
            getContext().startActivity(TrackListActivity.getStartIntent(
                getContext(), Messages.Category.ALBUM, albumId, albumName));
        }
    }

    private WebSocketService.Client wssClient = new WebSocketService.Client() {
        @Override
        public void onStateChanged(WebSocketService.State newState, WebSocketService.State oldState) {
        }

        @Override
        public void onMessageReceived(SocketMessage message) {
        }

        @Override
        public void onInvalidPassword() {
        }
    };

    private AlbumArtModel.AlbumArtCallback albumArtRetrieved = (model, url) -> {
        handler.post(() -> {
            if (model == albumArtModel) {
                if (Strings.empty(model.getUrl())) {
                    setMetadataDisplayMode(DisplayMode.NoArtwork);
                }
                else {
                    updateAlbumArt();
                }
            }
        });
    };
}
