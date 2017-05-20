package io.casey.musikcube.remote;

import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.widget.CheckBox;
import android.widget.CompoundButton;
import android.widget.TextView;

import java.util.HashMap;
import java.util.Map;

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
import io.casey.musikcube.remote.ui.util.Views;
import io.casey.musikcube.remote.ui.view.LongPressTextView;
import io.casey.musikcube.remote.ui.view.MainMetadataView;
import io.casey.musikcube.remote.websocket.Messages;
import io.casey.musikcube.remote.websocket.Prefs;
import io.casey.musikcube.remote.websocket.SocketMessage;
import io.casey.musikcube.remote.websocket.WebSocketService;

public class MainActivity extends WebSocketActivityBase {
    private static Map<RepeatMode, Integer> REPEAT_TO_STRING_ID;

    private WebSocketService wss = null;

    private SharedPreferences prefs;
    private PlaybackService playback;

    private MainMetadataView metadataView;
    private TextView playPause;
    private View connectedNotPlaying, disconnectedButton;
    private CheckBox shuffleCb, muteCb, repeatCb;
    private View disconnectedOverlay;

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

        this.prefs = this.getSharedPreferences(Prefs.NAME, Context.MODE_PRIVATE);
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
        metadataView.onPause();
        unbindCheckboxEventListeners();
    }

    @Override
    protected void onResume() {
        super.onResume();
        this.playback = getPlaybackService();
        metadataView.onResume(playback);
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
        this.metadataView = (MainMetadataView) findViewById(R.id.main_metadata_view);
        this.shuffleCb = (CheckBox) findViewById(R.id.check_shuffle);
        this.muteCb = (CheckBox) findViewById(R.id.check_mute);
        this.repeatCb = (CheckBox) findViewById(R.id.check_repeat);
        this.connectedNotPlaying = findViewById(R.id.connected_not_playing);
        this.disconnectedButton = findViewById(R.id.disconnected);
        this.disconnectedOverlay = findViewById(R.id.disconnected_overlay);
        this.playPause = (TextView) findViewById(R.id.button_play_pause);

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

        disconnectedButton.setOnClickListener((view) -> {
            wss.reconnect();
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
    }

    private void rebindUi() {
        if (this.playback == null) {
            throw new IllegalStateException();
        }

        final boolean streaming = prefs.getBoolean(Prefs.Key.STREAMING_PLAYBACK, Prefs.Default.STREAMING_PLAYBACK);
        final boolean connected = (wss.getState() == WebSocketService.State.Connected);
        final boolean stopped = (playback.getPlaybackState() == PlaybackState.Stopped);
        final boolean playing = (playback.getPlaybackState() == PlaybackState.Playing);
        final boolean showMetadataView = !stopped && connected && playback.getQueueCount() > 0;

        /* bottom section: transport controls */
        this.playPause.setText(playing ? R.string.button_pause : R.string.button_play);

        this.connectedNotPlaying.setVisibility((connected && stopped) ? View.VISIBLE : View.GONE);
        this.disconnectedOverlay.setVisibility(connected ? View.GONE : View.VISIBLE);

        final RepeatMode repeatMode = playback.getRepeatMode();
        final boolean repeatChecked = (repeatMode != RepeatMode.None);
        repeatCb.setText(REPEAT_TO_STRING_ID.get(repeatMode));
        Views.setCheckWithoutEvent(repeatCb, repeatChecked, this.repeatListener);

        this.shuffleCb.setText(streaming ? R.string.button_random : R.string.button_shuffle);
        Views.setCheckWithoutEvent(this.shuffleCb, playback.isShuffled(), this.shuffleListener);

        Views.setCheckWithoutEvent(this.muteCb, playback.isMuted(), this.muteListener);

        /* middle section: connected, disconnected, and metadata views */
        connectedNotPlaying.setVisibility(View.GONE);
        disconnectedButton.setVisibility(View.GONE);

        if (!showMetadataView) {
            metadataView.hide();

            if (!connected) {
                disconnectedButton.setVisibility(View.VISIBLE);
            }
            else if (stopped) {
                connectedNotPlaying.setVisibility(View.VISIBLE);
            }
        }
        else {
            metadataView.refresh();
        }
    }

    private void clearUi() {
        metadataView.clear();
        rebindUi();
    }

    private void navigateToPlayQueue() {
        startActivity(PlayQueueActivity.getStartIntent(MainActivity.this, playback.getQueuePosition()));
    }

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

    private PlaybackService.EventListener playbackEvents = () -> rebindUi();

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
