package io.casey.musikcube.remote;

import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.graphics.Color;
import android.os.Bundle;
import android.os.Handler;
import android.text.SpannableStringBuilder;
import android.text.TextPaint;
import android.text.method.LinkMovementMethod;
import android.text.style.ClickableSpan;
import android.text.style.ForegroundColorSpan;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewPropertyAnimator;
import android.widget.CheckBox;
import android.widget.CompoundButton;
import android.widget.ImageView;
import android.widget.TextView;

import com.bumptech.glide.Glide;
import com.bumptech.glide.load.resource.drawable.GlideDrawable;
import com.bumptech.glide.request.RequestListener;
import com.bumptech.glide.request.target.Target;

import org.json.JSONArray;
import org.json.JSONObject;

import java.util.HashMap;
import java.util.Map;

import io.casey.musikcube.remote.playback.Metadata;
import io.casey.musikcube.remote.playback.PlaybackService;
import io.casey.musikcube.remote.playback.PlaybackState;
import io.casey.musikcube.remote.playback.RepeatMode;
import io.casey.musikcube.remote.ui.activity.AlbumBrowseActivity;
import io.casey.musikcube.remote.ui.activity.CategoryBrowseActivity;
import io.casey.musikcube.remote.ui.activity.PlayQueueActivity;
import io.casey.musikcube.remote.ui.activity.SettingsActivity;
import io.casey.musikcube.remote.ui.activity.TrackListActivity;
import io.casey.musikcube.remote.ui.activity.WebSocketActivityBase;
import io.casey.musikcube.remote.ui.fragment.InvalidPasswordDialogFragment;
import io.casey.musikcube.remote.ui.model.AlbumArtModel;
import io.casey.musikcube.remote.ui.util.Views;
import io.casey.musikcube.remote.ui.view.LongPressTextView;
import io.casey.musikcube.remote.util.Strings;
import io.casey.musikcube.remote.websocket.Messages;
import io.casey.musikcube.remote.websocket.SocketMessage;
import io.casey.musikcube.remote.websocket.WebSocketService;

public class MainActivity extends WebSocketActivityBase {
    private static Map<RepeatMode, Integer> REPEAT_TO_STRING_ID;

    private WebSocketService wss = null;

    private SharedPreferences prefs;
    private TextView title, artist, album, playPause, volume;
    private TextView titleWithArt, artistAndAlbumWithArt, volumeWithArt;
    private TextView notPlayingOrDisconnected;
    private View connected;
    private CheckBox shuffleCb, muteCb, repeatCb;
    private View disconnectedOverlay;
    private Handler handler = new Handler();
    private PlaybackService playback;

    /* ugh, artwork related */
    private enum DisplayMode { Artwork, NoArtwork, Stopped }
    private View mainTrackMetadataWithAlbumArt, mainTrackMetadataNoAlbumArt;
    private ViewPropertyAnimator metadataAnim1, metadataAnim2;
    private AlbumArtModel albumArtModel = new AlbumArtModel();
    private ImageView albumArtImageView;

    static {
        REPEAT_TO_STRING_ID = new HashMap<>();
        REPEAT_TO_STRING_ID.put(RepeatMode.None, R.string.button_repeat_off);
        REPEAT_TO_STRING_ID.put(RepeatMode.List, R.string.button_repeat_list);
        REPEAT_TO_STRING_ID.put(RepeatMode.Track, R.string.button_repeat_track);
    }

    public static Intent getStartIntent(final Context context) {
        return new Intent(context, MainActivity.class)
            .addFlags(Intent.FLAG_ACTIVITY_SINGLE_TOP | Intent.FLAG_ACTIVITY_CLEAR_TOP);
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        this.prefs = this.getSharedPreferences("prefs", Context.MODE_PRIVATE);
        this.wss = getWebSocketService();
        this.playback = getPlaybackService();

        setContentView(R.layout.activity_main);
        bindEventListeners();

        if (!this.wss.hasValidConnection()) {
            startActivity(SettingsActivity.getStartIntent(this));
        }
    }

    @Override
    protected void onPause() {
        super.onPause();
        unbindCheckboxEventListeners();
    }

    @Override
    protected void onResume() {
        super.onResume();
        this.playback = getPlaybackService();
        bindCheckBoxEventListeners();
        rebindUi();
    }

    @Override
    protected void onRestoreInstanceState(Bundle savedInstanceState) {
        super.onRestoreInstanceState(savedInstanceState);
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        getMenuInflater().inflate(R.menu.main_menu, menu);
        return true;
    }

    @Override
    public boolean onPrepareOptionsMenu(Menu menu) {
        boolean connected = wss.getState() == WebSocketService.State.Connected;
        menu.findItem(R.id.action_playlists).setEnabled(connected);
        menu.findItem(R.id.action_genres).setEnabled(connected);
        return super.onPrepareOptionsMenu(menu);
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        switch (item.getItemId()) {
            case R.id.action_settings:
                startActivity(SettingsActivity.getStartIntent(this));
                return true;

            case R.id.action_genres:
                startActivity(CategoryBrowseActivity.getStartIntent(this, Messages.Category.GENRE));
                return true;

            case R.id.action_playlists:
                startActivity(CategoryBrowseActivity.getStartIntent(
                    this, Messages.Category.PLAYLISTS, CategoryBrowseActivity.DeepLink.TRACKS));
                return true;
        }

        return super.onOptionsItemSelected(item);
    }

    @Override
    protected WebSocketService.Client getWebSocketServiceClient() {
        return this.serviceClient;
    }

    @Override
    protected PlaybackService.EventListener getPlaybackServiceEventListener() {
        return this.playbackEvents;
    }

    private void bindCheckBoxEventListeners() {
        this.shuffleCb.setOnCheckedChangeListener(shuffleListener);
        this.muteCb.setOnCheckedChangeListener(muteListener);
        this.repeatCb.setOnCheckedChangeListener(repeatListener);
    }

    /* onRestoreInstanceState() calls setChecked(), which has the side effect of
    running these callbacks. this screws up state, especially for the repeat checkbox */
    private void unbindCheckboxEventListeners() {
        this.shuffleCb.setOnCheckedChangeListener(null);
        this.muteCb.setOnCheckedChangeListener(null);
        this.repeatCb.setOnCheckedChangeListener(null);
    }

    private void bindEventListeners() {
        this.title = (TextView) findViewById(R.id.track_title);
        this.artist = (TextView) findViewById(R.id.track_artist);
        this.album = (TextView) findViewById(R.id.track_album);
        this.volume = (TextView) findViewById(R.id.volume);

        this.titleWithArt = (TextView) findViewById(R.id.with_art_track_title);
        this.artistAndAlbumWithArt = (TextView) findViewById(R.id.with_art_artist_and_album);
        this.volumeWithArt = (TextView) findViewById(R.id.with_art_volume);

        this.playPause = (TextView) findViewById(R.id.button_play_pause);
        this.shuffleCb = (CheckBox) findViewById(R.id.check_shuffle);
        this.muteCb = (CheckBox) findViewById(R.id.check_mute);
        this.repeatCb = (CheckBox) findViewById(R.id.check_repeat);
        this.mainTrackMetadataWithAlbumArt = findViewById(R.id.main_track_metadata_with_art);
        this.mainTrackMetadataNoAlbumArt = findViewById(R.id.main_track_metadata_without_art);
        this.notPlayingOrDisconnected = (TextView) findViewById(R.id.main_not_playing);
        this.albumArtImageView = (ImageView) findViewById(R.id.album_art);
        this.connected = findViewById(R.id.connected);

        this.disconnectedOverlay = findViewById(R.id.disconnected_overlay);

        /* these will get faded in as appropriate */
        this.mainTrackMetadataNoAlbumArt.setAlpha(0.0f);
        this.mainTrackMetadataWithAlbumArt.setAlpha(0.0f);

        findViewById(R.id.button_prev).setOnClickListener((View view) -> playback.prev());

        final LongPressTextView seekBack = (LongPressTextView) findViewById(R.id.button_seek_back);
        seekBack.setOnTickListener((View view) -> playback.seekBackward());

        findViewById(R.id.button_play_pause).setOnClickListener((View view) -> {
            if (playback.getPlaybackState() == PlaybackState.Stopped) {
                playback.playAll();
            }
            else {
                playback.pauseOrResume();
            }
        });

        findViewById(R.id.button_next).setOnClickListener((View view) -> playback.next());

        final LongPressTextView seekForward = (LongPressTextView) findViewById(R.id.button_seek_forward);
        seekForward.setOnTickListener((View view) -> playback.seekForward());

        final LongPressTextView volumeUp = (LongPressTextView) findViewById(R.id.button_vol_up);
        volumeUp.setOnTickListener((View view) -> playback.volumeUp());

        final LongPressTextView volumeDown = (LongPressTextView) findViewById(R.id.button_vol_down);
        volumeDown.setOnTickListener((View view) -> playback.volumeDown());

        notPlayingOrDisconnected.setOnClickListener((view) -> {
            if (wss.getState() != WebSocketService.State.Connected) {
                wss.reconnect();
            }
        });

        findViewById(R.id.button_artists).setOnClickListener((View view) -> {
            startActivity(CategoryBrowseActivity.getStartIntent(this, Messages.Category.ALBUM_ARTIST));
        });

        findViewById(R.id.button_tracks).setOnClickListener((View view) -> {
            startActivity(TrackListActivity.getStartIntent(MainActivity.this));
        });

        findViewById(R.id.button_albums).setOnClickListener((View view) -> {
            startActivity(AlbumBrowseActivity.getStartIntent(MainActivity.this));
        });

        findViewById(R.id.button_play_queue).setOnClickListener((view) -> navigateToPlayQueue());

        findViewById(R.id.metadata_container).setOnClickListener((view) -> {
            if (playback.getQueueCount() > 0) {
                navigateToPlayQueue();
            }
        });

        disconnectedOverlay.setOnClickListener((view) -> {
            /* swallow input so user can't click on things while disconnected */
        });

        this.album.setOnClickListener((view) -> navigateToCurrentAlbum());
        this.artist.setOnClickListener((view) -> navigateToCurrentArtist());
    }

    private void rebindAlbumArtistWithArtTextView() {
        final String artist = playback.getTrackString(Metadata.Track.ARTIST, getString(R.string.unknown_artist));
        final String album = playback.getTrackString(Metadata.Track.ALBUM, getString(R.string.unknown_album));

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

    private void rebindUi() {
        if (this.playback == null) {
            throw new IllegalStateException();
        }

        /* state management for UI stuff is starting to get out of hand. we should
        refactor things pretty soon before they're completely out of control */

        final boolean streaming = prefs.getBoolean("streaming_playback", false);
        final WebSocketService.State state = wss.getState();

        final boolean connected = state == WebSocketService.State.Connected;

        final boolean playing = (playback.getPlaybackState() == PlaybackState.Playing);
        this.playPause.setText(playing ? R.string.button_pause : R.string.button_play);

        final boolean stopped = (playback.getPlaybackState() == PlaybackState.Stopped);
        notPlayingOrDisconnected.setVisibility(stopped ? View.VISIBLE : View.GONE);

        final boolean stateIsValidForArtwork = !stopped && connected && playback.getQueueCount() > 0;

        this.connected.setVisibility((connected && stopped) ? View.VISIBLE : View.GONE);
        this.disconnectedOverlay.setVisibility(connected ? View.GONE : View.VISIBLE);

        /* setup our state as if we have no album art -- because we don't know if we have any
        yet! the album art load process (if enabled) will ensure the correct metadata block
        is displayed in the correct location */
        if (!stateIsValidForArtwork) {
            setMetadataDisplayMode(DisplayMode.Stopped);
            notPlayingOrDisconnected.setText(connected ? R.string.transport_not_playing : R.string.status_disconnected);
            notPlayingOrDisconnected.setVisibility(View.VISIBLE);
        }
        else {
            notPlayingOrDisconnected.setVisibility(View.GONE);
        }

        final String artist = playback.getTrackString(Metadata.Track.ARTIST, "");
        final String album = playback.getTrackString(Metadata.Track.ALBUM, "");
        final String title = playback.getTrackString(Metadata.Track.TITLE, "");
        final String volume = getString(R.string.status_volume, Math.round(playback.getVolume() * 100));

        this.title.setText(Strings.empty(title) ? getString(R.string.unknown_title) : title);
        this.artist.setText(Strings.empty(artist) ? getString(R.string.unknown_artist) : artist);
        this.album.setText(Strings.empty(album) ? getString(R.string.unknown_album) : album);
        this.volume.setText(volume);

        this.rebindAlbumArtistWithArtTextView();
        this.titleWithArt.setText(Strings.empty(title) ? getString(R.string.unknown_title) : title);
        this.volumeWithArt.setText(volume);

        final RepeatMode repeatMode = playback.getRepeatMode();
        final boolean repeatChecked = (repeatMode != RepeatMode.None);
        repeatCb.setText(REPEAT_TO_STRING_ID.get(repeatMode));
        Views.setCheckWithoutEvent(repeatCb, repeatChecked, this.repeatListener);

        this.shuffleCb.setText(streaming ? R.string.button_random : R.string.button_shuffle);

        Views.setCheckWithoutEvent(this.shuffleCb, playback.isShuffled(), this.shuffleListener);
        Views.setCheckWithoutEvent(this.muteCb, playback.isMuted(), this.muteListener);

        boolean albumArtEnabledInSettings = this.prefs.getBoolean("album_art_enabled", true);

        if (stateIsValidForArtwork) {
            if (!albumArtEnabledInSettings || Strings.empty(artist) || Strings.empty(album)) {
                this.albumArtModel = new AlbumArtModel();
                setMetadataDisplayMode(DisplayMode.NoArtwork);
            }
            else {
                if (!this.albumArtModel.is(artist, album)) {
                    this.albumArtModel.destroy();
                    this.albumArtModel = new AlbumArtModel(title, artist, album, albumArtRetrieved);
                }

                updateAlbumArt();
            }
        }
    }

    private void clearUi() {
        albumArtModel = new AlbumArtModel();
        updateAlbumArt();
        rebindUi();
    }

    private void setMetadataDisplayMode(DisplayMode mode) {
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

    private void preloadNextImage() {
        final SocketMessage request = SocketMessage.Builder
                .request(Messages.Request.QueryPlayQueueTracks)
                .addOption(Messages.Key.OFFSET, this.playback.getQueuePosition() + 1)
                .addOption(Messages.Key.LIMIT, 1)
                .build();

        this.wss.send(request, this.getWebSocketServiceClient(), (response) -> {
            final JSONArray data = response.getJsonArrayOption(Messages.Key.DATA, new JSONArray());
            if (data.length() > 0) {
                JSONObject track = data.optJSONObject(0);
                final String artist = track.optString(Metadata.Track.ARTIST, "");
                final String album = track.optString(Metadata.Track.ALBUM, "");

                if (!albumArtModel.is(artist, album)) {
                    new AlbumArtModel("", artist, album, (info, url) -> {
                        int width = albumArtImageView.getWidth();
                        int height = albumArtImageView.getHeight();
                        Glide.with(MainActivity.this).load(url).downloadOnly(width, height);
                    }).fetch();
                }
            }
        });
    }

    private void updateAlbumArt() {
        if (playback.getPlaybackState() == PlaybackState.Stopped) {
            setMetadataDisplayMode(DisplayMode.NoArtwork);
        }

        final String url = albumArtModel.getUrl();

        if (Strings.empty(url)) {
            albumArtModel.fetch();
            setMetadataDisplayMode(DisplayMode.NoArtwork);
        }
        else {
            final int loadId = albumArtModel.getId();

            Glide.with(this)
                .load(url)
                .listener(new RequestListener<String, GlideDrawable>() {
                    @Override
                    public boolean onException(Exception e,
                                               String model,
                                               Target<GlideDrawable> target,
                                               boolean isFirstResource)
                    {
                        setMetadataDisplayMode(DisplayMode.NoArtwork);
                        return false;
                    }

                    @Override
                    public boolean onResourceReady(GlideDrawable resource,
                                                   String model,
                                                   Target<GlideDrawable> target,
                                                   boolean isFromMemoryCache,
                                                   boolean isFirstResource)
                    {
                        if (!isPaused()) {
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
    }

    private void navigateToCurrentArtist() {
        final long artistId = playback.getTrackLong(Metadata.Track.ARTIST_ID, -1);
        if (artistId != -1) {
            final String artistName = playback.getTrackString(Metadata.Track.ARTIST, "");
            startActivity(AlbumBrowseActivity.getStartIntent(
                MainActivity.this, Messages.Category.ARTIST, artistId, artistName));
        }
    }

    private void navigateToCurrentAlbum() {
        final long albumId = playback.getTrackLong(Metadata.Track.ALBUM_ID, -1);
        if (albumId != -1) {
            final String albumName = playback.getTrackString(Metadata.Track.ALBUM, "");
            startActivity(TrackListActivity.getStartIntent(
                MainActivity.this, Messages.Category.ALBUM, albumId, albumName));
        }
    }

    private void navigateToPlayQueue() {
        startActivity(PlayQueueActivity.getStartIntent(MainActivity.this, playback.getQueuePosition()));
    }

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

    private CheckBox.OnCheckedChangeListener muteListener =
        (CompoundButton compoundButton, boolean b) -> {
            if (b != playback.isMuted()) {
                playback.toggleMute();
            }
        };

    private CheckBox.OnCheckedChangeListener shuffleListener =
        (CompoundButton compoundButton, boolean b) -> {
            if (b != playback.isShuffled()) {
                playback.toggleShuffle();
            }
        };

    final CheckBox.OnCheckedChangeListener repeatListener = new CompoundButton.OnCheckedChangeListener() {
        @Override
        public void onCheckedChanged(CompoundButton compoundButton, boolean b) {
            final RepeatMode currentMode = playback.getRepeatMode();

            RepeatMode newMode = RepeatMode.None;

            if (currentMode == RepeatMode.None) {
                newMode = RepeatMode.List;
            }
            else if (currentMode == RepeatMode.List) {
                newMode = RepeatMode.Track;
            }

            final boolean checked = (newMode != RepeatMode.None);
            compoundButton.setText(REPEAT_TO_STRING_ID.get(newMode));
            Views.setCheckWithoutEvent(repeatCb, checked, this);

            playback.toggleRepeatMode();
        }
    };

    private PlaybackService.EventListener playbackEvents = new PlaybackService.EventListener() {
        @Override
        public void onStateUpdated() {
            rebindUi();
        }
    };

    private WebSocketService.Client serviceClient = new WebSocketService.Client() {
        @Override
        public void onStateChanged(WebSocketService.State newState, WebSocketService.State oldState) {
            if (newState == WebSocketService.State.Connected) {
                rebindUi();
            }
            else if (newState == WebSocketService.State.Disconnected) {
                clearUi();
            }
        }

        @Override
        public void onMessageReceived(SocketMessage message) {
        }

        @Override
        public void onInvalidPassword() {
            final String tag = InvalidPasswordDialogFragment.TAG;
            if (getSupportFragmentManager().findFragmentByTag(tag) == null) {
                InvalidPasswordDialogFragment
                    .newInstance().show(getSupportFragmentManager(), tag);
            }
        }
    };
}
